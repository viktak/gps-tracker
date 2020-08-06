#define __debugSettings
#define FORMAT_SPIFFS_IF_FAILED true
#define ARDUINOJSON_USE_LONG_LONG 1
#define MQTT_MAX_PACKET_SIZE 2048           //  This needs to be in PubSubClient.h!!!!

#include <includes.h>

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, SerialMon);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif

//  Preferences/Settings
Preferences prefs;
Settings appSettings;


//  GSM modem
const char apn[]  = GSM_APN_NAME;
const char gprsUser[] = GPRS_USER;
const char gprsPass[] = GPRS_PASSWORD;
u_char unsuccessfulGSMAttempts = 0;


// Server details
const char server[] = "vsh.pp.ua";
const char resource[] = "/TinyGSM/logo.txt";
const int  port = 80;


TinyGsmClient client(modem);
HttpClient http(client, server, port);


//  Web server
WebServer webServer(80);
bool isAccessPoint = false;
bool isAccessPointCreated = false;


//  GPS
HardwareSerial sGPS(SerialGPS);
TinyGPSPlus gps;
unsigned long locationLastLoggedToSDCard = 0;
unsigned long locationLastLoggedToMQTT = 0;


//  MQTT
WiFiClient wclient;
PubSubClient PSclient(client);


//  I2C
PCF857x ledPanel(LED_PANEL_ADDRESS, &Wire);


//  SD card
File myGPSLogFile;


//  Time
// Daylight savings time rules for Greece
TimeChangeRule myDST = {"MDT", Fourth, Sun, Mar, 2, DST_TIMEZONE_OFFSET * 60};
TimeChangeRule mySTD = {"MST", Fourth,  Sun, Oct, 2,  ST_TIMEZONE_OFFSET * 60};
Timezone myTZ(myDST, mySTD);
TimeChangeRule *tcr;        // Pointer to the time change rule
bool needsTime = true;
bool needsHeartbeat = false;


//  Timers
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

hw_timer_t *heartbeatTimer;
volatile SemaphoreHandle_t heartbeatTimerSemaphore;


//  Misc variables
OPERATION_MODES operationMode = OPERATION_MODES::DATA_LOGGING;
enum CONNECTION_STATE connectionState;
bool ntpInitialized = false;




//////////////////////////////////////////////////////////////////
/////   Misc helper functions
//////////////////////////////////////////////////////////////////

String GetRegistrationStatusName(RegStatus rs){
  switch (rs)
  {
  case -1: return "REG_NO_RESULT";
  case  0: return "REG_UNREGISTERED";
  case  1: return "REG_OK_HOME";
  case  2: return "REG_SEARCHING";
  case  3: return "REG_DENIED";
  case  4: return "REG_UNKNOWN";
  case  5: return "REG_OK_ROAMING";
  default: return "REALLY UNKNOWN";
  }
}

const char* GetResetReasonString(RESET_REASON reason){
  switch (reason){
    case 1  : return ("Vbat power on reset");break;
    case 3  : return ("Software reset digital core");break;
    case 4  : return ("Legacy watch dog reset digital core");break;
    case 5  : return ("Deep Sleep reset digital core");break;
    case 6  : return ("Reset by SLC module, reset digital core");break;
    case 7  : return ("Timer Group0 Watch dog reset digital core");break;
    case 8  : return ("Timer Group1 Watch dog reset digital core");break;
    case 9  : return ("RTC Watch dog Reset digital core");break;
    case 10 : return ("Instrusion tested to reset CPU");break;
    case 11 : return ("Time Group reset CPU");break;
    case 12 : return ("Software reset CPU");break;
    case 13 : return ("RTC Watch dog Reset CPU");break;
    case 14 : return ("for APP CPU, reset by PRO CPU");break;
    case 15 : return ("Reset when the vdd voltage is not stable");break;
    case 16 : return ("RTC Watch dog reset digital core and rtc module");break;
    default : return ("NO_MEAN");
  }
}

String TimeIntervalToString(time_t time){

  String myTime = "";
  char s[2];

  //  hours
  itoa((time/3600), s, DEC);
  myTime+= s;
  myTime+=":";

  //  minutes
  if(minute(time) <10)
    myTime+="0";

  itoa(minute(time), s, DEC);
  myTime+= s;
  myTime+=":";

  //  seconds
  if(second(time) <10)
    myTime+="0";

  itoa(second(time), s, DEC);
  myTime+= s;
  return myTime;
}

const char*  GetOperationalMode(OPERATION_MODES mode){
  switch (mode)
  {
  case OPERATION_MODES::DATA_LOGGING:
    return "Data logging";
    break;
  
  case OPERATION_MODES::WIFI_SETUP:
    return "WiFi setup";
    break;

  default:
    break;
  }
  return NULL;
}

//  !!! For "strftime" to work delete Time.h file in TimeLib library  !!!
char* GetFullDateTime(const char *formattingString, size_t size){
    time_t localTime = myTZ.toLocal(now(), &tcr);
    struct tm * now = localtime( &localTime );
    char* buf = new char[20];
    strftime (buf, 20, formattingString, now);
    return buf;
}

bool IsTimeValid(){
  SerialMon.println(gps.date.age());
  if (gps.time.isValid() && (gps.date.age() < 1000))
    return true;
  return false;
}

time_t GetTimeSinceEpoch(){
    struct tm t = {0};  // Initalize to all 0's
    t.tm_year = year(now()) - 1900;
    t.tm_mon = month(now()) - 1;
    t.tm_mday = day(now());
    t.tm_hour = hour(now());
    t.tm_min = minute(now());
    t.tm_sec = second(now());
    return mktime(&t);

}

void PrintSettings(){
  SerialMon.println("==========================App settings==========================");
  SerialMon.printf("App name\t\t%s\r\nAdmin password\t\t%s\r\nSSID\t\t\t%s\r\nPassword\t\t%s\r\nAP SSID\t\t\t%s\r\nAP Password\t\t%s\r\nTimezone\t\t%i\r\nMQTT Server\t\t%s\r\nMQTT Port\t\t%u\r\nMQTT TOPIC\t\t%s\r\nLog2SDCard interval\t%i\r\nLog2Server interval\t%i\r\nMax GSM attempts\t%i\r\nHearbeat interval\t%u\r\n", 
    appSettings.friendlyName, appSettings.adminPassword, appSettings.ssid, appSettings.password, appSettings.AccessPointSSID, appSettings.AccessPointPassword,
    appSettings.timeZone, appSettings.mqttServer, appSettings.mqttPort, appSettings.mqttTopic, appSettings.logToSDCardInterval,
    appSettings.logToMQTTServerInterval, appSettings.maxUnsuccessfulGSMAttempts, appSettings.heartbeatInterval);
  SerialMon.println("====================================================================");
}

void SaveSettings(){
  prefs.begin("gps-tracker", false);

  prefs.putString("APP_NAME", appSettings.friendlyName);
  prefs.putString("SSID", appSettings.ssid);
  prefs.putString("PASSWORD", appSettings.password);
  prefs.putString("ADMIN_PASSWORD", appSettings.adminPassword);
  prefs.putString("AP_SSID", appSettings.AccessPointSSID);
  prefs.putString("AP_PASSWORD", appSettings.AccessPointPassword);
  prefs.putChar("TIMEZONE", appSettings.timeZone);
  prefs.putString("MQTT_SERVER", appSettings.mqttServer);
  prefs.putUShort("MQTT_PORT", appSettings.mqttPort);
  prefs.putString("MQTT_TOPIC", appSettings.mqttTopic);
  prefs.putUInt("LOG_TO_SDC_INT", appSettings.logToSDCardInterval);
  prefs.putUInt("LOG_2_MQTT_INT", appSettings.logToMQTTServerInterval);
  prefs.putUChar("MAX_GSM_ATTEMPT", appSettings.maxUnsuccessfulGSMAttempts);
  prefs.putUInt("HEARTBEAT_INTL", appSettings.heartbeatInterval);
  prefs.end();
  delay(10);
}

