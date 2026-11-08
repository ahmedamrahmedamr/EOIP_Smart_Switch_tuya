/*
  How to Set a Pattern for the LED Indicator:
  ===========================================
  The `setLEDPattern` method allows you to define a custom blinking pattern for the LED.

  Parameters:
  -----------
  - on1Time:  Time (in arbitrary units) the LED stays ON in the first phase.
  - off1Time: Time (in arbitrary units) the LED stays OFF after the first ON phase.
  - on2Time:  Time (in arbitrary units) the LED stays ON in the second phase.
  - off2Time: Time (in arbitrary units) the LED stays OFF after the second ON phase.

  Example Usage:
  --------------
  ledIndicator.setLEDPattern(0, 1, 5, 2);
  
  Explanation of Example:
  -----------------------
  - The LED stays OFF for 0 units of time.
  - Then it stays ON for 1 unit of time.
  - After that, it stays OFF for 5 units of time.
  - Finally, it stays ON for 2 units of time.
  
  This will create a custom blinking pattern where the LED blinks according to the specified ON/OFF durations.
*/



#ifndef LEDINDICATOR_H
#define LEDINDICATOR_H

#include <Ticker.h>

class LEDIndicator {
private:
  int ledPin;
  int on1Time, off1Time, on2Time, off2Time;
  bool ledActiveState;
  int intervalCounter;
  int totalPatternDuration;
  Ticker ticker;
  float interval;
  bool tickerEnabled;

  void (*customCallback)();  // Function pointer for the callback, it called every period of time

  void updateLEDInternal();

public:
  LEDIndicator(int pin, float tickerInterval = 0.1, bool activeState = LOW);

  void setLEDPattern(int on1Time_, int off1Time_, int on2Time_, int off2Time_);
  void enable();
  void disable();
  void ledOn();
  void ledOff();

  // Set the custom callback function to be executed every interval
  void setCallback(void (*callback)());
};

#endif
