#include "WString.h"
#include "core_esp8266_features.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <FastLED.h>
#include "Mega_Smart_Config.h"
#include "Mega_Smart_Interface.h"
#include "Mega_Smart_Private.h"
#include <ArduinoJson.h>
#include <Ticker.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "LEDIndicator.h"
#include "tuya_dp_handler.h"


bool isDebugOn = 1;

// Define the maximum JSON capacity based on the JSON file size
const int capacity = EEPROM_SIZE;

// Create a buffer to store the JSON file
JsonDocument deviceInfo;

// To handle firmware data
String firmwareDownloadLink = "https://firebasestorage.googleapis.com/v0/b/smart-home-aae4e.appspot.com/o/firmware-update%2Fswitch%2Ffirmware.ino.bin?alt=media";
String firmwareVersionLink = "https://firebasestorage.googleapis.com/v0/b/smart-home-aae4e.appspot.com/o/firmware-update%2Fswitch%2Ffirmware_version.txt?alt=media";

// To handle received data
JsonDocument receivedInfo;


// Create an LEDIndicator object (using custom active state and default ticker interval)
LEDIndicator ledIndicator(LED_INDICATOR_PIN, 0.1, LED_INDICATOR_ACTIVE);

Ticker tickerResetPin;


int gResetSecCounter = 0;
int gSecCounter = 0;


IPAddress senderIP;

//######################### Access Point Data ############################


String macAddress = "";

bool b_isAPStarted = 0;
unsigned long ul_APTimeOut = 0;

//########################### UDP #################################


char packetBuffer[UDP_TX_PACKET_MAX_SIZE + 1];
String ReplyBuffer = "";

WiFiUDP Udp;

//###################################################################

//########################### Switches ##############################
uint8_t touchSen_LastRead_0 = 0;
uint8_t touchSen_LastRead_1 = 0;
uint8_t touchSen_LastRead_2 = 0;

uint8_t flashButton_LastRead = !FLASH_BUTTON_ACTIVE;

//###################################################################

//########################### RGB ##############################
CRGB leds[NUM_LEDS];


//###################### Connecting To WiFi #########################

Status currentConnectionStatusESP = ROUTER_NOT_CONNECTED, Last_state = ROUTER_NOT_CONNECTED;


const String defaultData = "{\"device_type\":\"switch\",\"mac_address\":\"00:11:22:33:44:55\",\"device_location\":\"Living Room\",\"firmware_version\":\"1.1.0\",\"wifi_ssid\":\"\",\"wifi_password\":\"01019407823EOIP\",\"server_ip\":\"server_ip\",\"server_port\":1234,\"sending_to_port\":8081,\"listen_on_port\":8888,\"commands\":\"command\",\"sw0\":0,\"sw1\":0,\"sw2\":0,\"led\":1,\"red\":255,\"green\":255,\"blue\":255,\"update_period\":10}";

void initDeviceInfo(void) {


  if (isDebugOn) {

    Serial.println("firmware_version: ");

    Serial.print("latest_firmware_url: ");
    Serial.println(firmwareVersionLink);

    Serial.print("download_firmware_url: ");
    Serial.println(firmwareDownloadLink);

    Serial.print("capacity: ");
    Serial.println(capacity);

    Serial.print("defaultData: ");
    Serial.println(defaultData.length());
  }


  if (!boolGetDeviceInfoFromEEPROM()) {

    // Parse the defaultData JSON
    deserializeJson(deviceInfo, defaultData);

    if (isDebugOn) {
      Serial.print("defaultData before saving");
      Serial.println(defaultData);
    }

    saveDataToEEPROM();
  }
  // Else if data already existed on eeprom and need update
  else if (deviceInfo["firmware_version"] != FIRMWARE_VERSION) { 
    
    JsonDocument tempDeviceInfo = deviceInfo;

    // Parse the defaultData JSON
    deserializeJson(deviceInfo, defaultData);

    deviceInfo["firmware_version"] = FIRMWARE_VERSION;

    // Make data you want to keep only, any unwritten data will be taken from default data
    deviceInfo["wifi_ssid"] = tempDeviceInfo["wifi_ssid"];
    deviceInfo["wifi_password"] = tempDeviceInfo["wifi_password"];
    // deviceInfo["red"] = tempDeviceInfo["red"];
    // deviceInfo["green"] = tempDeviceInfo["green"];
    // deviceInfo["blue"] = tempDeviceInfo["blue"];
    // deviceInfo[""] = ;
    // deviceInfo[""] = ;


    Serial.print("defaultData before saving");
    Serial.println(defaultData);

    saveDataToEEPROM();

  }
  else {
    // take data from EEPROM
    
  }




  /* Check the number of elements in the "switches" array */
  // JsonArray switchesArray = doc["switches"].as<JsonArray>(); // Get the "switches" array
  // int numSwitches = switchesArray.size(); // Get the number of elements in the array
}


bool boolGetDeviceInfoFromEEPROM() {
  String savedDeviceInfo = "";
  char readChar;

  for (int i = 0; i < (int)capacity; i++) {
    readChar = EEPROM.read(i);
    savedDeviceInfo += readChar;
    // Serial.println(readChar);
  }

  JsonDocument tempDeviceInfo;

  // Parse the savedDeviceInfo JSON
  deserializeJson(tempDeviceInfo, savedDeviceInfo);

  if (isDebugOn) {
    Serial.print("savedDeviceInfo: ");
    Serial.println(savedDeviceInfo);
  }



  //   [].is<JsonVariant>() is deprecated since ArduinoJson 7.2 and you must use is<T>() instead.
  // // Before
  // if (doc[].is<JsonVariant>("value")) {
  //   int value = doc["value"];
  //   // ...
  // }

  // // After
  // if (doc["value"].is<int>()) {
  //   int value = doc["value"];
  //   // ...
  // }
  // If you want to check that a key exists regardless of its type, you can use is<JsonVariant>().
  // Check if the "device_type" field exists
  if (tempDeviceInfo["device_type"].is<JsonVariant>()) {
    deviceInfo = tempDeviceInfo;
    if (isDebugOn)
      Serial.println("device_type exists.");
      
    return true;
  } else {
    if (isDebugOn)
      Serial.println("device_type does not exist.");
    return false;
  }
}