void LoadSettings(bool LoadDefaults = false){

  prefs.begin("gps-tracker", false);
  
  if ( LoadDefaults )
    prefs.clear();  //  clear all settings

  strcpy(appSettings.friendlyName, (prefs.getString("APP_NAME", DEFAULT_APP_FRIENDLY_NAME).c_str()));
  strcpy(appSettings.ssid, (prefs.getString("SSID", " ").c_str()));
  strcpy(appSettings.password, (prefs.getString("PASSWORD", " ").c_str()));
  strcpy(appSettings.adminPassword, (prefs.getString("ADMIN_PASSWORD", DEFAULT_ADMIN_PASSWORD).c_str()));
  appSettings.timeZone = prefs.getChar("TIMEZONE", DEFAULT_TIMEZONE);
  strcpy(appSettings.mqttServer, (prefs.getString("MQTT_SERVER", DEFAULT_MQTT_SERVER).c_str()));
  appSettings.mqttPort = prefs.getUShort("MQTT_PORT", DEFAULT_MQTT_PORT);
  strcpy(appSettings.mqttTopic, (prefs.getString("MQTT_TOPIC").c_str()));
  strcpy(appSettings.AccessPointSSID, (prefs.getString("AP_SSID").c_str()));
  strcpy(appSettings.AccessPointPassword, (prefs.getString("AP_PASSWORD", DEFAULT_AP_PASSWORD).c_str()));
  appSettings.logToSDCardInterval = prefs.getUInt("LOG_TO_SDC_INT", DEFAULT_LOG_TO_SD_CARD_INTERVAL);
  appSettings.logToMQTTServerInterval = prefs.getUInt("LOG_2_MQTT_INT", DEFAULT_LOG_TO_MQTT_SERVER_INTERVAL);
  appSettings.maxUnsuccessfulGSMAttempts = prefs.getUChar("MAX_GSM_ATTEMPT", DEFAULT_MAX_UNSUCCESSFUL_GSM_ATTEMPTS);
  appSettings.heartbeatInterval = prefs.getUInt("HEARTBEAT_INTL", DEFAULT_HEARTBEAT_INTERVAL);
  prefs.end();

  if (strcmp(appSettings.AccessPointSSID,"") == 0){
    char c[ESP_ACCESS_POINT_NAME_SIZE];
    uint64_t chipid = ESP.getEfuseMac();

    sprintf(c, "%s-%04X%08X", DEFAULT_AP_SSID, (uint16_t)(chipid>>32), (uint32_t)chipid);
    strcpy(appSettings.AccessPointSSID, c);
  }

  if (strcmp(appSettings.mqttTopic,"") == 0){
    char c[48];
    uint64_t chipid = ESP.getEfuseMac();

    sprintf(c, "%s-%04X%08X", DEFAULT_MQTT_TOPIC, (uint16_t)(chipid>>32), (uint32_t)chipid);
    strcpy(appSettings.mqttTopic, c);
  }
  
  //  Need to save settings just in case something was reset to default
  SaveSettings();

}

bool SetSystemTimeFromGPS(){
  if ( IsTimeValid() ){

    struct tm tm = {0};
    tm.tm_year = gps.date.year();
    tm.tm_mon = gps.date.month();
    tm.tm_mday = gps.date.day();
    tm.tm_hour = gps.time.hour();
    tm.tm_min = gps.time.minute();
    tm.tm_sec = gps.time.second();

    setTime(tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_mday, tm.tm_mon, tm.tm_year);
    SerialMon.println("System time set from GPS.");
    return true;
  }
  SerialMon.println("System time could not be set from GPS.");
  return false;
}

String DateTimeToString(time_t time){

  String myTime = "";
  char s[2];

  //  years
  itoa(year(time), s, DEC);
  myTime+= s;
  myTime+="-";


  //  months
  itoa(month(time), s, DEC);
  myTime+= s;
  myTime+="-";

  //  days
  itoa(day(time), s, DEC);
  myTime+= s;

  myTime+=" ";

  //  hours
  itoa(hour(time), s, DEC);
  myTime+= s;
  myTime+=":";

  //  minutes
  if(minute(time) <10)
    myTime+="0";

  itoa(minute(time), s, DEC);
  myTime+= s;
  myTime+=":";

  //  seconds
  if(second(time) <10)
    myTime+="0";

  itoa(second(time), s, DEC);
  myTime+= s;

  return myTime;
}




//////////////////////////////////////////////////////////////////
/////   Interrupt handlers
//////////////////////////////////////////////////////////////////

void IRAM_ATTR heartbeatTimerCallback() {
  portENTER_CRITICAL_ISR(&timerMux);
  needsHeartbeat = true;
  portEXIT_CRITICAL_ISR(&timerMux);
  xSemaphoreGiveFromISR(heartbeatTimerSemaphore, NULL);
}




//////////////////////////////////////////////////////////////////
/////   Web server
//////////////////////////////////////////////////////////////////

bool is_authenticated(){
  if (webServer.hasHeader("Cookie")){
    String cookie = webServer.header("Cookie");
    if (cookie.indexOf("EspAuth=1") != -1) {
      return true;
    }
  }
  return false;
}

void handleLogin(){
  String msg = "";
  if (webServer.hasHeader("Cookie")){
    String cookie = webServer.header("Cookie");
  }
  if (webServer.hasArg("DISCONNECT")){
    String header = "HTTP/1.1 301 OK\r\nSet-Cookie: EspAuth=0\r\nLocation: /login.html\r\nCache-Control: no-cache\r\n\r\n";
    webServer.sendContent(header);
    return;
  }
  if (webServer.hasArg("username") && webServer.hasArg("password")){
    if (webServer.arg("username") == DEFAULT_ADMIN_USERNAME &&  webServer.arg("password") == appSettings.adminPassword ){
      String header = "HTTP/1.1 301 OK\r\nSet-Cookie: EspAuth=1\r\nLocation: /status.html\r\nCache-Control: no-cache\r\n\r\n";
      webServer.sendContent(header);
      return;
    }
    msg = "<div class=\"alert alert-danger\"><strong>Error!</strong> Wrong user name and/or password specified.<a href=\"#\" class=\"close\" data-dismiss=\"alert\" aria-label=\"close\">&times;</a></div>";
  }

  File f = SPIFFS.open("/pageheader.html", "r");
  String headerString;
  if (f.available()) headerString = f.readString();
  f.close();

  time_t localTime = myTZ.toLocal(now(), &tcr);

  f = SPIFFS.open("/login.html", "r");

  String s, htmlString;

  while (f.available()){
    s = f.readStringUntil('\n');

    if (s.indexOf("%pageheader%")>-1) s.replace("%pageheader%", headerString);
    if (s.indexOf("%year%")>-1) s.replace("%year%", (String)year(localTime));
    if (s.indexOf("%alert%")>-1) s.replace("%alert%", msg);

    htmlString+=s;
  }
  f.close();
  webServer.send(200, "text/html", htmlString);
}

