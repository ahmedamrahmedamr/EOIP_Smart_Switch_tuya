/*
  LED indicator pattern (same behaviour as the ESP8266 version).

  setLEDPattern(on1Time, off1Time, on2Time, off2Time) - durations in steps of the
  indicator interval (default 0.1 s). Example: ledIndicator.setLEDPattern(1, 20, 1, 20);

  ESP32 change: no Ticker. loop() calls service() through Mega_Smart_poll(), so the
  pattern callback (which reads deviceInfo) never runs in another task.
*/
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

  void begin();  // call from setup() path, not from a global constructor
  void setLEDPattern(int on1Time_, int off1Time_, int on2Time_, int off2Time_);
  void enable();
  void disable();
  void service();  // call often from loop()
  void ledOn();
  void ledOff();
  void setCallback(void (*callback)());
};

#endif