bool boolSetDeviceInfoToEEPROM(char* data) {

  EEPROM.begin(EEPROM_SIZE);

  for (int i = 0; (i < capacity); i++) {
    // WriteTo_EEPROM(i, data[i]);
    EEPROM.write(i, data[i]);
  }

  EEPROM.commit();

  return true;

  /* How to save data to EEPROM */
  // char buffer[capacity];
  // // Serialize the JSON document to a string
  // serializeJsonPretty(deviceInfo, buffer, capacity);
  // boolSetDeviceInfoToEEPROM(buffer);
}


void saveDataToEEPROM(void) {
  char buffer[capacity];
  // Serialize the JSON document to a string
  serializeJsonPretty(deviceInfo, buffer, capacity);
  boolSetDeviceInfoToEEPROM(buffer);
}




//###################################################################
void Mega_Smart_init() {

  // Init RGB WS2812 or SK6812 ??
  // FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS);  // GRB ordering is typical
  // FastLED.addLeds<SK6812, DATA_PIN, RGB>(leds, NUM_LEDS);  // GRB ordering is typical
  FastLED.addLeds<WS2812, DATA_PIN, RGB>(leds, NUM_LEDS);  // GRB ordering is typical


  // Print WiFi info
  if (isDebugOn) {
    Serial.print("Fetched SSID :: ");
    Serial.println(deviceInfo["wifi_ssid"].as<String>());
    Serial.print("Fetched Pass :: ");
    Serial.println(deviceInfo["wifi_password"].as<String>());
  }

  // Init relays pins
  pinMode(RELAY0_PIN, OUTPUT);  // Initialize the relay0 with touch 0
  pinMode(RELAY1_PIN, OUTPUT);  // Initialize the relay1 with touch 1

  pinMode(LED_RGB_PIN, OUTPUT);  // Initialize the led for middle touch
  // pinMode(LED_INDICATOR_PIN, OUTPUT);  // Initialize the led for reer led

  // Init switches
  pinMode(TOUCH0_PIN, INPUT);  // Initialize the touch 2
  pinMode(TOUCH1_PIN, INPUT);  // Initialize the touch 3
  pinMode(TOUCH2_PIN, INPUT);  // Initialize the touch 1

  pinMode(FLASH_BUTTON_PIN, INPUT);  // Initialize the touch 1

  // Turn off led indicator
  ledIndicator.setLEDPattern(0, 1, 0, 1);
  ledIndicator.setCallback(updateIndicationLED);
  ledIndicator.enable();

  // turn off all relays and leds
  digitalWrite(RELAY0_PIN, !RELAY0_ACTIVE);
  touchSen_LastRead_0 = digitalRead(TOUCH0_PIN);
  deviceInfo["sw0"] = 0;

  digitalWrite(RELAY1_PIN, !RELAY1_ACTIVE);
  touchSen_LastRead_2 = digitalRead(TOUCH2_PIN);
  deviceInfo["sw2"] = 0;

  digitalWrite(LED_RGB_PIN, !LED_RGB_ACTIVE);
  touchSen_LastRead_1 = digitalRead(TOUCH1_PIN);
  deviceInfo["sw1"] = 0;

  // turn on indication led
  // digitalWrite(LED_INDICATOR_PIN, LED_INDICATOR_ACTIVE);
  // deviceInfo["led"] = 1;


  // RGB control
  if (deviceInfo["sw1"].as<bool>()) {
    setColor((uint8_t)deviceInfo["green"].as<int>(), (uint8_t)deviceInfo["red"].as<int>(), (uint8_t)deviceInfo["blue"].as<int>());
  } else {
    // FastLED.clearData();
    setColor(0, 0, 0);
  }


  // Init UDP
  Udp.begin(deviceInfo["listen_on_port"]);

  deviceInfo["mac_address"] = Get_Mac_Address();
}

void MonitorTouchSensors() {
  if (digitalRead(TOUCH2_PIN) != touchSen_LastRead_2 /* && digitalRead(TOUCH2_PIN) == TOUCH2_ACTIVE*/) {
    deviceInfo["sw2"] = deviceInfo["sw2"].as<bool>() ^ 1;
    touchSen_LastRead_2 = digitalRead(TOUCH2_PIN);
    digitalWrite(RELAY1_PIN, deviceInfo["sw2"].as<bool>() ? RELAY1_ACTIVE : !RELAY1_ACTIVE);

    deviceInfo["commands"] = "UPDATE_OK";
    tuya_notify_relay1(deviceInfo["sw2"].as<bool>());
    if (b_isAPStarted) {
      // update the specified device
      sendReplyToSender();
      sendReplyToAll();
    } else {
      // update all network
      sendReplyToAll();
      sendReplyToAll();
      sendReplyToAll();
    }
    checkResetPattern(1);
  } else {
    touchSen_LastRead_2 = digitalRead(TOUCH2_PIN);
  }

  if (digitalRead(TOUCH0_PIN) != touchSen_LastRead_0 /*&& digitalRead(TOUCH0_PIN) == TOUCH0_ACTIVE*/) {
    deviceInfo["sw0"] = deviceInfo["sw0"].as<bool>() ^ 1;
    touchSen_LastRead_0 = digitalRead(TOUCH0_PIN);
    digitalWrite(RELAY0_PIN, deviceInfo["sw0"].as<bool>() ? RELAY0_ACTIVE : !RELAY0_ACTIVE);
    deviceInfo["commands"] = "UPDATE_OK";
    tuya_notify_relay0(deviceInfo["sw0"].as<bool>());
    if (b_isAPStarted) {
      // update the specified device
      sendReplyToSender();
      sendReplyToAll();
    } else {
      // update all network
      sendReplyToAll();
      sendReplyToAll();
      sendReplyToAll();
    }
    checkResetPattern(2);
  } else {
    touchSen_LastRead_0 = digitalRead(TOUCH0_PIN);
  }

  if (digitalRead(TOUCH1_PIN) != touchSen_LastRead_1 /*&& digitalRead(TOUCH1_PIN) == TOUCH1_ACTIVE*/) {
    deviceInfo["sw1"] = deviceInfo["sw1"].as<bool>() ^ 1;
    touchSen_LastRead_1 = digitalRead(TOUCH1_PIN);

    // RGB control
    if (deviceInfo["sw1"].as<bool>()) {
      setColor((uint8_t)deviceInfo["green"].as<int>(), (uint8_t)deviceInfo["red"].as<int>(), (uint8_t)deviceInfo["blue"].as<int>());

    } else {
      // FastLED.clearData();
      setColor(0, 0, 0);
    }


    digitalWrite(LED_RGB_PIN, deviceInfo["sw1"].as<bool>() ? LED_RGB_ACTIVE : !LED_RGB_ACTIVE);
    deviceInfo["commands"] = "UPDATE_OK";
    tuya_notify_rgb_switch(deviceInfo["sw1"].as<bool>());
    if (b_isAPStarted) {
      // update the specified device
      sendReplyToSender();
      sendReplyToAll();
    } else {
      // update all network
      sendReplyToAll();
      sendReplyToAll();
      sendReplyToAll();
    }

    checkResetPattern(0);

  } else {
    touchSen_LastRead_1 = digitalRead(TOUCH1_PIN);
  }

  if (digitalRead(FLASH_BUTTON_PIN) != flashButton_LastRead && digitalRead(FLASH_BUTTON_PIN) == FLASH_BUTTON_ACTIVE) {

    flashButton_LastRead = digitalRead(FLASH_BUTTON_PIN);


    // deviceInfo["led"] = deviceInfo["led"].as<bool>() ^ 1;

    if (isDebugOn)
      Serial.printf("flashLed_status: %d ", deviceInfo["led"].as<bool>());

    // digitalWrite(LED_INDICATOR_PIN, deviceInfo["led"].as<bool>() ? LED_INDICATOR_ACTIVE : !LED_INDICATOR_ACTIVE);

    deviceInfo["commands"] = "UPDATE_OK";

    if (b_isAPStarted) {
      // update the specified device
      sendReplyToSender();
      sendReplyToAll();
    } else {
      // update all network
      sendReplyToAll();
      sendReplyToAll();
      sendReplyToAll();
    }

  } else {
    flashButton_LastRead = digitalRead(FLASH_BUTTON_PIN);
  }
}