void handleRoot() {

  if (!is_authenticated()){
    String header = "HTTP/1.1 301 OK\r\nLocation: /login.html\r\nCache-Control: no-cache\r\n\r\n";
    webServer.sendContent(header);
    return;
  }

  File f = SPIFFS.open("/pageheader.html", "r");
  String headerString;
  if (f.available()) headerString = f.readString();
  f.close();

  time_t localTime = myTZ.toLocal(now(), &tcr);

  f = SPIFFS.open("/index.html", "r");

  String FirmwareVersionString = String(FIRMWARE_VERSION) + " @ " + String(__TIME__) + " - " + String(__DATE__);

  String s, htmlString;

  while (f.available()){
    s = f.readStringUntil('\n');

    if (s.indexOf("%pageheader%")>-1) s.replace("%pageheader%", headerString);
    if (s.indexOf("%year%")>-1) s.replace("%year%", (String)year(localTime));
    if (s.indexOf("%chipid%")>-1) s.replace("%chipid%", (String)(uint32_t)ESP.getEfuseMac());
    if (s.indexOf("%hardwareid%")>-1) s.replace("%hardwareid%", HARDWARE_ID);
    if (s.indexOf("%hardwareversion%")>-1) s.replace("%hardwareversion%", HARDWARE_VERSION);
    if (s.indexOf("%softwareid%")>-1) s.replace("%softwareid%", SOFTWARE_ID);
    if (s.indexOf("%firmwareversion%")>-1) s.replace("%firmwareversion%", FirmwareVersionString);

    htmlString+=s;
  }
  f.close();
  webServer.send(200, "text/html", htmlString);
}

void handleStatus() {

  if (!is_authenticated()){
     String header = "HTTP/1.1 301 OK\r\nLocation: /login.html\r\nCache-Control: no-cache\r\n\r\n";
     webServer.sendContent(header);
     return;
  }

  File f = SPIFFS.open("/pageheader.html", "r");
  String headerString;
  if (f.available()) headerString = f.readString();
  f.close();

  char c[ESP_ACCESS_POINT_NAME_SIZE];
  uint64_t chipid = ESP.getEfuseMac();

  sprintf(c, "ESP-%04X%08X", (uint16_t)(chipid>>32), (uint32_t)chipid);
  strcpy(appSettings.AccessPointSSID, c);


  time_t localTime = myTZ.toLocal(now(), &tcr);

  String s;

  f = SPIFFS.open("/status.html", "r");

  String htmlString, ds18b20list;
  String FirmwareVersionString = String(FIRMWARE_VERSION) + " @ " + String(__TIME__) + " - " + String(__DATE__);

  char battVoltage[10];
  sprintf(battVoltage, "%1.3fV", float(modem.getBattVoltage())/float(1000));
  
  char battPercent[10];
  sprintf(battPercent, "%u%%", u_char(modem.getBattPercent()));

  while (f.available()){
    s = f.readStringUntil('\n');

    //  System information
    if (s.indexOf("%pageheader%")>-1) s.replace("%pageheader%", headerString);
    if (s.indexOf("%year%")>-1) s.replace("%year%", (String)year(localTime));
    if (s.indexOf("%espid%")>-1) s.replace("%espid%", c);
    if (s.indexOf("%hardwareid%")>-1) s.replace("%hardwareid%", HARDWARE_ID);
    if (s.indexOf("%hardwareversion%")>-1) s.replace("%hardwareversion%", HARDWARE_VERSION);
    if (s.indexOf("%softwareid%")>-1) s.replace("%softwareid%", SOFTWARE_ID);
    if (s.indexOf("%firmwareversion%")>-1) s.replace("%firmwareversion%", FirmwareVersionString);
    if (s.indexOf("%chipid%")>-1) s.replace("%chipid%", c);
    if (s.indexOf("%uptime%")>-1) s.replace("%uptime%", TimeIntervalToString(millis()/1000));
    if (s.indexOf("%currenttime%")>-1) s.replace("%currenttime%", DateTimeToString(localTime));
    if (s.indexOf("%lastresetreason0%")>-1) s.replace("%lastresetreason0%", GetResetReasonString(rtc_get_reset_reason(0)));
    if (s.indexOf("%lastresetreason1%")>-1) s.replace("%lastresetreason1%", GetResetReasonString(rtc_get_reset_reason(1)));
    if (s.indexOf("%flashchipsize%")>-1) s.replace("%flashchipsize%",String(ESP.getFlashChipSize()));
    if (s.indexOf("%flashchipspeed%")>-1) s.replace("%flashchipspeed%",String(ESP.getFlashChipSpeed()));
    if (s.indexOf("%freeheapsize%")>-1) s.replace("%freeheapsize%",String(ESP.getFreeHeap()));
    if (s.indexOf("%freesketchspace%")>-1) s.replace("%freesketchspace%",String(ESP.getFreeSketchSpace()));
    if (s.indexOf("%friendlyname%")>-1) s.replace("%friendlyname%",appSettings.friendlyName);
    if (s.indexOf("%mqtt-topic%")>-1) s.replace("%mqtt-topic%",appSettings.mqttTopic);

    //  Network settings
    switch (WiFi.getMode()) {
      case WIFI_AP:
        if (s.indexOf("%wifimode%")>-1) s.replace("%wifimode%", "Access Point");
        if (s.indexOf("%macaddress%")>-1) s.replace("%macaddress%",String(WiFi.softAPmacAddress()));
        if (s.indexOf("%networkaddress%")>-1) s.replace("%networkaddress%",WiFi.softAPIP().toString());
        if (s.indexOf("%ssid%")>-1) s.replace("%ssid%",String(WiFi.SSID()));
        if (s.indexOf("%subnetmask%")>-1) s.replace("%subnetmask%","n/a");
        if (s.indexOf("%gateway%")>-1) s.replace("%gateway%","n/a");
        break;
      case WIFI_STA:
        if (s.indexOf("%wifimode%")>-1) s.replace("%wifimode%", "Station");
        if (s.indexOf("%macaddress%")>-1) s.replace("%macaddress%",String(WiFi.macAddress()));
        if (s.indexOf("%networkaddress%")>-1) s.replace("%networkaddress%",WiFi.localIP().toString());
        if (s.indexOf("%ssid%")>-1) s.replace("%ssid%",String(WiFi.SSID()));
        if (s.indexOf("%subnetmask%")>-1) s.replace("%subnetmask%",WiFi.subnetMask().toString());
        if (s.indexOf("%gateway%")>-1) s.replace("%gateway%",WiFi.gatewayIP().toString());
        break;
      default:
        //  This should not happen...
        break;
    }

    //  Modem settings
    if (s.indexOf("%modemname%")>-1) s.replace("%modemname%", modem.getModemName().c_str());
    if (s.indexOf("%modeminfo%")>-1) s.replace("%modeminfo%", modem.getModemInfo().c_str());
    if (s.indexOf("%imei%")>-1) s.replace("%imei%", modem.getIMEI().c_str());
    if (s.indexOf("%imsi%")>-1) s.replace("%imsi%", modem.getIMSI().c_str());
    if (s.indexOf("%ccid%")>-1) s.replace("%ccid%", modem.getSimCCID().c_str());
    if (s.indexOf("%simstatus%")>-1) s.replace("%simstatus%", String(modem.getSimStatus()).c_str());
    if (s.indexOf("%batteryvoltage%")>-1) s.replace("%batteryvoltage%", battVoltage);
    if (s.indexOf("%batterypercentage%")>-1) s.replace("%batterypercentage%", battPercent);
    if (s.indexOf("%registrationstatus%")>-1) s.replace("%registrationstatus%", GetRegistrationStatusName(modem.getRegistrationStatus()).c_str());
    if (s.indexOf("%operator%")>-1) s.replace("%operator%", modem.getOperator().c_str());
    if (s.indexOf("%signalquality%")>-1) s.replace("%signalquality%", String(modem.getSignalQuality()).c_str());
    if (s.indexOf("%isnetworkconnected%")>-1) s.replace("%isnetworkconnected%", modem.isNetworkConnected()?"Connected":"Not connected");
    if (s.indexOf("%isgprsconnected%")>-1) s.replace("%isgprsconnected%", modem.isGprsConnected()?"Connected":"Not connected");
    if (s.indexOf("%gsmipaddress%")>-1) s.replace("%gsmipaddress%", modem.getLocalIP().c_str());

      htmlString+=s;
    }
    f.close();
  webServer.send(200, "text/html", htmlString);
}

