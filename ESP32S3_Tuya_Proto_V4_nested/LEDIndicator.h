#ifndef LEDINDICATOR_H
#define LEDINDICATOR_H

#include <Arduino.h>

class LEDIndicator {
private:
  int ledPin;
  int on1Time, off1Time, on2Time, off2Time;
  bool ledActiveState;
  int intervalCounter;
  int totalPatternDuration;
  uint32_t intervalMs;
  uint32_t lastTickMs;
  bool enabled;

  void (*customCallback)();  // called every interval

  void updateLEDInternal();

public:
  LEDIndicator(int pin, float tickerInterval = 0.1, bool activeState = LOW);

  void begin();
  void setLEDPattern(int on1Time_, int off1Time_, int on2Time_, int off2Time_);
  void enable();
  void disable();
  void service();  // called from Mega_Smart_poll() in loop()
  void ledOn();
  void ledOff();
  void setCallback(void (*callback)());
};

#endif