void setColor(uint8_t redValue, uint8_t greenValue, uint8_t blueValue) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(redValue, greenValue, blueValue);
    FastLED.show();
  }
}

bool ConnectToWiFi(String ssid, String password, int waitCount, bool restartConnection = 1) {

  int i = 0;


  if ((ssid.length() < 1) || (password.length() < 8)) {

    if (isDebugOn) {
      Serial.print("\nIn ConnectToWiFi => can not Connect To: ");
      Serial.print(ssid);
      Serial.print(":");
      Serial.println(password);
    }

    return WIFI_NOT_CONNECTED;
  }


  if (restartConnection) {
    // Disconnect from an AP if it was previously connected
    Serial.print("Restart WiFi connection\n");
    WiFi.disconnect();
    delay(50);

    // if (scanned_SSID == ssid) {
    WiFi.begin(ssid, password);
  }

  i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    i++;
    delay(50);
    if (isDebugOn)
      Serial.print(".");


    currentConnectionStatusESP = ROUTER_CONNECTING;

    if (i > waitCount) {
      break;
    }
  }


  if (WiFi.status() == WL_CONNECTED) {

    currentConnectionStatusESP = ROUTER_CONNECTED;

    Serial.print("Connected To ");
    Serial.println(ssid);
    Serial.print("with local IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.printf("with port %d \n", deviceInfo["listen_on_port"].as<int>());
    Serial.printf("\nReceived from remote IPAddress %s:%d\n )\n", senderIP.toString().c_str(), deviceInfo["sending_to_port"].as<int>());

    return WIFI_CONNECTED;
  } else {
    currentConnectionStatusESP = ROUTER_NOT_CONNECTED;
    Serial.print("\nNot Connected To: ");
    Serial.print(ssid);
    Serial.print(":");
    Serial.println(password);


    return WIFI_NOT_CONNECTED;
  }
}


void WriteTo_EEPROM(int const address, uint8_t const val) {
  EEPROM.write(address, val);
  EEPROM.commit();
}



void ClearBuffer(char* buffer) {
  int i = 0;
  while (buffer[i] != 0) {
    buffer[i] = 0;
    i++;
  }
}

String Get_Mac_Address() {
  byte mac[6];
  WiFi.macAddress(mac);
  macAddress = "";
  // Concatenate the individual parts of the MAC address
  for (int i = 0; i < 6; ++i) {
    char macPart[3];
    sprintf(macPart, "%02X", mac[i]);
    macAddress += macPart;
    if (i < 5) {
      macAddress += ':';
    }
  }
  return macAddress;
}