void handleNetworkSettings() {

  if (!is_authenticated()){
     String header = "HTTP/1.1 301 OK\r\nLocation: /login.html\r\nCache-Control: no-cache\r\n\r\n";
     webServer.sendContent(header);
     return;
   }

  if (webServer.method() == HTTP_POST){  //  POST
    if (webServer.hasArg("ssid")){
      strcpy(appSettings.ssid, webServer.arg("ssid").c_str());
      strcpy(appSettings.password, webServer.arg("password").c_str());
      SaveSettings();
      ESP.restart();
    }
  }

  File f = SPIFFS.open("/pageheader.html", "r");

  String headerString;

  if (f.available()) headerString = f.readString();
  f.close();

  time_t localTime = myTZ.toLocal(now(), &tcr);

  f = SPIFFS.open("/networksettings.html", "r");
  String s, htmlString, wifiList;

  byte numberOfNetworks = WiFi.scanNetworks();
  for (size_t i = 0; i < numberOfNetworks; i++) {
    wifiList+="<div class=\"radio\"><label><input ";
    if (i==0) wifiList+="id=\"ssid\" ";

    wifiList+="type=\"radio\" name=\"ssid\" value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + "</label></div>";
  }

  while (f.available()){
    s = f.readStringUntil('\n');

    if (s.indexOf("%pageheader%")>-1) s.replace("%pageheader%", headerString);
    if (s.indexOf("%year%")>-1) s.replace("%year%", (String)year(localTime));
    if (s.indexOf("%wifilist%")>-1) s.replace("%wifilist%", wifiList);
      htmlString+=s;
    }
    f.close();
  webServer.send(200, "text/html", htmlString);

}

void handleTools() {

  if (!is_authenticated()){
     String header = "HTTP/1.1 301 OK\r\nLocation: /login.html\r\nCache-Control: no-cache\r\n\r\n";
     webServer.sendContent(header);
     return;
   }

  if (webServer.method() == HTTP_POST){  //  POST

    if (webServer.hasArg("reset")){
      LoadSettings(true);
      ESP.restart();
    }

    if (webServer.hasArg("restart")){
      ESP.restart();
    }
  }

  File f = SPIFFS.open("/pageheader.html", "r");
  String headerString;
  if (f.available()) headerString = f.readString();
  f.close();

  f = SPIFFS.open("/tools.html", "r");

  String s, htmlString;

  while (f.available()){
    s = f.readStringUntil('\n');

    if (s.indexOf("%pageheader%")>-1) s.replace("%pageheader%", headerString);
      htmlString+=s;
    }
    f.close();
  webServer.send(200, "text/html", htmlString);

}

void handleGeneralSettings() {

  if (!is_authenticated()){
     String header = "HTTP/1.1 301 OK\r\nLocation: /login.html\r\nCache-Control: no-cache\r\n\r\n";
     webServer.sendContent(header);
     return;
   }

  if (webServer.method() == HTTP_POST){  //  POST

    SerialMon.println("================= Submitted data =================");
    for (size_t i = 0; i < webServer.args(); i++)
      SerialMon.printf("%s: %s\r\n", webServer.argName(i).c_str(), webServer.arg(i).c_str());
    SerialMon.println("==================================================");

    //  System settings
    if (webServer.hasArg("friendlyname"))
      strcpy(appSettings.friendlyName, webServer.arg("friendlyname").c_str());

    if (webServer.hasArg("heartbeatinterval"))
      appSettings.heartbeatInterval = atoi(webServer.arg("heartbeatinterval").c_str());
    
    if (webServer.hasArg("log2sdcardinterval"))
      appSettings.logToSDCardInterval = atoi(webServer.arg("log2sdcardinterval").c_str());

    if (webServer.hasArg("log2mqttinterval"))
      appSettings.logToMQTTServerInterval = atoi(webServer.arg("log2mqttinterval").c_str());

    if (webServer.hasArg("maxfailedgsmattempts"))
      appSettings.maxUnsuccessfulGSMAttempts = atoi(webServer.arg("maxfailedgsmattempts").c_str());

    if (webServer.hasArg("timezoneselector")){
      appSettings.timeZone = atoi(webServer.arg("timezoneselector").c_str());
    }


    //  MQTT settings
    if (webServer.hasArg("mqttbroker")){
      sprintf(appSettings.mqttServer, "%s", webServer.arg("mqttbroker").c_str());
    }

    if (webServer.hasArg("mqttport")){
      appSettings.mqttPort = atoi(webServer.arg("mqttport").c_str());
    }

    if (webServer.hasArg("mqtttopic")){
        sprintf(appSettings.mqttTopic, "%s", webServer.arg("mqtttopic").c_str());
    }


    //  Access point
    if (webServer.hasArg("accesspointssid")){
      sprintf(appSettings.AccessPointSSID, "%s", webServer.arg("accesspointssid").c_str());
    }

    if ((webServer.arg("accesspointpassword").length() > 0) && webServer.hasArg("accesspointpassword") && webServer.hasArg("confirmaccesspointpassword"))
      if (webServer.arg("accesspointpassword") == webServer.arg("confirmaccesspointpassword"))
        sprintf(appSettings.AccessPointPassword, "%s", webServer.arg("accesspointpassword").c_str());
    

    //  Administrative settings
    if ((webServer.arg("deviceadminpassword").length() > 0) && webServer.hasArg("deviceadminpassword") && webServer.hasArg("confirmdeviceadminpassword"))
      if (webServer.arg("deviceadminpassword") == webServer.arg("confirmdeviceadminpassword"))
        sprintf(appSettings.adminPassword, "%s", webServer.arg("deviceadminpassword").c_str());
    

    SaveSettings();
    ESP.restart();
  }

  File f = SPIFFS.open("/pageheader.html", "r");
  String headerString;
  if (f.available()) headerString = f.readString();
  f.close();

  f = SPIFFS.open("/generalsettings.html", "r");

  String s, htmlString, timezoneslist = "";

  char ss[4];

  for (signed char i = -12; i < 15; i++) {
    itoa(i, ss, DEC);
    timezoneslist+="<option ";
    if (appSettings.timeZone == i){
      timezoneslist+= "selected ";
    }
    timezoneslist+= "value=\"";
    timezoneslist+=ss;
    timezoneslist+="\">UTC ";
    if (i>0){
      timezoneslist+="+";
    }
    if (i!=0){
      timezoneslist+=ss;
      timezoneslist+=":00";
    }
    timezoneslist+="</option>";
    timezoneslist+="\n";
  }

  while (f.available()){
    s = f.readStringUntil('\n');

    if (s.indexOf("%pageheader%")>-1) s.replace("%pageheader%", headerString);
    if (s.indexOf("%mqtt-servername%")>-1) s.replace("%mqtt-servername%", appSettings.mqttServer);
    if (s.indexOf("%mqtt-port%")>-1) s.replace("%mqtt-port%", String(appSettings.mqttPort));
    if (s.indexOf("%mqtt-topic%")>-1) s.replace("%mqtt-topic%", appSettings.mqttTopic);
    if (s.indexOf("%timezoneslist%")>-1) s.replace("%timezoneslist%", timezoneslist);
    if (s.indexOf("%friendlyname%")>-1) s.replace("%friendlyname%", appSettings.friendlyName);
    if (s.indexOf("%heartbeatinterval%")>-1) s.replace("%heartbeatinterval%", (String)appSettings.heartbeatInterval);
    if (s.indexOf("%log2sdcardinterval%")>-1) s.replace("%log2sdcardinterval%", (String)appSettings.logToSDCardInterval);
    if (s.indexOf("%log2mqttinterval%")>-1) s.replace("%log2mqttinterval%", (String)appSettings.logToMQTTServerInterval);
    if (s.indexOf("%maxfailedgsmattempts%")>-1) s.replace("%maxfailedgsmattempts%", (String)appSettings.maxUnsuccessfulGSMAttempts);
    if (s.indexOf("%accesspointssid%")>-1) s.replace("%accesspointssid%", (String)appSettings.AccessPointSSID);
    if (s.indexOf("%accesspointpassword%")>-1) s.replace("%accesspointpassword%", "");
    if (s.indexOf("%confirmaccesspointpassword%")>-1) s.replace("%confirmaccesspointpassword%", "");
    if (s.indexOf("%deviceadminpassword%")>-1) s.replace("%deviceadminpassword%", "");
    if (s.indexOf("%confirmdeviceadminpassword%")>-1) s.replace("%confirmdeviceadminpassword%", "");
    if (s.indexOf("%deviceadmin%")>-1) s.replace("%deviceadmin%", "admin");

    htmlString+=s;
  }
  f.close();
  webServer.send(200, "text/html", htmlString);
}

