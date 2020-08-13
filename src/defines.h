#ifndef DEFINES_H
#define DEFINES_H

////////////////////////////////////////////////////////////////////
/// General
////////////////////////////////////////////////////////////////////

#define MQTT_CUSTOMER "viktak"
#define MQTT_PROJECT  "spiti"

#define HARDWARE_ID "ESP32-WROOM-32 dev board"
#define HARDWARE_VERSION "1.0"
#define SOFTWARE_ID "GPS tracker"
#define FIRMWARE_VERSION "1.0"

#define DST_TIMEZONE_OFFSET 3    // Day Light Saving Time offset
#define ST_TIMEZONE_OFFSET  2    // Standard Time offset

#define ESP_ACCESS_POINT_NAME_SIZE  63
#define WIFI_CONNECTION_TIMEOUT 10
#define OTA_BLINKING_RATE 3
#define NTP_REFRESH_INTERVAL 3600
#define MAX_FAILED_BOOT_ATTEMPTS   5
#define MAX_WIFI_INACTIVITY     120



////////////////////////////////////////////////////////////////////
/// Deafult values
////////////////////////////////////////////////////////////////////

#define DEFAULT_GSM_PIN                         "1747"
#define DEFAULT_GPRS_APN_NAME                   "internet"
#define DEFAULT_GPRS_USERNAME                   ""
#define DEFAULT_GPRS_PASSWORD                   ""

#define DEFAULT_ADMIN_USERNAME                  "admin"
#define DEFAULT_ADMIN_PASSWORD                  "admin"
#define DEFAULT_APP_FRIENDLY_NAME               "GPS Tracker/Logger"
#define DEFAULT_AP_SSID                         "TRACKER"
#define DEFAULT_AP_PASSWORD                     "12345678"

#define DEFAULT_TIMEZONE                        2

#define DEFAULT_MQTT_SERVER                     "test.mosquitto.org"
#define DEFAULT_MQTT_PORT                       1883
#define DEFAULT_MQTT_TOPIC                      "tracker"

#define DEFAULT_HEARTBEAT_INTERVAL              300 //  seconds
#define DEFAULT_LOG_TO_SD_CARD_INTERVAL         5   //  seconds
#define DEFAULT_LOG_TO_MQTT_SERVER_INTERVAL     60  //  seconds

#define DEFAULT_MAX_FAILED_GSM_ATTEMPTS         3   //  number (of attempts)
#define DEFAULT_MAX_FAILED_MQTT_ATTEMPTS        5   //  number (of attempts)




////////////////////////////////////////////////////////////////////
/// Buttons
////////////////////////////////////////////////////////////////////

#define BUTTON_SELECT_MODE 0



////////////////////////////////////////////////////////////////////
/// Modem
////////////////////////////////////////////////////////////////////

#define TINY_GSM_MODEM_SIM800
// #define TINY_GSM_MODEM_SIM808
// #define TINY_GSM_MODEM_SIM868
// #define TINY_GSM_MODEM_SIM900
// #define TINY_GSM_MODEM_SIM7000
// #define TINY_GSM_MODEM_SIM5360
// #define TINY_GSM_MODEM_SIM7600
// #define TINY_GSM_MODEM_UBLOX
// #define TINY_GSM_MODEM_SARAR4
// #define TINY_GSM_MODEM_M95
// #define TINY_GSM_MODEM_BG96
// #define TINY_GSM_MODEM_A6
// #define TINY_GSM_MODEM_A7
// #define TINY_GSM_MODEM_M590
// #define TINY_GSM_MODEM_MC60
// #define TINY_GSM_MODEM_MC60E
// #define TINY_GSM_MODEM_ESP8266
// #define TINY_GSM_MODEM_XBEE
// #define TINY_GSM_MODEM_SEQUANS_MONARCH

// Increase RX buffer to capture the entire response
// Chips without internal buffering (A6/A7, ESP8266, M590)
// need enough space in the buffer for the entire response
// else data will be lost (and the http library will fail).
#if !defined(TINY_GSM_RX_BUFFER)
#define TINY_GSM_RX_BUFFER 650
#endif

// Add a reception delay - may be needed for a fast processor at a slow baud rate
// #define TINY_GSM_YIELD() { delay(2); }

#define MODEM_BAUDRATE      115200

#define MODEM_RESET_GPIO    33
#define MODEM_RECEIVE_GPIO  16
#define MODEM_SEND_GPIO     17
#define NETWORK_TIMEOUT     10000

// See all AT commands, if wanted
//#define DUMP_AT_COMMANDS


////////////////////////////////////////////////////////////////////
/// Hardware serial
////////////////////////////////////////////////////////////////////

#define SerialMon Serial
#define SerialAT Serial1
#define SerialGPS Serial2
#define TINY_GSM_DEBUG SerialMon

#define DEBUG_BAUDRATE 115200



////////////////////////////////////////////////////////////////////
/// GPS
////////////////////////////////////////////////////////////////////

#define GPS_BAUDRATE 9600
#define GPS_RECEIVE_GPIO 13
#define GPS_SEND_GPIO -1



////////////////////////////////////////////////////////////////////
/// SD Card
////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////
/// LED notifications
////////////////////////////////////////////////////////////////////

#define CONNECTION_STATUS_LED_GPIO 2
#define LED_PANEL_ADDRESS           0x27

#define LED_PANEL_SD_CARD_ACCESS    7
#define LED_PANEL_SD_CARD_ERROR     6
#define LED_PANEL_GSM_NETWORK       5
#define LED_PANEL_GPRS              4
#define LED_PANEL_WIFI_MODE         3


#endif