void Connecting_To_Router_Or_Ap() {

  // Check if access point started
  if (!b_isAPStarted) {
    // Check if already connected
    if (WiFi.status() != WL_CONNECTED) {
      // if WiFi existed tries to connect
      if (ConnectToWiFi(deviceInfo["wifi_ssid"], deviceInfo["wifi_password"], WIFI_CONNEECTING_WAITING_COUNT) == WIFI_NOT_CONNECTED) {

        // For debugging if(Current_state!=Last_state)
        currentConnectionStatusESP = ROUTER_NOT_CONNECTED;
        if (currentConnectionStatusESP != Last_state) {
          Last_state = currentConnectionStatusESP;

          if (isDebugOn)
            Serial.print("The Connection to Router is Faild\n");
        }


        // Make sure to disconnect wifi
        WiFi.disconnect();

        // Delay for safety
        delay(50);

        // Start access point
        WiFi.softAP(AP_ssid, AP_password);

        // Check if any device connected to it
        if (WiFi.softAPgetStationNum() == 0) {
          currentConnectionStatusESP = AP_NOT_CONNECTED;
        } else {
          currentConnectionStatusESP = AP_CONNECTED;
        }


        b_isAPStarted = true;
        // Reset AP TimeOut
        ul_APTimeOut = millis();

        //setting Router State


      } else {
        //setting Router State
        currentConnectionStatusESP = ROUTER_CONNECTED;

        //Turn off AP
        WiFi.softAPdisconnect();

        if (currentConnectionStatusESP != Last_state) {
          Last_state = currentConnectionStatusESP;
          // Successed to conncet

          if (isDebugOn) {
            Serial.print("The Connection to Router is Succedded\n");
            Serial.print("with local esp IP Address: ");
            Serial.println(WiFi.localIP());
            Serial.printf("to Remote IP Address is %s with Port  %d ", senderIP.toString().c_str(), Udp.remotePort());
          }
        }
      }
    } else {
      //setting Router State
      currentConnectionStatusESP = ROUTER_CONNECTED;
      if (currentConnectionStatusESP != Last_state) {
        Last_state = currentConnectionStatusESP;
        // Already connected //
        if (isDebugOn)
          Serial.print("The Connection to Router is Already connected\n");
      }
    }
  } else {
    // Access point already started

    // Check if any device connected to it
    if (WiFi.softAPgetStationNum() == 0) {
      currentConnectionStatusESP = AP_NOT_CONNECTED;

      // No device connected
      if (currentConnectionStatusESP != Last_state) {
        Last_state = currentConnectionStatusESP;
        if (isDebugOn)
          Serial.print("The Esp can't find any network connected to it  \n");
      }


      /* Check for timeout */
      unsigned long tempTime = millis();

      if (((tempTime - ul_APTimeOut) / 1000) >= AP_TIMEOUT_SEC_MAX) {

        /* Disable access point */
        b_isAPStarted = false;

        //Turn off AP
        WiFi.softAPdisconnect();
      }
    } else {
      currentConnectionStatusESP = AP_CONNECTED;
      if (currentConnectionStatusESP != Last_state) {
        Last_state = currentConnectionStatusESP;
        // There is a esp connect ed to something
        if (isDebugOn)
          Serial.printf("The Esp connected succesfuly with its AP IP Address %s :%d \n", WiFi.softAPIP().toString().c_str(), deviceInfo["listen_on_port"].as<int>());
      }
    }
  }
}

void void_connectToRouter() {

  static int connectingTimeOut = 30;
  static bool restartConnection = 0;
  // Check if access point started
  if (!b_isAPStarted) {
    // Check if already connected
    if (((String)deviceInfo["wifi_ssid"]).length() < 1 || ((String)deviceInfo["wifi_password"]).length() < 8) {

      if (isDebugOn){
        Serial.print("\nIn void_connectToRouter => can not Connect To: ");
        Serial.print((String)deviceInfo["wifi_ssid"]);
        Serial.print(":");
        Serial.println((String)deviceInfo["wifi_password"]);
      }

      // Make sure to disconnect wifi
      WiFi.disconnect();

      // Delay for safety
      delay(50);

      // Start access point
      WiFi.softAP(AP_ssid, AP_password);

      // Check if any device connected to it
      if (WiFi.softAPgetStationNum() == 0) {
        currentConnectionStatusESP = AP_NOT_CONNECTED;
      } else {
        currentConnectionStatusESP = AP_CONNECTED;
      }


      b_isAPStarted = true;
      // Reset AP TimeOut
      ul_APTimeOut = millis();
    } 
    else if (WiFi.status() != WL_CONNECTED) {
      connectingTimeOut++;

      if (connectingTimeOut >= 30) {
        connectingTimeOut = 0;
        restartConnection = 1;
      } else {
        restartConnection = 0;
      }

      if (ConnectToWiFi(deviceInfo["wifi_ssid"], deviceInfo["wifi_password"], 10, restartConnection) == WIFI_NOT_CONNECTED) {

        // For debugging if(Current_state!=Last_state)
        // currentConnectionStatusESP = ROUTER_NOT_CONNECTED;
        // if (currentConnectionStatusESP != Last_state) {
        //   Last_state = currentConnectionStatusESP;

        if (isDebugOn)
          Serial.print("The Connection to Router Failed\n");
        // }


      } else {
        //setting Router State
        currentConnectionStatusESP = ROUTER_CONNECTED;

        //Turn off AP
        WiFi.softAPdisconnect();

        if (currentConnectionStatusESP != Last_state) {
          Last_state = currentConnectionStatusESP;
          // Successed to conncet

          if (isDebugOn) {
            Serial.print("The Connection to Router is Succedded\n");
            Serial.print("with local esp IP Address: ");
            Serial.println(WiFi.localIP());
            Serial.printf("to Remote IP Address is %s with Port  %d ", senderIP.toString().c_str(), Udp.remotePort());
          }
        }
      }
    } else {
      //setting Router State
      currentConnectionStatusESP = ROUTER_CONNECTED;
      if (currentConnectionStatusESP != Last_state) {
        Last_state = currentConnectionStatusESP;
        // Already connected //
        if (isDebugOn)
          Serial.print("The Connection to Router is Already connected\n");
      }
    }
  } else {
    // Access point already started

    // Check if any device connected to it
    if (WiFi.softAPgetStationNum() == 0) {
      currentConnectionStatusESP = AP_NOT_CONNECTED;

      // No device connected
      if (currentConnectionStatusESP != Last_state) {
        Last_state = currentConnectionStatusESP;
        if (isDebugOn)
          Serial.print("The Esp can't find any network connected to it  \n");
      }


      /* Check for timeout */
      unsigned long tempTime = millis();

      if (((tempTime - ul_APTimeOut) / 1000) >= AP_TIMEOUT_SEC_MAX) {

        /* Disable access point */
        b_isAPStarted = false;

        //Turn off AP
        WiFi.softAPdisconnect();
      }
    } else {
      currentConnectionStatusESP = AP_CONNECTED;
      if (currentConnectionStatusESP != Last_state) {
        Last_state = currentConnectionStatusESP;
        // There is a esp connect ed to something
        if (isDebugOn)
          Serial.printf("The Esp connected succesfuly with its AP IP Address %s :%d \n", WiFi.softAPIP().toString().c_str(), deviceInfo["listen_on_port"].as<int>());
      }
    }
  }
}


