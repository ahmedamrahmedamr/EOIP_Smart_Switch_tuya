#include "LEDIndicator.h"
#include <Arduino.h>

LEDIndicator::LEDIndicator(int pin, float tickerInterval, bool activeState) {
  ledPin = pin;
  ledActiveState = activeState;
  interval = tickerInterval;
  pinMode(ledPin, OUTPUT);
  intervalCounter = 0;
  tickerEnabled = false;
  customCallback = nullptr;  // Initialize callback to null
  setLEDPattern(0, 1, 0, 1);
}

void LEDIndicator::setLEDPattern(int on1Time_, int off1Time_, int on2Time_, int off2Time_) {
  on1Time = on1Time_;
  off1Time = off1Time_;
  on2Time = on2Time_;
  off2Time = off2Time_;
  totalPatternDuration = on1Time + off1Time + on2Time + off2Time;
}

void LEDIndicator::enable() {
  if (!tickerEnabled) {
    ticker.attach(interval, [this]() { this->updateLEDInternal(); });
    tickerEnabled = true;
  }
}

void LEDIndicator::disable() {
  if (tickerEnabled) {
    ticker.detach();
    tickerEnabled = false;
  }
}

void LEDIndicator::updateLEDInternal() {
  intervalCounter++;

  // Execute the custom callback function if set
  if (customCallback != nullptr) {
    customCallback();
  }

  // LED pattern update
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

// Set the custom callback function
void LEDIndicator::setCallback(void (*callback)()) {
  customCallback = callback;
}
