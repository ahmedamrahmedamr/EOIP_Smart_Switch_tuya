#ifndef MEGA_SMART_INTERFACE_H
#define MEGA_SMART_INTERFACE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Global persistent JSON config + live state
extern JsonDocument deviceInfo;
extern bool isDebugOn;

//###################### Connecting To WiFi #########################
#define WIFI_CONNECTED 1
#define WIFI_NOT_CONNECTED 0

enum Status {
  ROUTER_CONNECTED,
  ROUTER_NOT_CONNECTED,
  AP_CONNECTED,
  AP_NOT_CONNECTED,
  ROUTER_CONNECTING
};

// Periodic work (touch, 1 s counter, LED pattern). Runs in loop() context, never in a timer task.
void Mega_Smart_startScheduler();
void Mega_Smart_poll();

void secCounter();
void checkResetProcedure();

//###################### RGB Data ###############################
#define NUM_LEDS 20
#define DATA_PIN 15

//###################### Functions Prototypes #########################
bool ConnectToWiFi(String ssid, String password, int waitCount = 300, bool restartConnection = true);
void MonitorTouchSensors();
void GetData_And_TakeAction();
void Mega_Smart_init();
bool boolSetDeviceInfoToStorage(const String& data);
bool boolGetDeviceInfoFromStorage();
String Get_Mac_Address();
void takeAction(String cmd);
void initDeviceInfo(void);
void AutoStatusUpdate(void);
void saveDataToStorage(void);
void updateIndicationLED(void);

void sendOKToSender(void);
void sendReplyToAll(void);
void sendReplyToSender(void);

void setColor(uint8_t redValue, uint8_t greenValue, uint8_t blueValue);

String generateRandomHexString(uint8_t bytesLength);
String generateRandomToken(void);
void checkResetPattern(uint8_t pos);
void void_resetSwitch(void);

void void_connectToRouter();

#endif