// Function to update the LED pattern based on the current WiFi status
void updateIndicationLED() {

  if (deviceInfo["led"].as<bool>()) {

    switch (currentConnectionStatusESP) {
      case ROUTER_CONNECTED:
        ledIndicator.setLEDPattern(1, 20, 1, 20);
        break;
      case AP_CONNECTED:
        ledIndicator.setLEDPattern(1, 0, 1, 0);  // LED on
        break;
      case ROUTER_CONNECTING:
      case ROUTER_NOT_CONNECTED:
        ledIndicator.setLEDPattern(1, 1, 1, 20);
        break;
      case AP_NOT_CONNECTED:
        ledIndicator.setLEDPattern(5, 5, 5, 5);  // Slow blinking
        break;
      default:
        ledIndicator.setLEDPattern(0, 1, 0, 1);  // LED off
        break;
    }

  } else {
    ledIndicator.setLEDPattern(0, 1, 0, 1);  // LED off
  }
}






// Check reset button
void secCounter() {

  gSecCounter++;

  if (digitalRead(FLASH_BUTTON_PIN) == FLASH_BUTTON_ACTIVE) {
    gResetSecCounter++;
  }

  // GetData_And_TakeAction();

  AutoStatusUpdate();
}

void sendOKToSender(void) {
  int tries = 0;

  // Send confirmation message.
  ReplyBuffer.clear();
  ReplyBuffer = "OK";

  if (isDebugOn)
    Serial.printf("\nSend to IP %s:%d, OK\n", senderIP.toString().c_str(), deviceInfo["sending_to_port"].as<int>());

  tries = 0;
  while (!Udp.beginPacket(senderIP, deviceInfo["sending_to_port"])) {
    if (++tries >= 3) {
      if (isDebugOn)
        Serial.printf("Error in begin \n");
      break;
    }
  }

  Udp.write(ReplyBuffer.c_str());

  tries = 0;
  while (!Udp.endPacket()) {
    if (++tries >= 3) {
      if (isDebugOn)
        Serial.printf("Error in end\n");
      break;
    }
  }
}

void sendReplyToAll(void) {
  int tries = 0;

  ReplyBuffer.clear();
  serializeJsonPretty(deviceInfo, ReplyBuffer);
  if (isDebugOn)
    Serial.printf("\nSend to IP 255.255.255.255:%d, command: %s\n", deviceInfo["sending_to_port"].as<int>(), deviceInfo["commands"].as<const char*>());

  tries = 0;
  while (!Udp.beginPacket("255.255.255.255", deviceInfo["sending_to_port"])) {
    if (++tries >= 3) {
      if (isDebugOn)
        Serial.printf("Error in begin \n");
      break;
    }
  }

  Udp.write(ReplyBuffer.c_str());

  tries = 0;
  while (!Udp.endPacket()) {
    if (++tries >= 3) {
      if (isDebugOn)
        Serial.printf("Error in end\n");
      break;
    }
  }
}

void sendReplyToSender(void) {
  int tries = 0;
  ReplyBuffer.clear();
  serializeJsonPretty(deviceInfo, ReplyBuffer);

  if (isDebugOn)
    Serial.printf("\nSend to IP %s:%d, command: %s\n", senderIP.toString().c_str(), deviceInfo["sending_to_port"].as<int>(), deviceInfo["commands"].as<const char*>());


  tries = 0;
  while (!Udp.beginPacket(senderIP, deviceInfo["sending_to_port"])) {
    if (++tries >= 3) {
      if (isDebugOn)
        Serial.printf("Error in begin \n");
      break;
    }
  }

  Udp.write(ReplyBuffer.c_str());

  tries = 0;
  while (!Udp.endPacket()) {
    if (++tries >= 3) {
      if (isDebugOn)
        Serial.printf("Error in end\n");
      break;
    }
  }
}

void checkResetProcedure() {
  if (gResetSecCounter > 9) {
    gResetSecCounter = 0;

    void_resetSwitch();
  }
}


void void_resetSwitch(void) {
  deviceInfo["led"] = 1;

  if (isDebugOn)
    Serial.println("Resetting AP");

  // Make sure to disconnect wifi
  WiFi.disconnect();

  // Delay for safety
  delay(50);

  // Start access point
  WiFi.softAP(AP_ssid, AP_password);

  // Check if any device connected to it
  if (WiFi.softAPgetStationNum() == 0) {
    currentConnectionStatusESP = AP_NOT_CONNECTED;
  } else {
    currentConnectionStatusESP = AP_CONNECTED;
  }


  b_isAPStarted = true;
  // Reset AP TimeOut
  ul_APTimeOut = millis();
}

void GetData_And_TakeAction() {
  int packetSize = Udp.parsePacket();
  if (packetSize) {

    unsigned long start = millis();  // Get the start time in milliseconds

    senderIP = Udp.remoteIP();

    // Serial.printf("Received packet of size %d from %s:%d\n    (to %s:%d, free heap = %d B)\n", packetSize, senderIP.toString().c_str(), Udp.remotePort(), Udp.destinationIP().toString().c_str(), Udp.localPort(), ESP.getFreeHeap());

    ClearBuffer(packetBuffer);

    // read the packet into packetBufffer
    Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);

    if (isDebugOn) {
      Serial.print("Contents:: ");
      Serial.println(packetBuffer);
    }

    // Parse the packetBuffer JSON
    deserializeJson(receivedInfo, packetBuffer);




    if (receivedInfo["mac_address"].is<JsonVariant>()) {
      if (isDebugOn)
        Serial.println("1");

      if ((receivedInfo["mac_address"] == macAddress) || (receivedInfo["mac_address"] == "HH:HH:HH:HH:HH:HH")) {

        if (isDebugOn)
          Serial.println("2");

        // Send confirmation message.
        sendOKToSender();
        // senderIP = Udp.remoteIP();



        if (receivedInfo["commands"].is<JsonVariant>()) {
          if (isDebugOn)
            Serial.println("3");
          takeAction(receivedInfo["commands"]);
        }
      } else if (receivedInfo["commands"].is<JsonVariant>()) {
        if (isDebugOn)
          Serial.println("4");

        if (receivedInfo["commands"] == "MAC_ADDRESS_READ") {

          // Send confirmation message.
          sendOKToSender();
          // senderIP = Udp.remoteIP();

          if (isDebugOn)
            Serial.println("5");
          takeAction(receivedInfo["commands"]);
        }
      }
    } else if (receivedInfo["commands"].is<JsonVariant>()) {
      if (isDebugOn)
        Serial.println("6");


      if (receivedInfo["commands"] == "MAC_ADDRESS_READ") {

        // Send confirmation message.
        sendOKToSender();
        // senderIP = Udp.remoteIP();

        if (isDebugOn)
          Serial.println("7");
        takeAction(receivedInfo["commands"]);
      }
    }

    unsigned long end = millis();  // Get the end time in milliseconds
    if (isDebugOn) {
      Serial.print("Function execution time: ");
      Serial.print(end - start);  // Print the elapsed time
      Serial.println(" milliseconds");
    }
  }
  // Udp.begin(deviceInfo["listen_on_port"]);
}

