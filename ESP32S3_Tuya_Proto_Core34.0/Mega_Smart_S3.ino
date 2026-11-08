// Mega Smart switch - ESP32-S3 (N16R8) port
#include <Arduino.h>
#include "Mega_Smart_Config.h"
#include "Mega_Smart_Interface.h"
#include "Mega_Smart_Private.h"
#include "tuya_cloud.h"
#include "tuya_dp_handler.h"

bool isFirstLoop = true;

void setup() {

  setCpuFrequencyMhz(160);  // same as the ESP8266 build (S3 default is 240)

  Serial.begin(SERIAL_BAUD);

    delay(3000);   // give Serial Monitor time to attach before printing

  // Native USB-CDC needs a moment for the host to open the port; UART bridge returns at once.
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < 1500) {
    delay(10);
  }

  if (isDebugOn) {
    Serial.println("..");
    Serial.println("Started...");
  }

  initDeviceInfo();   // loads (or creates) the stored JSON config in NVS

  Mega_Smart_init();   // WiFi mode, UDP, pins, RGB

  void_connectToRouter();   // first pass only (waits ~0.5 s); loop() keeps retrying

  tuya_cloud_init();        // MQTT/TLS setup + start SNTP (SNTP syncs once Wi-Fi is up)

  Mega_Smart_startScheduler();  // replaces tickerResetPin (1 s) and tickerMonitorTouchSensors (50 ms)

  delay(1000);

  if (isDebugOn) {
    Serial.println("");
    Serial.println("Started cont...");
  }
}

void loop() {

  // Kept from the original: RGB output is only driven after the first loop pass.
  if (!isFirstLoop) {

    Mega_Smart_poll();          // touch sensors, 1 s counter, LED indicator
    void_connectToRouter();     // AP only when there are no credentials or after a reset
    checkResetProcedure();
    GetData_And_TakeAction();   // UDP
    tuya_loop();                // Tuya Cloud MQTT servicing & reconnect handler

  } else {
    isFirstLoop = false;
  }

  delay(10);
}