void handleNotFound(){
  if (!is_authenticated()){
     String header = "HTTP/1.1 301 OK\r\nLocation: /login.html\r\nCache-Control: no-cache\r\n\r\n";
     webServer.sendContent(header);
     return;
   }

  File f = SPIFFS.open("/pageheader.html", "r");
  String headerString;
  if (f.available()) headerString = f.readString();
  f.close();

  f = SPIFFS.open("/badrequest.html", "r");

  String s, htmlString;

  while (f.available()){
    s = f.readStringUntil('\n');

    if (s.indexOf("%pageheader%")>-1) s.replace("%pageheader%", headerString);
      htmlString+=s;
    }
    f.close();
  webServer.send(200, "text/html", htmlString);
}

void InitWifiWebServer(){
  //  Web server
  if (MDNS.begin("esp32")) {
    SerialMon.println("MDNS responder started.");
  }

  //  Page handles
  webServer.on("/", handleStatus);
  webServer.on("/login.html", handleLogin);
  webServer.on("/status.html", handleStatus);
  webServer.on("/networksettings.html", handleNetworkSettings);
  webServer.on("/tools.html", handleTools);
  webServer.on("/generalsettings.html", handleGeneralSettings);

  webServer.onNotFound(handleNotFound);


  /*handling uploading firmware file */
  webServer.on("/update", HTTP_POST, []() {
    webServer.sendHeader("Connection", "close");
    webServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  }, []() {
    HTTPUpload& upload = webServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
      SerialMon.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        SerialMon.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        ESP.restart();
      } else {
        Update.printError(Serial);
      }
    }
    yield();
  });

  //  Start HTTP (web) server
  webServer.begin();
  SerialMon.println("HTTP server started.");

  //  Authenticate HTTP requests
  const char * headerkeys[] = {"User-Agent","Cookie"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  webServer.collectHeaders(headerkeys, headerkeyssize );
}




//////////////////////////////////////////////////////////////////
/////   SD card
//////////////////////////////////////////////////////////////////

void InitSDCard(){
  SerialMon.println("Initializing SD Card...");
  ledPanel.write(LED_PANEL_SD_CARD_ACCESS, HIGH);
  ledPanel.write(LED_PANEL_SD_CARD_ERROR, HIGH);
  if (SD.begin()){
    ledPanel.write(LED_PANEL_SD_CARD_ACCESS, LOW);
    uint8_t cardType = SD.cardType();                  
    if(cardType == CARD_NONE){
      SerialMon.println("No SD card attached");
    ledPanel.write(LED_PANEL_SD_CARD_ERROR, LOW);
    }
    else{
      SerialMon.print("SD Card Type: ");
      if(cardType == CARD_MMC){
          SerialMon.println("MMC");
      } else if(cardType == CARD_SD){
          SerialMon.println("SDSC");
      } else if(cardType == CARD_SDHC){
          SerialMon.println("SDHC");
      } else {
          SerialMon.println("UNKNOWN");
      }                    
      uint64_t cardSize = SD.cardSize() / (1024 * 1024);
      SerialMon.printf("SD Card Size: %lluMB\r\n", cardSize);
      SerialMon.printf("Total space: %lluMB\r\n", SD.totalBytes() / (1024 * 1024));
      SerialMon.printf("Used space: %lluMB\r\n", SD.usedBytes() / (1024 * 1024));
      SerialMon.printf("Free space: %lluMB\r\n", SD.totalBytes() / (1024 * 1024) - SD.usedBytes() / (1024 * 1024));
    }
    SD.end(); 
    ledPanel.write(LED_PANEL_SD_CARD_ACCESS, HIGH);
    ledPanel.write(LED_PANEL_SD_CARD_ERROR, HIGH);
  }
  else{
    SerialMon.println("SD Card could not be initialized...");
    ledPanel.write(LED_PANEL_SD_CARD_ERROR, LOW);
  }
}

void WriteOrAppendFile(fs::FS &fs, const char * path, const char * message){
    ledPanel.write(LED_PANEL_SD_CARD_ACCESS, LOW);
    SerialMon.printf("INFO: Writing file: %s\r\n", path);

    File file;
    if ( fs.exists(path) ){
      file = fs.open(path, FILE_APPEND);
    }
    else{
      file = fs.open(path, FILE_WRITE);
    }

    if( !file ){
        SerialMon.println("Could not open file for writing.");
        return;
    }
    if( file.print(message) ){
      SerialMon.println("File written to SD card.");
      ledPanel.write(LED_PANEL_SD_CARD_ERROR, HIGH);
    }
    else {
      SerialMon.println("Could not write to SD card.");
      ledPanel.write(LED_PANEL_SD_CARD_ERROR, LOW);
    }
    file.close();
    ledPanel.write(LED_PANEL_SD_CARD_ACCESS, HIGH);
}