String payload;
int counter = 0;
void takeAction(String cmd) {

  // Udp.stop();
  // Udp.begin(deviceInfo["listen_on_port"]);

  if (cmd == "WIFI_CONFIG") {


    //###################### Connecting To WiFi #######################
    if (receivedInfo["wifi_ssid"].is<JsonVariant>() && receivedInfo["wifi_password"].is<JsonVariant>()) {

      if (receivedInfo["wifi_ssid"] == deviceInfo["wifi_ssid"] && receivedInfo["wifi_password"] == deviceInfo["wifi_password"]) {
        // Send rejection message
        deviceInfo["commands"] = "WIFI_CONFIG_SAME";
        sendReplyToSender();
      } else {
      }

      // Send connecting message
      deviceInfo["commands"] = "WIFI_CONFIG_CONNECTING";
      sendReplyToSender();
      delay(1000);

      if (ConnectToWiFi(receivedInfo["wifi_ssid"], receivedInfo["wifi_password"], WIFI_CONNEECTING_WAITING_COUNT) == WIFI_CONNECTED) {

        // save new WiFi
        deviceInfo["wifi_ssid"] = receivedInfo["wifi_ssid"];
        deviceInfo["wifi_password"] = receivedInfo["wifi_password"];

        saveDataToEEPROM();


        // Send done message (no need)
        deviceInfo["commands"] = "WIFI_CONFIG_OK";
        sendReplyToSender();

        //Turn off AP
        WiFi.softAPdisconnect();
        if (isDebugOn) {
          Serial.printf("Succefuly connected with Remote IP Address is %s with Port  %d ", senderIP.toString().c_str(), Udp.remotePort());
        }
      } else {

        //------------------------- Remain In Access Point------------------
        deviceInfo["commands"] = "WIFI_CONFIG_FAILED";
        sendReplyToSender();
      }


    } else {
      // Send rejection message
      deviceInfo["commands"] = "WIFI_CONFIG_MISSED_DATA";
      sendReplyToSender();
    }
    //#################################################################

  } else if (cmd == "MAC_ADDRESS_READ") {

    // Send MAC_ADDRESS
    deviceInfo["commands"] = "MAC_ADDRESS_READ_OK";
    sendReplyToSender();

  } else if (cmd == "WIFI_CONNECT_CHECK") {
    // Sent Ok before is enough
    deviceInfo["commands"] = "WIFI_CONNECT_CHECK_OK";
    sendReplyToSender();

  } else if (cmd == "STATUS_READ" || cmd == "SWITCH_READ") {

    // Send Switches status
    ReplyBuffer.clear();

    if (cmd == "STATUS_READ") {
      deviceInfo["commands"] = "STATUS_READ_OK";
    }

    if (cmd == "SWITCH_READ") {
      deviceInfo["commands"] = "SWITCH_READ_OK";
    }

    sendReplyToSender();

  } else if (cmd == "STATUS_WRITE" || cmd == "SWITCH_WRITE") {

    if (receivedInfo["sw0"].is<JsonVariant>()) {
      deviceInfo["sw0"] = receivedInfo["sw0"];
      digitalWrite(RELAY0_PIN, deviceInfo["sw0"].as<bool>() ? RELAY0_ACTIVE : !RELAY0_ACTIVE);
    }

    if (receivedInfo["sw1"].is<JsonVariant>()) {
      deviceInfo["sw1"] = receivedInfo["sw1"];
      digitalWrite(LED_RGB_PIN, deviceInfo["sw1"].as<bool>() ? LED_RGB_ACTIVE : !LED_RGB_ACTIVE);
      // RGB control
      if (deviceInfo["sw1"].as<bool>()) {
        setColor((uint8_t)deviceInfo["green"].as<int>(), (uint8_t)deviceInfo["red"].as<int>(), (uint8_t)deviceInfo["blue"].as<int>());
      } else {
        // FastLED.clearData();
        setColor(0, 0, 0);
      }
    }

    if (receivedInfo["sw2"].is<JsonVariant>()) {
      deviceInfo["sw2"] = receivedInfo["sw2"];
      digitalWrite(RELAY1_PIN, deviceInfo["sw2"].as<bool>() ? RELAY1_ACTIVE : !RELAY1_ACTIVE);
    }

    if (receivedInfo["led"].is<JsonVariant>()) {
      deviceInfo["led"] = receivedInfo["led"];
      saveDataToEEPROM();
      // digitalWrite(LED_INDICATOR_PIN, deviceInfo["led"].as<bool>() ? LED_INDICATOR_ACTIVE : !LED_INDICATOR_ACTIVE);
    }

    // Send Switches status
    ReplyBuffer.clear();

    if (cmd == "STATUS_WRITE") {
      deviceInfo["commands"] = "STATUS_WRITE_OK";
    }

    if (cmd == "SWITCH_WRITE") {
      deviceInfo["commands"] = "SWITCH_WRITE_OK";
    }


    if (b_isAPStarted) {
      // update the specified device
      sendReplyToSender();
    } else {
      // update all network
      sendReplyToAll();
      sendReplyToAll();
      sendReplyToAll();
    }


  } else if (cmd == "RGB_READ") {

    // Send RGB status
    deviceInfo["commands"] = "RGB_READ_OK";
    if (isDebugOn) {
      Serial.printf("red:%d  green:%d  blue:%d  \n", deviceInfo["red"].as<int>(), deviceInfo["green"].as<int>(), deviceInfo["blue"].as<int>());
    }

    sendReplyToSender();

  } else if (cmd == "RGB_WRITE") {

    if (receivedInfo["blue"].is<JsonVariant>() && receivedInfo["green"].is<JsonVariant>() && receivedInfo["red"].is<JsonVariant>()) {
      deviceInfo["green"] = receivedInfo["green"];
      deviceInfo["red"] = receivedInfo["red"];
      deviceInfo["blue"] = receivedInfo["blue"];

      // RGB control
      if (deviceInfo["sw1"].as<bool>()) {
        setColor((uint8_t)deviceInfo["green"].as<int>(), (uint8_t)deviceInfo["red"].as<int>(), (uint8_t)deviceInfo["blue"].as<int>());
      } else {
        // FastLED.clearData();
        setColor(0, 0, 0);
      }

      saveDataToEEPROM();

      // Send RGB new status
      deviceInfo["commands"] = "RGB_WRITE_OK";
      if (isDebugOn) {
        Serial.printf("\n%d  %d  %d  \n", deviceInfo["red"].as<int>(), deviceInfo["green"].as<int>(), deviceInfo["blue"].as<int>());
      }

      if (b_isAPStarted) {
        // update the specified device
        sendReplyToSender();

      } else {
        // update all network
        sendReplyToAll();
        sendReplyToAll();
        sendReplyToAll();
      }
    }

  } else if (cmd == "DEVICE_CONFIG_WRITE") {

    if (receivedInfo["device_location"].is<JsonVariant>()) {
      deviceInfo["device_location"] = receivedInfo["device_location"];
    }
    if (receivedInfo["server_port"].is<JsonVariant>() && receivedInfo["server_ip"].is<JsonVariant>()) {
      deviceInfo["server_port"] = receivedInfo["server_port"];
      deviceInfo["server_ip"] = receivedInfo["server_ip"];
    }

    if (receivedInfo["listen_on_port"].is<JsonVariant>()) {
      deviceInfo["listen_on_port"] = receivedInfo["listen_on_port"];
    }
    if (receivedInfo["sending_to_port"].is<JsonVariant>()) {
      deviceInfo["sending_to_port"] = receivedInfo["sending_to_port"];
    }

    if (receivedInfo["update_period"].is<JsonVariant>()) {
      deviceInfo["update_period"] = receivedInfo["update_period"];
    }

    saveDataToEEPROM();

    // Send RGB new status
    deviceInfo["commands"] = "DEVICE_CONFIG_WRITE_OK";

    sendReplyToSender();

  } else if (cmd == "READ") {
    deviceInfo["commands"] = "READ_OK";
    sendReplyToSender();
  } else if (cmd == "CHECK_FOR_NEW_FIRMWARE") {

    WiFiClientSecure wifiClient;  // Create a WiFiClient object
    // Get the latest firmware file name

    wifiClient.setInsecure();
    HTTPClient http;
    http.begin(wifiClient, firmwareVersionLink);  // Use the WiFiClient object here
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();  // Get the full JSON response
      if (isDebugOn) {
        Serial.println("Received: " + payload);
      }

      if (payload != deviceInfo["firmware_version"]) {

        deviceInfo["commands"] = "CHECK_FOR_NEW_FIRMWARE_OK";
      } else if (payload == deviceInfo["firmware_version"]) {
        deviceInfo["commands"] = "CHECK_FOR_NEW_FIRMWARE_SAME";
      }

    } else {
      if (isDebugOn) {
        Serial.println("Failed to fetch the latest firmware file name.");
      }
      deviceInfo["commands"] = "CHECK_FOR_NEW_FIRMWARE_FAIL";
    }

    http.end();

    sendReplyToSender();


  } else if (cmd == "DOWNLOAD_NEW_FIRMWARE") {

    WiFiClientSecure wifiClient;  // Create a WiFiClient object
    // Get the latest firmware file name

    wifiClient.setInsecure();

    HTTPClient http;
    http.begin(wifiClient, firmwareVersionLink);  // Use the WiFiClient object here
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      payload = http.getString();  // Get the full JSON response
      http.end();
      if (isDebugOn) {
        Serial.println("Received: " + payload);
      }

      if (payload != deviceInfo["firmware_version"]) {

        // Disable autoupdate after downloading new code
        ESPhttpUpdate.rebootOnUpdate(false);

        ESPhttpUpdate.onStart(onStartOfUpdate);
        ESPhttpUpdate.onProgress(onProgressOfUpdate);
        ESPhttpUpdate.onEnd(onEndOfUpdate);

        // Create HTTPClient for OTA update
        HTTPClient httpClient;

        httpClient.begin(wifiClient, firmwareDownloadLink);  // Use the WiFiClient object here


        counter = 0;


        // Perform OTA update using the HTTPClient object
        t_httpUpdate_return ret = ESPhttpUpdate.update(httpClient);

        httpClient.end();
        switch (ret) {
          case HTTP_UPDATE_OK:
            if (isDebugOn) {
              Serial.println("Update success!");
            }
            deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_OK";
            // deviceInfo["firmware_version"] = payload;
            // saveDataToEEPROM();
            sendReplyToSender();
            sendReplyToAll();
            sendReplyToAll();
            sendReplyToAll();

            delay(2000);
            ESP.restart();

            // Serial.println("Update success2!");
            // delay(10000);
            break;
          case HTTP_UPDATE_FAILED:
            deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_FAIL";
            // Init UDP
            // Udp.stop();
            // while (!Udp.begin(deviceInfo["listen_on_port"])) {
            //   Serial.println("Try to begin listening after failure in updating ota!");
            // }
            // delay(1000);
            sendReplyToSender();
            sendReplyToAll();
            sendReplyToAll();
            sendReplyToAll();
            if (isDebugOn) {
              Serial.printf("Update failed. Error1 (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
            }
            break;

          case HTTP_UPDATE_NO_UPDATES:
            deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_FAIL";
            // Init UDP
            // Udp.stop();
            // while (!Udp.begin(deviceInfo["listen_on_port"])) {
            //   Serial.println("Try to begin listening after failure in updating ota!");
            // }
            // delay(1000);
            sendReplyToSender();
            sendReplyToAll();
            sendReplyToAll();
            sendReplyToAll();
            if (isDebugOn) {
              Serial.printf("Update failed. Error2 (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
            }
            break;
        }

        // Ensure that the HTTPClient is properly ended
        httpClient.end();  // Clean up after the OTA update

      } else {
        deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_SAME";
        sendReplyToSender();
        sendReplyToAll();
        sendReplyToAll();
        sendReplyToAll();
      }
    } else {
      http.end();
      if (isDebugOn) {
        Serial.println("Failed to fetch the latest firmware file name.");
      }
      deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_FAIL";
      sendReplyToSender();
      sendReplyToAll();
      sendReplyToAll();
      sendReplyToAll();
    }


  } else if (cmd == "RESTART") {

    // Send MAC_ADDRESS
    deviceInfo["commands"] = "RESTART_OK";
    sendReplyToAll();
    sendReplyToAll();
    sendReplyToAll();


    delay(2000);
    ESP.restart();
  } else if (cmd == "DEBUG") {

    if (receivedInfo["is_debug_on"].is<JsonVariant>()) {

      if (receivedInfo["is_debug_on"].as<bool>()) {
        isDebugOn = 1;
        deviceInfo["commands"] = "DEBUG_ON";
      } else {
        isDebugOn = 0;
        deviceInfo["commands"] = "DEBUG_OFF";
      }


      sendReplyToAll();
      sendReplyToAll();
      sendReplyToAll();
    }
  }
}


