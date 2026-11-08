#ifndef MEGA_SMART_PRIVATE_H
#define MEGA_SMART_PRIVATE_H

#include <Arduino.h>

extern bool isDebugOn;

void ClearBuffer(char* buffer);

void setColor(uint8_t redValue, uint8_t greenValue, uint8_t blueValue);

// OTA callbacks (signatures match HTTPUpdate on ESP32)
void onStartOfUpdate(void);
void onProgressOfUpdate(int i1, int i2);
void onEndOfUpdate(void);

#endif