void LogToSDCard(char msg[]){
  if (SD.begin()){
    ledPanel.write(LED_PANEL_SD_CARD_ERROR, HIGH);
    WriteOrAppendFile(SD, GetFullDateTime("/%Y%m%d.TXT", size_t(14)), msg);
    SD.end();
  }
  else{
    ledPanel.write(LED_PANEL_SD_CARD_ERROR, LOW);
    SerialMon.println("ERROR: SD card could not be accessed.");
  }
}




//////////////////////////////////////////////////////////////////
/////   GSM modem
//////////////////////////////////////////////////////////////////

void PrintModemProperties(){
  SerialMon.println("========================== Modem settings ==========================");
  SerialMon.printf("Modem name\t%s\r\nInfo\t\t%s\r\nIMEI\t\t%s\r\nIMSI\t\t%s\r\nSIM CCID\t%s\r\nSIM Status\t%u\r\nBattery\t\t%1.3fV\r\nBattery\t\t%i%%\r\nBattery state\t%u\r\nGSM Date/Time\t%s\r\nRegistration\t%s\r\nOperator\t%s\r\nSignal quality\t%i\r\nNetwork\t\t%s\r\nGPRS\t\t%s\r\nIP address\t%s\r\n",
    modem.getModemName().c_str(), modem.getModemInfo().c_str(), modem.getIMEI().c_str(), modem.getIMSI().c_str(), modem.getSimCCID().c_str(),
    modem.getSimStatus(), float(modem.getBattVoltage())/float(1000), modem.getBattPercent(), modem.getBattChargeState(), modem.getGSMDateTime(DATE_FULL).c_str(),
    GetRegistrationStatusName(modem.getRegistrationStatus()).c_str(), modem.getOperator().c_str(), modem.getSignalQuality(),
    modem.isNetworkConnected()?"Connected":"Not connected", modem.isGprsConnected()?"Connected":"Not connected",
    modem.getLocalIP().c_str()
    // modem.getGsmLocation().c_str()
    //modem.getGsmLocationTime(),
    //modem.getNetworkTime(), 
    
  );
  SerialMon.println("==================================================================");
}

void HardwareResetGSMModem(){
  //  Datasheet describes a value of 0.3-105 ms, so 10 ms should be OK.
  digitalWrite(MODEM_RESET_GPIO, LOW);
  delay(10);
  digitalWrite(MODEM_RESET_GPIO, HIGH);
}

void RestartGSMModem(){
  ledPanel.write(LED_PANEL_GPRS, HIGH);
  ledPanel.write(LED_PANEL_GSM_NETWORK, HIGH);
  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Restarting modem...");
  HardwareResetGSMModem();
  modem.restart();
  // modem.init();

  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem Info: ");
  SerialMon.println(modemInfo);

  // Unlock your SIM card with a PIN if needed
  if ( GSM_PIN && modem.getSimStatus() != 3 ) {
    modem.simUnlock(GSM_PIN);
    if (modem.getSimStatus()==SIM_READY)
      SerialMon.println("SIM card unlocked successfully.");
      else
      SerialMon.println("SIM card could not be unlocked.");
  }  
}

void ConnectToGSMNetwork(){
  if (!modem.isNetworkConnected()){
    ledPanel.write(LED_PANEL_GSM_NETWORK, HIGH);
    SerialMon.print("Connecting to GSM network...");
    if (modem.waitForNetwork()){
      SerialMon.println(" success.");
      unsuccessfulGSMAttempts = 0;
      ledPanel.write(LED_PANEL_GSM_NETWORK, LOW);
    }
    else{
      unsuccessfulGSMAttempts++;
      SerialMon.printf(" failed. #%u\r\n", unsuccessfulGSMAttempts);
      if (unsuccessfulGSMAttempts > appSettings.maxUnsuccessfulGSMAttempts)
        RestartGSMModem();
    }
  }
  else{
#ifdef __debugSettings
    SerialMon.println("Modem is already connected to the GSM network, nothing to do.");
#endif
  }
}

void ConnectToGPRS(){
  ConnectToGSMNetwork();

  if (modem.isNetworkConnected()){
    if (!modem.isGprsConnected()){
      ledPanel.write(LED_PANEL_GPRS, HIGH);
      // GPRS connection parameters are usually set after network registration
      SerialMon.print("Connecting to ");
      SerialMon.printf("%s...", apn);
      if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(" fail.");
        return;
      }
      SerialMon.println(" success.");

      SerialMon.println("GPRS connected.");
      ledPanel.write(LED_PANEL_GPRS, LOW);
      SerialMon.print("GPRS IP address: ");
      SerialMon.println(modem.getLocalIP());

      needsHeartbeat = true;
    }
    else{
#ifdef __debugSettings
      SerialMon.println("Modem is already connected to the GPRS network, nothing to do.");
#endif
    }
  }
}

void DisconnectFromGPRS(){
  modem.gprsDisconnect();
  SerialMon.println(F("GPRS disconnected."));
  ledPanel.write(LED_PANEL_GPRS, HIGH);
}

void GSMshit(){

  SerialMon.print(F("Performing HTTP GET request... "));
  int err = http.get(resource);
  if (err != 0) {
    SerialMon.println(F("failed to connect"));
    delay(10000);
    return;
  }

  int status = http.responseStatusCode();
  SerialMon.print(F("Response status code: "));
  SerialMon.println(status);
  if (!status) {
    delay(10000);
    return;
  }

  SerialMon.println(F("Response Headers:"));
  while (http.headerAvailable()) {
    String headerName = http.readHeaderName();
    String headerValue = http.readHeaderValue();
    SerialMon.println("    " + headerName + " : " + headerValue);
  }

  int length = http.contentLength();
  if (length >= 0) {
    SerialMon.print(F("Content length is: "));
    SerialMon.println(length);
  }
  if (http.isResponseChunked()) {
    SerialMon.println(F("The response is chunked"));
  }

  String body = http.responseBody();
  SerialMon.println(F("Response:"));
  SerialMon.println(body);

  SerialMon.print(F("Body length is: "));
  SerialMon.println(body.length());

  http.stop();
  SerialMon.println(F("Server disconnected"));

  DisconnectFromGPRS();  
}




//////////////////////////////////////////////////////////////////
/////   MQTT
//////////////////////////////////////////////////////////////////

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  SerialMon.print("Message arrived [");
  SerialMon.print(topic);
  SerialMon.print("]: ");
  SerialMon.write(payload, len);
  SerialMon.println();

  // Only proceed if incoming message's topic matches
  // if (String(topic) == topicLed) {
  // }
}

void ConnectToMQTTBroker(){
  ConnectToGPRS();

  if (modem.isGprsConnected()){
    if ( !PSclient.connected() ){
        SerialMon.print("Connecting to ");
        SerialMon.print(appSettings.mqttServer);
        SerialMon.print("... ");
        /////////////////////////////////////////////////////////////////////////
        /////////////////////////////////////////////////////////////////////////
        /////////////////////////////////////////////////////////////////////////
        /////////////////////////////////////////////////////////////////////////
        /////////////////////////////////////////////////////////////////////////
        /////////////////////////////////////////////////////////////////////////
        boolean status = PSclient.connect("defaultSSID", (MQTT_CUSTOMER + String("/") + MQTT_PROJECT + String("/") + appSettings.mqttTopic + "/STATE").c_str(), 0, true, "offline" );

        if (status == false) {
          SerialMon.println(" failed.");
        }
        SerialMon.println(" success.");

        PSclient.subscribe((MQTT_CUSTOMER + String("/") + MQTT_PROJECT + String("/") + appSettings.mqttTopic + "/cmnd").c_str(), 0);
        PSclient.publish((MQTT_CUSTOMER + String("/") + MQTT_PROJECT + String("/") + appSettings.mqttTopic + "/STATE").c_str(), "online", true);
      }
      else{
#ifdef __debugSettings
        SerialMon.println("Already connected to the MQTT broker, nothing to do.");
#endif
      }
  }
}

