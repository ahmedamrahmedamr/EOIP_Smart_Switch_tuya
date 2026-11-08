// Mega Smart switch - ESP32-S3 (N16R8) port
#include <Arduino.h>
#include "Mega_Smart_Config.h"
#include "Mega_Smart_Interface.h"
#include "Mega_Smart_Private.h"
#include "tuya_cloud.h"
#include "tuya_dp_handler.h"

bool isFirstLoop = true;

void setup() {
  setCpuFrequencyMhz(160);  

  Serial.begin(SERIAL_BAUD);
  delay(3000);   

  uint32_t t0 = millis();
  while (!Serial && (millis() - t0 < 1500)) { // Fixed missing operator here
    delay(10);
  }

  if (isDebugOn) {
    Serial.println("..");
    Serial.println("Started...");
  }

  initDeviceInfo();     

  Mega_Smart_init();    

  void_connectToRouter();     
  tuya_cloud_init();            

  Mega_Smart_startScheduler();  

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