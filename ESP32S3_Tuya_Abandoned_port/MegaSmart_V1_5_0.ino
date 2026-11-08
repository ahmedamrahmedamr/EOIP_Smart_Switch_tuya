// MegaSmart V1.5.0 - ESP32-S3 build
#include <WiFi.h>
#include <WiFiUdp.h>
#include "Mega_Smart_Config.h"
#include "Mega_Smart_Interface.h"
#include "Mega_Smart_Private.h"

extern bool isDebugOn;

void setup() {
  Serial.begin(115200);
  delay(300); // Allow UART buffer and monitor to initialize

  Serial.println("\n\n=================================");
  Serial.println("MegaSmart starting on ESP32-S3...");
  Serial.println("=================================");

  // --- TEST LEDs on GPIO 13 & 14 (HIGH = LED on) ---
  pinMode(TEST_LED0_PIN, OUTPUT);
  pinMode(TEST_LED1_PIN, OUTPUT);
  digitalWrite(TEST_LED0_PIN, HIGH);
  digitalWrite(TEST_LED1_PIN, HIGH);
  // -------------------------------------------------

  // EEPROM is handled internally via Preferences in initDeviceInfo()
  initDeviceInfo();
  Mega_Smart_init();

  delay(1000);

  if (isDebugOn) {
    Serial.println("");
    Serial.println("Started cont on ESP32...");
  }
}

// All periodic work runs here in the main task (no timer callbacks touch the network or the JSON state).
void loop() {
  static unsigned long lastTouchMs = 0;
  static unsigned long lastSecMs = 0;
  unsigned long now = millis();

  if (now - lastTouchMs >= 50) {  // touch/button scan every 50 ms
    lastTouchMs = now;
    MonitorTouchSensors();
  }

  if (now - lastSecMs >= 1000) {  // 1 s tick: reset-button timer + periodic status update
    lastSecMs = now;
    secCounter();
  }

  ServiceNotifications();   // send any pending state-change update
  void_connectToRouter();   // does not open the AP unless there are no credentials or the reset pattern was used
  checkResetProcedure();
  GetData_And_TakeAction();

  delay(10);
}