void onStartOfUpdate(void) {

  deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_START";
  // Init UDP
  Udp.stop();
  while (!Udp.begin(deviceInfo["listen_on_port"])) {
    if (isDebugOn) {
      Serial.println("Try to begin listening after failure in updating ota!");
    }
  }

  if (isDebugOn) {
    Serial.println("DOWNLOAD_NEW_FIRMWARE_START");
  }

  sendReplyToSender();
  sendReplyToSender();
  sendReplyToSender();

  sendReplyToAll();
  sendReplyToAll();
  sendReplyToAll();
}


void onProgressOfUpdate(int i1, int i2) {

  counter++;

  if (counter % 20 == 0) {

    deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_UPDATING_" + String(i1) + "_" + String(i2);
    // deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_UPDATING_";

    sendReplyToSender();
    sendReplyToAll();

  } else if (i1 == i2) {

    deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_UPDATING_" + String(i1) + "_" + String(i2);
    // deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_UPDATING_";

    sendReplyToSender();
    sendReplyToAll();
    sendReplyToAll();
    sendReplyToAll();
  }

  if (isDebugOn) {
    Serial.printf("counter: %d, i1: %d, i2: %d \n", counter, i1, i2);
  }
}

void onEndOfUpdate(void) {
  // firmware_downloadingStatus = 2;
  // Serial.println("Update success1!");
  deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_OK";
  deviceInfo["firmware_version"] = payload;
  saveDataToEEPROM();
  // Init UDP
  Udp.stop();
  Udp.begin(deviceInfo["listen_on_port"]);
  sendReplyToSender();
  sendReplyToAll();
  sendReplyToAll();
  sendReplyToAll();

  if (isDebugOn) {
    Serial.println("Update success!");
  }
  // delay(10000);
  delay(2000);
  ESP.restart();
}

