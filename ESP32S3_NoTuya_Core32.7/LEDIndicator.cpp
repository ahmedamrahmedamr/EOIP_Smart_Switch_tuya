#include "LEDIndicator.h"
#include <Arduino.h>

LEDIndicator::LEDIndicator(int pin, float tickerInterval, bool activeState) {
  ledPin = pin;
  ledActiveState = activeState;
  intervalMs = (uint32_t)(tickerInterval * 1000.0f);
  if (intervalMs == 0) intervalMs = 1;
  intervalCounter = 0;
  lastTickMs = 0;
  enabled = false;
  customCallback = nullptr;
  setLEDPattern(0, 1, 0, 1);
  // No pinMode() here: global constructors run before the Arduino core is initialised.
}

void LEDIndicator::begin() {
  pinMode(ledPin, OUTPUT);
  ledOff();
}

void LEDIndicator::setLEDPattern(int on1Time_, int off1Time_, int on2Time_, int off2Time_) {
  on1Time = on1Time_;
  off1Time = off1Time_;
  on2Time = on2Time_;
  off2Time = off2Time_;
  totalPatternDuration = on1Time + off1Time + on2Time + off2Time;
}

void LEDIndicator::enable() {
  enabled = true;
  lastTickMs = millis();
}

void LEDIndicator::disable() {
  enabled = false;
}

void LEDIndicator::service() {
  if (!enabled) return;
  uint32_t now = millis();
  if ((uint32_t)(now - lastTickMs) >= intervalMs) {
    lastTickMs = now;
    updateLEDInternal();
  }
}

void LEDIndicator::updateLEDInternal() {
  intervalCounter++;

  if (customCallback != nullptr) {
    customCallback();
  }

  if (intervalCounter <= on1Time) {
    ledOn();
  } else if (intervalCounter <= (on1Time + off1Time)) {
    ledOff();
  } else if (intervalCounter <= (on1Time + off1Time + on2Time)) {
    ledOn();
  } else if (intervalCounter <= totalPatternDuration) {
    ledOff();
  } else {
    intervalCounter = 0;
  }
}

void LEDIndicator::ledOn() {
  digitalWrite(ledPin, ledActiveState);
}

void LEDIndicator::ledOff() {
  digitalWrite(ledPin, !ledActiveState);
}

void LEDIndicator::setCallback(void (*callback)()) {
  customCallback = callback;
}
