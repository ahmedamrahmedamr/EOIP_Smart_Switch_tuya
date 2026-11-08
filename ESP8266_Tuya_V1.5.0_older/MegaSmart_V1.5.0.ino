
#include <WiFiUdp.h>
#include <EEPROM.h>
#include "Mega_Smart_Config.h"
#include "Mega_Smart_interface.h"
#include "Mega_Smart_Private.h"
#include <Ticker.h>
#include "tuya_cloud.h"
#include "tuya_dp_handler.h"

Ticker tickerMonitorTouchSensors;
extern bool isDebugOn;
extern Ticker tickerResetPin;


void setup() {

  system_update_cpu_freq(160);  // Set CPU to 160 MHz

  Serial.begin(921600);

  if (isDebugOn) {
    Serial.println("..");
    Serial.println("Started...");
  }

  EEPROM.begin(EEPROM_SIZE);

  initDeviceInfo();

  Mega_Smart_init();

  // Initialize TuyaLink cloud connection
  tuya_handler_init();

  tickerResetPin.attach(1, secCounter);  // check reset and open access point

  //###################################################################

  // Initialize the ticker to call the MonitorTouchSensors function every // 0.1 seconds = 100 milliseconds
  tickerMonitorTouchSensors.attach(0.05, MonitorTouchSensors);

  // delay(300);
  delay(1000);

  if (isDebugOn) {
    Serial.println("");
    Serial.println("Started cont...");
  }
}

bool isFirstLoop = 1;

void loop() {

  // This because RGB does not work correctly else after first loop
  if (!isFirstLoop) {


    //--------------------------------- Try to Connect to Router or AP  ---------------------------------
    // Connecting_To_Router_Or_Ap(); 
    // Automatic switch between router and access point if can not connect to router

    void_connectToRouter(); // Don't turn access point else if no WiFi credentials or pattern occured

    //--------------------------------- TuyaLink Cloud Loop ---------------------------------------------
    tuya_loop();

    checkResetProcedure();

    //----------------------------------- UDP Data And Take acction ------------------------------------
    GetData_And_TakeAction();


  } else {
    isFirstLoop = 0;
  }

  delay(10);
}
