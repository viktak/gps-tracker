struct Settings{
  u_char FailedBootAttempts;

  char ssid[32];
  char password[32];

  char adminPassword[32];

  char AccessPointSSID[32];
  char AccessPointPassword[32];

  char friendlyName[32];

  signed char timeZone;

  char mqttServer[64];
  uint16_t mqttPort;
  char mqttTopic[64];

  u_int heartbeatInterval;
  u_int logToSDCardInterval;
  u_int logToMQTTServerInterval;

  u_char maxfailedGSMAttempts;

  char simPIN[16];
  char gprsAPName[32];
  char gprsUserName[32];
  char gprsPassword[32];

  u_char requiredGPSAccuracy;

};


