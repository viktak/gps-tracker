#ifndef ENUMS_H
#define ENUMS_H

enum BUTTON_STATES {
  BUTTON_NOT_PRESSED,    //  button not pressed
  BUTTON_BOUNCING,       //  button in not stable state
  BUTTON_PRESSED,        //  button pressed
  BUTTON_LONG_PRESSED    //  button pressed for at least BUTTON_LONG_PRESS_TRESHOLD milliseconds
};

enum OPERATION_MODES{
  DATA_LOGGING,
  WIFI_SETUP
};

// Connection FSM operational states
enum CONNECTION_STATE {
  STATE_CHECK_WIFI_CONNECTION,
  STATE_WIFI_CONNECT,
  STATE_CHECK_INTERNET_CONNECTION,
  STATE_INTERNET_CONNECTED
};


#endif