void SendLocationDataToServer(){
  ConnectToMQTTBroker();

  if (PSclient.connected()){
    SerialMon.println("Sending data to server...");

    const size_t capacity = JSON_OBJECT_SIZE(200);
    DynamicJsonDocument doc(capacity);
    char c[11];

    doc["_type"] = "location";
    doc["acc"] = gps.hdop.isValid()?gps.hdop.value():99;
    doc["alt"] = gps.altitude.isValid()?gps.altitude.meters():-1;
    doc["batt"] = modem.getBattPercent();
    doc["conn"] = "m";
    snprintf(c, sizeof(c), "%3.8f", gps.location.lat());
    doc["lat"] = c;
    snprintf(c, sizeof(c), "%3.8f", gps.location.lng());
    doc["lon"] = c;
    doc["t"] = "p";
    doc["tid"] = "XX";
    doc["tst"] = GetTimeSinceEpoch();
    doc["vac"] = 99;
    doc["vel"] = gps.speed.isValid()?gps.speed.kmph():-1;

    #ifdef __debugSettings
    serializeJsonPretty(doc,Serial);
    SerialMon.println();
    #endif

    String myJsonString;
    serializeJson(doc, myJsonString);

    PSclient.publish(("owntracks/" + String(appSettings.mqttTopic) + "/SIM800").c_str(), myJsonString.c_str(), false);
  }

}

void SendHeartbeat(){

  const size_t capacity = JSON_OBJECT_SIZE(30) + JSON_OBJECT_SIZE(60);
  DynamicJsonDocument doc(capacity);

  char c[48];
  uint64_t chipid = ESP.getEfuseMac();

  sprintf(c, "ESP-%04X%08X", (uint16_t)(chipid>>32), (uint32_t)chipid);
  doc["ChipID"] = c;

  if (IsTimeValid()){
    doc["Time"] = GetFullDateTime("%F %T", size_t(20));
  }
  doc["Node"] = ESP.getEfuseMac();
  doc["Freeheap"] = ESP.getFreeHeap();
  doc["FriendlyName"] = appSettings.friendlyName;

  
  // JsonObject wifiDetails = doc.createNestedObject("Wifi");
  // wifiDetails["SSId"] = String(WiFi.SSID());
  // wifiDetails["MACAddress"] = String(WiFi.macAddress());
  // wifiDetails["IPAddress"] = WiFi.localIP().toString();

  JsonObject modemDetails = doc.createNestedObject("GPRS");
  // modemDetails["GsmLocation"] = modem.getGsmLocation();
  modemDetails["IMEI"] = modem.getIMEI();
  String s = modem.getIMSI();
  if (s != NULL)
    modemDetails["IMSI"] = s;
  modemDetails["LocalIP"] = modem.getLocalIP();
  modemDetails["ModemInfo"] = modem.getModemInfo();
  modemDetails["ModemName"] = modem.getModemName();
  modemDetails["Operator"] = modem.getOperator();
  modemDetails["RegistrationStatus"] = GetRegistrationStatusName(modem.getRegistrationStatus());
  modemDetails["SignalQuality"] = String(modem.getSignalQuality());
  modemDetails["SimCCID"] = modem.getSimCCID();

  String myJsonString;

  serializeJson(doc, myJsonString);

#ifdef __debugSettings
  serializeJsonPretty(doc,Serial);
  SerialMon.println();
#endif


  ConnectToMQTTBroker();

  if (PSclient.connected()){
    PSclient.publish((MQTT_CUSTOMER + String("/") + MQTT_PROJECT + String("/") + appSettings.mqttTopic + "/HEARTBEAT").c_str(), myJsonString.c_str(), false);
#ifdef __debugSettings
    SerialMon.println("Heartbeat sent.");
#endif
    needsHeartbeat = false;
  }
}


//////////////////////////////////////////////////////////////////
/////   I2C
//////////////////////////////////////////////////////////////////

void InitI2C(){
  byte error, address;
  int nDevices;

  Wire.begin();

  SerialMon.println("Scanning for I2C devices...");

  nDevices = 0;
  for(address = 1; address < 127; address++ ){
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0){
      SerialMon.print("Device ");
      SerialMon.print(nDevices);
      SerialMon.print(":\t0x");
      if (address<16)
        SerialMon.print("0");
      SerialMon.println(address,HEX);

      nDevices++;
    }
    else if (error==4){
      SerialMon.print("Unknow error at address 0x");
      if (address<16)
        SerialMon.print("0");
      SerialMon.println(address,HEX);
    }
  }
  if (nDevices == 0)
    SerialMon.println("No I2C devices found.\n");

  ledPanel.write8(0xff);

  for (size_t i = 0; i < 8; i++) {
    ledPanel.write(i, 0);
    delay(100);
  }

  for (size_t i = 0; i < 8; i++) {
    ledPanel.write(i, 1);
    delay(100);
  }

}




void setup() {
  // Set console baud rate
  SerialMon.begin(115200);
  delay(1000);

  //  Settings
  LoadSettings(false);
#ifdef __debugSettings
  PrintSettings();
#endif

  String FirmwareVersionString = String(FIRMWARE_VERSION) + " @ " + String(__TIME__) + " - " + String(__DATE__);

  SerialMon.println("Hardware ID:      " + (String)HARDWARE_ID);
  SerialMon.println("Hardware version: " + (String)HARDWARE_VERSION);
  SerialMon.println("Software ID:      " + (String)SOFTWARE_ID);
  SerialMon.println("Software version: " + FirmwareVersionString);
  SerialMon.println();


  //  GPIOs
  pinMode(MODEM_RESET_GPIO, OUTPUT);
  digitalWrite(MODEM_RESET_GPIO, HIGH);

  pinMode(BUTTON_SELECT_MODE, INPUT_PULLUP);

  pinMode(2, OUTPUT);
  digitalWrite(2,LOW);
  

  //  SD card
  InitSDCard();


  //  I2C
  InitI2C();


  //  Select mode of operation
  operationMode = OPERATION_MODES(!digitalRead(BUTTON_SELECT_MODE));
  //operationMode = OPERATION_MODES::WIFI_SETUP;
  SerialMon.printf("\r\n====================================\r\nMode of operation: %s\r\n====================================\r\n", 
    operationMode==OPERATION_MODES::DATA_LOGGING?GetOperationalMode(OPERATION_MODES::DATA_LOGGING):GetOperationalMode(OPERATION_MODES::WIFI_SETUP));


  // Set GSM module baud rate
  SerialAT.begin(MODEM_BAUDRATE, SERIAL_8N1, MODEM_RECEIVE_GPIO, MODEM_SEND_GPIO);
  RestartGSMModem();


  switch (operationMode)
  {
  case OPERATION_MODES::DATA_LOGGING:{

    //  GPS
    sGPS.begin(GPS_BAUDRATE, SERIAL_8N1, GPS_RECEIVE_GPIO, GPS_SEND_GPIO);
    //GSMshit();

    //  MQTT
    PSclient.setServer(appSettings.mqttServer, appSettings.mqttPort);
    PSclient.setCallback(mqttCallback);

    //  Timers/interrupts
    heartbeatTimerSemaphore = xSemaphoreCreateBinary();
    heartbeatTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(heartbeatTimer, &heartbeatTimerCallback, true);
    timerAlarmWrite(heartbeatTimer, appSettings.heartbeatInterval * 1000000, true);
    timerAlarmEnable(heartbeatTimer);

    // Set the initial connection state

    locationLastLoggedToSDCard = locationLastLoggedToMQTT = millis();

    break;
  }
  
  case OPERATION_MODES::WIFI_SETUP:{
    //  Internal file system

    if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
      SerialMon.println("Error: Failed to initialize the filesystem!");
    }

    connectionState = STATE_CHECK_WIFI_CONNECTION;

    ConnectToGPRS();
    PrintModemProperties();
    break;
  }
  default:
    break;
  }
}

