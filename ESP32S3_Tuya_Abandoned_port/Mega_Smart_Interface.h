#ifndef MEGA_SMART_INTERFACE_H
#define MEGA_SMART_INTERFACE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

//###################### Connecting To WiFi #########################

#define WIFI_CONNECTED 1
#define WIFI_NOT_CONNECTED 0

// Define WiFi statuses
enum Status {
  ROUTER_CONNECTED,
  ROUTER_NOT_CONNECTED,
  AP_CONNECTED,
  AP_NOT_CONNECTED,
  ROUTER_CONNECTING
};



// // Function prototypes
// void updateLED();
// void setLEDPattern(int on1Time_, int off1Time_, int on2Time_, int off2Time_);
// void ledOn();
// void ledOff();

// Check reset button
void secCounter();
void checkResetProcedure();

//###################### RGB Data ###############################

#define NUM_LEDS 20  // Since you have only one LED
#define DATA_PIN 15


//###################### Functions Prototypes #########################
bool ConnectToWiFi(String ssid, String password);
void MonitorTouchSensors();
void ServiceNotifications(void);
void Connecting_To_Router_Or_Ap();
void GetData_And_TakeAction();
void Mega_Smart_init();
void MonitorRGB_LEDs();
bool boolSetDeviceInfoToEEPROM(char* data);
bool boolGetDeviceInfoFromEEPROM();
String Get_Mac_Address();
void takeAction(String cmd);
void initDeviceInfo(void);
void AutoStatusUpdate(void);
void saveDataToEEPROM(void);
void updateIndicationLED(void);
void ensureUdpStarted(void);

void sendOKToSender(void);
void sendReplyToAll(void);
void sendReplyToSender(void);


String generateRandomHexString(uint8_t bytesLength);
String generateRandomToken(void);
void checkResetPattern(uint8_t pos);
void void_resetSwitch(void);

void void_connectToRouter();

// void firmwareDownloadingProcedures(void);


#endif