void onErrorUpdate(void) {

  deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_FAIL";

  // Init UDP
  Udp.stop();
  while (!Udp.begin(deviceInfo["listen_on_port"])) {
    if (isDebugOn) {
      Serial.println("Try to begin listening after failure in updating ota!");
    }
  }
  sendReplyToSender();
  sendReplyToAll();
  sendReplyToAll();
  sendReplyToAll();

  if (isDebugOn) {
    Serial.println("DOWNLOAD_NEW_FIRMWARE_FAIL in call back!");
  }
}


void AutoStatusUpdate(void) {

  if (((gSecCounter % deviceInfo["update_period"].as<int>()) == 0) && (deviceInfo["update_period"].as<int>() != 0)) {

    // RGB control
    if (deviceInfo["sw1"].as<bool>()) {
      // setColor(0, 0, 0);
      setColor((uint8_t)deviceInfo["green"].as<int>(), (uint8_t)deviceInfo["red"].as<int>(), (uint8_t)deviceInfo["blue"].as<int>());
    } else {
      // FastLED.clearData();
      setColor(0, 0, 0);
    }

    deviceInfo["commands"] = "UPDATE_OK";

    if (b_isAPStarted) {
      // update the specified device
      sendReplyToSender();
    } else {
      // update all network
      sendReplyToAll();
    }
  }
}

String generateRandomHexString(uint8_t bytesLength) {
  const char hexChars[] = "0123456789ABCDEF";
  String result = "";

  randomSeed(micros());  // Seed randomness from an analog pin

  uint8_t tMac[6];
  WiFi.macAddress(tMac);  // Get the NodeMCU MAC address


  // Convert MAC address to hex string without colons
  for (int i = 0; i < 6; i++) {
    if (tMac[i] < 16) result += '0';  // Ensure two-digit format
    result += String(tMac[i], HEX);
  }

  result.toUpperCase();  // Ensure uppercase hex

  // Complete to 10 bytes (20 hex characters) with random values
  while (result.length() < (bytesLength * 2)) {
    result += hexChars[random(16)];
  }

  return result;
}

String generateRandomToken() {
  return generateRandomHexString(10);
}

void checkResetPattern(uint8_t pos) {

  static int tapCount = 0;
  static unsigned long lastTapTime = 0;
  static uint8_t currentStage = 0;

  if (millis() - lastTapTime < 1000) {  // Check if tap is within 700ms

    if (pos == 0) {
      tapCount++;
    }


    Serial.printf("tapCount: %d\n", tapCount);


    if ((tapCount >= 5) && (currentStage == 0) && (pos == 0)) {  // Number of taps to detect
      currentStage = 1;
      Serial.println("Resetting: go to Stage = 1");
    } else if ((currentStage == 1) && (pos == 1)) {
      currentStage = 2;
      Serial.println("Resetting: go to Stage = 2");
    } else if ((currentStage == 2) && (pos == 2)) {
      Serial.println("Resetting: go to Stage = 3 and resetting");
      Serial.print("New token: ");
      Serial.println(generateRandomToken());

      // void_resetSwitch();
      //factoryReset();
      gResetSecCounter = 10;
    } else {
      // start over or ignore?
    }

  } else {
    tapCount = 1;  // Reset counter if too much time passed
    currentStage = 0;
  }

  lastTapTime = millis();
}