void loop() {
  switch (operationMode){
  case OPERATION_MODES::DATA_LOGGING:{
    while ( sGPS.available() > 0) 
      gps.encode(sGPS.read());

    //  Update time if necessary
    if ( needsTime && gps.time.isValid() ){
      needsTime = !SetSystemTimeFromGPS();
    }

    if ( gps.location.isUpdated() ){

      if ( millis() - locationLastLoggedToSDCard > appSettings.logToSDCardInterval * 1000 ){
        if ( gps.location.isValid() ){

          //  Create message to log
          char log[100];
          snprintf(log, sizeof(log), "%02u-%02u-%02u %02u:%02u:%02u, %u, %3.8f %3.8f, %3.1f\r\n", 
            gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second(), gps.satellites.value(), gps.location.lat(), gps.location.lng(), gps.speed.kmph());

          SerialMon.print(log);

          LogToSDCard(log);
          locationLastLoggedToSDCard = millis();
        }
      }

      if ( millis() - locationLastLoggedToMQTT > appSettings.logToMQTTServerInterval * 1000 ){
        if ( gps.location.isValid() ){
          SendLocationDataToServer();
          locationLastLoggedToMQTT = millis();
        }
      }
    }

    //  Heartbeat
    if ( needsHeartbeat ) SendHeartbeat();

    PSclient.loop();

  }
    break;
  case OPERATION_MODES::WIFI_SETUP:{
    if (isAccessPoint){
      if (!isAccessPointCreated){
        SerialMon.printf("Could not connect to %s.\r\nReverting to Access Point mode.\r\n", appSettings.ssid);

        delay(500);

        WiFi.mode(WIFI_AP);
        WiFi.softAP(appSettings.AccessPointSSID, appSettings.AccessPointPassword);

        IPAddress myIP = WiFi.softAPIP();
        isAccessPointCreated = true;

        InitWifiWebServer();

        SerialMon.println("Access point created. Use the following information to connect to the ESP device, then follow the on-screen instructions to connect to a different wifi network:");

        SerialMon.print("SSID:\t\t\t");
        SerialMon.println(appSettings.AccessPointSSID);

        SerialMon.print("Password:\t\t");
        SerialMon.println(appSettings.AccessPointPassword);

        SerialMon.print("Access point address:\t");
        SerialMon.println(myIP);

      }
      webServer.handleClient();
    }
    else{
      switch (connectionState) {

        // Check the WiFi connection
        case STATE_CHECK_WIFI_CONNECTION:{
          // Are we connected ?
          if (WiFi.status() != WL_CONNECTED) {
            // Wifi is NOT connected
            digitalWrite(CONNECTION_STATUS_LED_GPIO, HIGH);
            connectionState = STATE_WIFI_CONNECT;
          } else  {
            // Wifi is connected so check Internet
            digitalWrite(CONNECTION_STATUS_LED_GPIO, LOW);
            connectionState = STATE_CHECK_INTERNET_CONNECTION;
          }
          break;
        }

        // No Wifi so attempt WiFi connection
        case STATE_WIFI_CONNECT:{
            // Indicate NTP no yet initialized
            ntpInitialized = false;

            digitalWrite(CONNECTION_STATUS_LED_GPIO, HIGH);
            SerialMon.printf("Trying to connect to WIFI network: %s", appSettings.ssid);

            // Set station mode
            WiFi.mode(WIFI_STA);

            // Start connection process
            WiFi.begin(appSettings.ssid, appSettings.password);

            // Initialize iteration counter
            uint8_t attempt = 0;

            while ((WiFi.status() != WL_CONNECTED) && (attempt++ < WIFI_CONNECTION_TIMEOUT)) {
              digitalWrite(CONNECTION_STATUS_LED_GPIO, LOW);
              SerialMon.print(".");
              delay(50);
              digitalWrite(CONNECTION_STATUS_LED_GPIO, HIGH);
              delay(950);
            }
            if (attempt >= WIFI_CONNECTION_TIMEOUT) {
              SerialMon.println();
              SerialMon.println("Could not connect to WiFi.");
              delay(100);

              isAccessPoint=true;

              break;
            }
            digitalWrite(CONNECTION_STATUS_LED_GPIO, LOW);
            SerialMon.println(" Success!");
            SerialMon.print("IP address: ");
            SerialMon.println(WiFi.localIP());
            connectionState = STATE_CHECK_INTERNET_CONNECTION;
            break;
          }

        case STATE_CHECK_INTERNET_CONNECTION:{
          // Do we have a connection to the Internet ?
          if (checkInternetConnection()) {
            // We have an Internet connection
            if (!ntpInitialized) {
              // We are connected to the Internet for the first time so set NTP provider
              initNTP();

              ntpInitialized = true;

              SerialMon.println("Connected to the Internet.");

              //  OTA
              ArduinoOTA.onStart([]() {
                SerialMon.println("OTA started.");
              });

              ArduinoOTA.onEnd([]() {
                SerialMon.println("\nOTA finished.");
              });

              ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
                SerialMon.printf("Progress: %u%%\r", (progress / (total / 100)));
                if (progress % OTA_BLINKING_RATE == 0){
                  if (digitalRead(CONNECTION_STATUS_LED_GPIO)==LOW)
                    digitalWrite(CONNECTION_STATUS_LED_GPIO, HIGH);
                    else
                    digitalWrite(CONNECTION_STATUS_LED_GPIO, LOW);
                }
              });

              ArduinoOTA.onError([](ota_error_t error) {
                SerialMon.printf("Error[%u]: ", error);
                if (error == OTA_AUTH_ERROR) SerialMon.println("Authentication failed.");
                else if (error == OTA_BEGIN_ERROR) SerialMon.println("Begin failed.");
                else if (error == OTA_CONNECT_ERROR) SerialMon.println("Connect failed.");
                else if (error == OTA_RECEIVE_ERROR) SerialMon.println("Receive failed.");
                else if (error == OTA_END_ERROR) SerialMon.println("End failed.");
              });

              ArduinoOTA.begin();
              InitWifiWebServer();
            }

            connectionState = STATE_INTERNET_CONNECTED;
            digitalWrite(CONNECTION_STATUS_LED_GPIO, HIGH);
          } else  {
            connectionState = STATE_CHECK_WIFI_CONNECTION;
            digitalWrite(CONNECTION_STATUS_LED_GPIO, LOW);
          }
          break;
        }

        case STATE_INTERNET_CONNECTED:{
          ArduinoOTA.handle();

          // Set next connection state
          connectionState = STATE_CHECK_WIFI_CONNECTION;
          break;

        }

      }
      webServer.handleClient();
    }

  }
    break;
  
  default:
    break;
  }
}
