#include "WString.h"
// ESP32 (Arduino-ESP32) build
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

#include <WiFiUdp.h>
#include <Preferences.h>
static Preferences _prefs;
#include <FastLED.h>
#include "Mega_Smart_Config.h"
#include "Mega_Smart_Interface.h"
#include "Mega_Smart_Private.h"
#include <ArduinoJson.h>
#include <Ticker.h>
#include "LEDIndicator.h"


#ifndef UDP_TX_PACKET_MAX_SIZE
#define UDP_TX_PACKET_MAX_SIZE 512
#endif



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

// Set by the touch/button handlers; loop() sends the network update (never from a timer)
volatile bool g_notifyPending = false;
// Set by the tap-pattern reset gesture; consumed in checkResetProcedure()
volatile bool g_forceReset = false;


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


const String defaultData = "{\"device_type\":\"switch\",\"mac_address\":\"00:11:22:33:44:55\",\"device_location\":\"Living Room\",\"firmware_version\":\"1.1.0\",\"wifi_ssid\":\"\",\"wifi_password\":\"\",\"server_ip\":\"server_ip\",\"server_port\":1234,\"sending_to_port\":8081,\"listen_on_port\":8888,\"commands\":\"command\",\"sw0\":0,\"sw1\":0,\"sw2\":0,\"led\":1,\"red\":255,\"green\":255,\"blue\":255,\"update_period\":10}";

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


    Serial.print("defaultData before saving");
    Serial.println(defaultData);

    saveDataToEEPROM();

  }
  else {
    // take data from EEPROM
    
  }
}


bool boolGetDeviceInfoFromEEPROM() {
  _prefs.begin("mega", true);   // read-only
  String savedDeviceInfo = _prefs.getString("cfg", "");
  _prefs.end();

  JsonDocument tempDeviceInfo;
  deserializeJson(tempDeviceInfo, savedDeviceInfo);

  if (isDebugOn) {
    Serial.print("savedDeviceInfo: ");
    Serial.println(savedDeviceInfo);
  }

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
  _prefs.begin("mega", false);  // read-write
  _prefs.putString("cfg", data);
  _prefs.end();
  return true;
}


void saveDataToEEPROM(void) {
  char buffer[capacity];
  serializeJsonPretty(deviceInfo, buffer, capacity);
  boolSetDeviceInfoToEEPROM(buffer);
}


//###################################################################
void Mega_Smart_init() {

  // FastLED temporarily disabled on ESP32-S3 to avoid RMT DMA cache crash (esp_cache_msync)
  // FastLED.addLeds<WS2812, DATA_PIN, RGB>(leds, NUM_LEDS);

  // Print WiFi info
  if (isDebugOn) {
    Serial.print("Fetched SSID :: ");
    Serial.println(deviceInfo["wifi_ssid"].as<String>());
    Serial.print("Fetched Pass length :: ");
    Serial.println(deviceInfo["wifi_password"].as<String>().length());
  }

  // Init relays pins
  pinMode(RELAY0_PIN, OUTPUT);  
  pinMode(RELAY1_PIN, OUTPUT);  

  pinMode(LED_RGB_PIN, OUTPUT);  

  // Init switches (keep GPIO 13 and 14 as test LED outputs)
  pinMode(TOUCH0_PIN, INPUT);  
  pinMode(TEST_LED0_PIN, OUTPUT);
  digitalWrite(TEST_LED0_PIN, HIGH);
  pinMode(TEST_LED1_PIN, OUTPUT);
  digitalWrite(TEST_LED1_PIN, HIGH);

  pinMode(FLASH_BUTTON_PIN, INPUT);  

  // Turn off led indicator
  ledIndicator.setLEDPattern(0, 1, 0, 1);
  ledIndicator.setCallback(updateIndicationLED);
  ledIndicator.enable();

  // turn off all relays and leds
  digitalWrite(RELAY0_PIN, !RELAY0_ACTIVE);
  touchSen_LastRead_0 = digitalRead(TOUCH0_PIN);
  deviceInfo["sw0"] = 0;

  digitalWrite(RELAY1_PIN, !RELAY1_ACTIVE);
  touchSen_LastRead_2 = 1;
  deviceInfo["sw2"] = 0;

  digitalWrite(LED_RGB_PIN, !LED_RGB_ACTIVE);
  touchSen_LastRead_1 = 1;
  deviceInfo["sw1"] = 0;

  // RGB control
  if (deviceInfo["sw1"].as<bool>()) {
    setColor((uint8_t)deviceInfo["green"].as<int>(), (uint8_t)deviceInfo["red"].as<int>(), (uint8_t)deviceInfo["blue"].as<int>());
  } else {
    setColor(0, 0, 0);
  }

  // NOTE: UDP is initialized dynamically by ensureUdpStarted() AFTER Wi-Fi / AP starts.
  // Calling Udp.begin() before Wi-Fi starts triggers FreeRTOS assert on ESP32!

  deviceInfo["mac_address"] = Get_Mac_Address();
  if (isDebugOn) {
    Serial.print("MAC Address: ");
    Serial.println(deviceInfo["mac_address"].as<String>());
    Serial.println("Mega_Smart_init complete!");
  }
}

void MonitorTouchSensors() {
  if (digitalRead(TOUCH2_PIN) != touchSen_LastRead_2) {
    deviceInfo["sw2"] = deviceInfo["sw2"].as<bool>() ^ 1;
    touchSen_LastRead_2 = digitalRead(TOUCH2_PIN);
    digitalWrite(RELAY1_PIN, deviceInfo["sw2"].as<bool>() ? RELAY1_ACTIVE : !RELAY1_ACTIVE);

    g_notifyPending = true;
    checkResetPattern(1);
  } else {
    touchSen_LastRead_2 = digitalRead(TOUCH2_PIN);
  }

  if (digitalRead(TOUCH0_PIN) != touchSen_LastRead_0) {
    deviceInfo["sw0"] = deviceInfo["sw0"].as<bool>() ^ 1;
    touchSen_LastRead_0 = digitalRead(TOUCH0_PIN);
    digitalWrite(RELAY0_PIN, deviceInfo["sw0"].as<bool>() ? RELAY0_ACTIVE : !RELAY0_ACTIVE);
    g_notifyPending = true;
    checkResetPattern(2);
  } else {
    touchSen_LastRead_0 = digitalRead(TOUCH0_PIN);
  }

  if (digitalRead(TOUCH1_PIN) != touchSen_LastRead_1) {
    deviceInfo["sw1"] = deviceInfo["sw1"].as<bool>() ^ 1;
    touchSen_LastRead_1 = digitalRead(TOUCH1_PIN);

    // RGB control
    if (deviceInfo["sw1"].as<bool>()) {
      setColor((uint8_t)deviceInfo["green"].as<int>(), (uint8_t)deviceInfo["red"].as<int>(), (uint8_t)deviceInfo["blue"].as<int>());
    } else {
      setColor(0, 0, 0);
    }

    digitalWrite(LED_RGB_PIN, deviceInfo["sw1"].as<bool>() ? LED_RGB_ACTIVE : !LED_RGB_ACTIVE);
    g_notifyPending = true;

    checkResetPattern(0);

  } else {
    touchSen_LastRead_1 = digitalRead(TOUCH1_PIN);
  }

  if (digitalRead(FLASH_BUTTON_PIN) != flashButton_LastRead && digitalRead(FLASH_BUTTON_PIN) == FLASH_BUTTON_ACTIVE) {
    flashButton_LastRead = digitalRead(FLASH_BUTTON_PIN);

    if (isDebugOn)
      Serial.printf("flashLed_status: %d ", deviceInfo["led"].as<bool>());

    g_notifyPending = true;

  } else {
    flashButton_LastRead = digitalRead(FLASH_BUTTON_PIN);
  }
}



// Sends the pending state-change update. Called from loop(), never from a timer.
void ServiceNotifications(void) {
  if (!g_notifyPending) return;
  g_notifyPending = false;
  deviceInfo["commands"] = "UPDATE_OK";
  if (b_isAPStarted) {
    sendReplyToSender();
    sendReplyToAll();
  } else {
    sendReplyToAll();
    sendReplyToAll();
    sendReplyToAll();
  }
}

void setColor(uint8_t redValue, uint8_t greenValue, uint8_t blueValue) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(redValue, greenValue, blueValue);
  }
  // FastLED.show(); // disabled to avoid ESP32-S3 RMT DMA cache crash
}

static bool udpStarted = false;
void ensureUdpStarted() {
  if (!udpStarted) {
    int port = deviceInfo["listen_on_port"].as<int>();
    if (port <= 0) port = 8888;
    Udp.begin(port);
    udpStarted = true;
    if (isDebugOn) {
      Serial.printf("[UDP STARTED] Listening on port %d\n", port);
    }
  }
}

bool ConnectToWiFi(String ssid, String password, int waitCount, bool restartConnection = 1) {

  int i = 0;

  if ((ssid.length() < 1) || (password.length() < 8)) {
    if (isDebugOn) {
      Serial.print("\nIn ConnectToWiFi => can not Connect To: ");
      Serial.print(ssid);
      Serial.println(" (password hidden)");
    }
    return WIFI_NOT_CONNECTED;
  }

  if (restartConnection) {
    Serial.print("Restart WiFi connection\n");
    WiFi.disconnect();
    delay(50);
    WiFi.begin(ssid, password);
  }

  i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    i++;
    delay(50);
    MonitorTouchSensors();  // keep touch working while connecting (no network calls inside)
    if (isDebugOn)
      Serial.print(".");

    currentConnectionStatusESP = ROUTER_CONNECTING;

    if (i > waitCount) {
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    currentConnectionStatusESP = ROUTER_CONNECTED;
    ensureUdpStarted();

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
    Serial.println(" (password hidden)");

    return WIFI_NOT_CONNECTED;
  }
}


void WriteTo_EEPROM(int const address, uint8_t const val) {
  // Preferences doesn't support raw-address writes; save full config instead.
  // This function is kept for API compatibility.
  saveDataToEEPROM();
}



void ClearBuffer(char* buffer) {
  int i = 0;
  while (buffer[i] != 0) {
    buffer[i] = 0;
    i++;
  }
}

#include <esp_mac.h>

String Get_Mac_Address() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char macPart[18];
  snprintf(macPart, sizeof(macPart), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  macAddress = String(macPart);
  return macAddress;
}

void startSoftAP() {
  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_AP);
  delay(50);
  IPAddress local_ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  bool success = WiFi.softAP(AP_ssid, AP_password);
  if (success) {
    ensureUdpStarted();
    if (isDebugOn) {
      Serial.printf("\n[AP STARTED] SSID: %s | IP: %s\n", AP_ssid, WiFi.softAPIP().toString().c_str());
    }
  } else {
    if (isDebugOn) {
      Serial.println("\n[AP ERROR] WiFi.softAP failed to start!");
    }
  }
}

void Connecting_To_Router_Or_Ap() {
  if (!b_isAPStarted) {
    if (WiFi.status() != WL_CONNECTED) {
      if (ConnectToWiFi(deviceInfo["wifi_ssid"], deviceInfo["wifi_password"], WIFI_CONNEECTING_WAITING_COUNT) == WIFI_NOT_CONNECTED) {

        currentConnectionStatusESP = ROUTER_NOT_CONNECTED;
        if (currentConnectionStatusESP != Last_state) {
          Last_state = currentConnectionStatusESP;

          if (isDebugOn)
            Serial.print("The Connection to Router is Faild\n");
        }

        startSoftAP();

        if (WiFi.softAPgetStationNum() == 0) {
          currentConnectionStatusESP = AP_NOT_CONNECTED;
        } else {
          currentConnectionStatusESP = AP_CONNECTED;
        }

        b_isAPStarted = true;
        ul_APTimeOut = millis();

      } else {
        currentConnectionStatusESP = ROUTER_CONNECTED;
        WiFi.softAPdisconnect();

        if (currentConnectionStatusESP != Last_state) {
          Last_state = currentConnectionStatusESP;
          if (isDebugOn) {
            Serial.print("The Connection to Router is Succedded\n");
            Serial.print("with local esp IP Address: ");
            Serial.println(WiFi.localIP());
            Serial.printf("to Remote IP Address is %s with Port  %d ", senderIP.toString().c_str(), Udp.remotePort());
          }
        }
      }
    } else {
      currentConnectionStatusESP = ROUTER_CONNECTED;
      if (currentConnectionStatusESP != Last_state) {
        Last_state = currentConnectionStatusESP;
        if (isDebugOn)
          Serial.print("The Connection to Router is Already connected\n");
      }
    }
  } else {
    if (WiFi.softAPgetStationNum() == 0) {
      currentConnectionStatusESP = AP_NOT_CONNECTED;

      if (currentConnectionStatusESP != Last_state) {
        Last_state = currentConnectionStatusESP;
        if (isDebugOn)
          Serial.print("The Esp can't find any network connected to it  \n");
      }

      unsigned long tempTime = millis();
      if (((tempTime - ul_APTimeOut) / 1000) >= AP_TIMEOUT_SEC_MAX) {
        b_isAPStarted = false;
        WiFi.softAPdisconnect();
      }
    } else {
      currentConnectionStatusESP = AP_CONNECTED;
      if (currentConnectionStatusESP != Last_state) {
        Last_state = currentConnectionStatusESP;
        if (isDebugOn)
          Serial.printf("The Esp connected succesfuly with its AP IP Address %s :%d \n", WiFi.softAPIP().toString().c_str(), deviceInfo["listen_on_port"].as<int>());
      }
    }
  }
}

void void_connectToRouter() {
  static int connectingTimeOut = 30;
  static bool restartConnection = 0;
  
  if (!b_isAPStarted) {
    if (((String)deviceInfo["wifi_ssid"]).length() < 1 || ((String)deviceInfo["wifi_password"]).length() < 8) {

      if (isDebugOn){
        Serial.print("\nIn void_connectToRouter => can not Connect To: ");
        Serial.print((String)deviceInfo["wifi_ssid"]);
        Serial.print(":");
        Serial.println("(password hidden)");
      }

      startSoftAP();

      if (WiFi.softAPgetStationNum() == 0) {
        currentConnectionStatusESP = AP_NOT_CONNECTED;
      } else {
        currentConnectionStatusESP = AP_CONNECTED;
      }

      b_isAPStarted = true;
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
        if (isDebugOn)
          Serial.print("The Connection to Router Failed\n");
      } else {
        currentConnectionStatusESP = ROUTER_CONNECTED;
        WiFi.softAPdisconnect();

        if (currentConnectionStatusESP != Last_state) {
          Last_state = currentConnectionStatusESP;
          if (isDebugOn) {
            Serial.print("The Connection to Router is Succedded\n");
            Serial.print("with local esp IP Address: ");
            Serial.println(WiFi.localIP());
            Serial.printf("to Remote IP Address is %s with Port  %d ", senderIP.toString().c_str(), Udp.remotePort());
          }
        }
      }
    } else {
      currentConnectionStatusESP = ROUTER_CONNECTED;
      if (currentConnectionStatusESP != Last_state) {
        Last_state = currentConnectionStatusESP;
        if (isDebugOn)
          Serial.print("The Connection to Router is Already connected\n");
      }
    }
  } else {
    if (WiFi.softAPgetStationNum() == 0) {
      currentConnectionStatusESP = AP_NOT_CONNECTED;

      if (currentConnectionStatusESP != Last_state) {
        Last_state = currentConnectionStatusESP;
        if (isDebugOn)
          Serial.print("The Esp can't find any network connected to it  \n");
      }

      unsigned long tempTime = millis();
      if (((tempTime - ul_APTimeOut) / 1000) >= AP_TIMEOUT_SEC_MAX) {
        b_isAPStarted = false;
        WiFi.softAPdisconnect();
      }
    } else {
      currentConnectionStatusESP = AP_CONNECTED;
      if (currentConnectionStatusESP != Last_state) {
        Last_state = currentConnectionStatusESP;
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
  } else {
    gResetSecCounter = 0;  // count consecutive seconds only
  }
  AutoStatusUpdate();
}

void sendOKToSender(void) {
  int tries = 0;
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

Udp.write((const uint8_t*)ReplyBuffer.c_str(), ReplyBuffer.length());

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

Udp.write((const uint8_t*)ReplyBuffer.c_str(), ReplyBuffer.length());

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

Udp.write((const uint8_t*)ReplyBuffer.c_str(), ReplyBuffer.length());

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
  if (g_forceReset || gResetSecCounter > 9) {
    g_forceReset = false;
    gResetSecCounter = 0;
    void_resetSwitch();
  }
}


void void_resetSwitch(void) {
  deviceInfo["led"] = 1;

  if (isDebugOn)
    Serial.println("Resetting AP");

  startSoftAP();

  if (WiFi.softAPgetStationNum() == 0) {
    currentConnectionStatusESP = AP_NOT_CONNECTED;
  } else {
    currentConnectionStatusESP = AP_CONNECTED;
  }

  b_isAPStarted = true;
  ul_APTimeOut = millis();
}

void GetData_And_TakeAction() {
  if (!udpStarted) return;
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    unsigned long start = millis();
    senderIP = Udp.remoteIP();

    ClearBuffer(packetBuffer);
    Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);

    if (isDebugOn) {
      Serial.print("Contents:: ");
      Serial.println(packetBuffer);
    }

    deserializeJson(receivedInfo, packetBuffer);

    if (receivedInfo["mac_address"].is<JsonVariant>()) {
      if ((receivedInfo["mac_address"] == macAddress) || (receivedInfo["mac_address"] == "HH:HH:HH:HH:HH:HH")) {
        sendOKToSender();
        if (receivedInfo["commands"].is<JsonVariant>()) {
          takeAction(receivedInfo["commands"]);
        }
      } else if (receivedInfo["commands"].is<JsonVariant>()) {
        if (receivedInfo["commands"] == "MAC_ADDRESS_READ") {
          sendOKToSender();
          takeAction(receivedInfo["commands"]);
        }
      }
    } else if (receivedInfo["commands"].is<JsonVariant>()) {
      if (receivedInfo["commands"] == "MAC_ADDRESS_READ") {
        sendOKToSender();
        takeAction(receivedInfo["commands"]);
      }
    }

    unsigned long end = millis();
    if (isDebugOn) {
      Serial.print("Function execution time: ");
      Serial.print(end - start);
      Serial.println(" milliseconds");
    }
  }
}

String payload;
int counter = 0;
void takeAction(String cmd) {
  if (cmd == "WIFI_CONFIG") {
    if (receivedInfo["wifi_ssid"].is<JsonVariant>() && receivedInfo["wifi_password"].is<JsonVariant>()) {
      if (receivedInfo["wifi_ssid"] == deviceInfo["wifi_ssid"] && receivedInfo["wifi_password"] == deviceInfo["wifi_password"]) {
        deviceInfo["commands"] = "WIFI_CONFIG_SAME";
        sendReplyToSender();
      }

      deviceInfo["commands"] = "WIFI_CONFIG_CONNECTING";
      sendReplyToSender();
      delay(1000);

      if (ConnectToWiFi(receivedInfo["wifi_ssid"], receivedInfo["wifi_password"], WIFI_CONNEECTING_WAITING_COUNT) == WIFI_CONNECTED) {
        deviceInfo["wifi_ssid"] = receivedInfo["wifi_ssid"];
        deviceInfo["wifi_password"] = receivedInfo["wifi_password"];
        saveDataToEEPROM();

        deviceInfo["commands"] = "WIFI_CONFIG_OK";
        sendReplyToSender();
        WiFi.softAPdisconnect();
        if (isDebugOn) {
          Serial.printf("Succefuly connected with Remote IP Address is %s with Port  %d ", senderIP.toString().c_str(), Udp.remotePort());
        }
      } else {
        deviceInfo["commands"] = "WIFI_CONFIG_FAILED";
        sendReplyToSender();
      }
    } else {
      deviceInfo["commands"] = "WIFI_CONFIG_MISSED_DATA";
      sendReplyToSender();
    }
  } else if (cmd == "MAC_ADDRESS_READ") {
    deviceInfo["commands"] = "MAC_ADDRESS_READ_OK";
    sendReplyToSender();
  } else if (cmd == "WIFI_CONNECT_CHECK") {
    deviceInfo["commands"] = "WIFI_CONNECT_CHECK_OK";
    sendReplyToSender();
  } else if (cmd == "STATUS_READ" || cmd == "SWITCH_READ") {
    ReplyBuffer.clear();
    if (cmd == "STATUS_READ") deviceInfo["commands"] = "STATUS_READ_OK";
    if (cmd == "SWITCH_READ") deviceInfo["commands"] = "SWITCH_READ_OK";
    sendReplyToSender();
  } else if (cmd == "STATUS_WRITE" || cmd == "SWITCH_WRITE") {
    if (receivedInfo["sw0"].is<JsonVariant>()) {
      deviceInfo["sw0"] = receivedInfo["sw0"];
      digitalWrite(RELAY0_PIN, deviceInfo["sw0"].as<bool>() ? RELAY0_ACTIVE : !RELAY0_ACTIVE);
    }
    if (receivedInfo["sw1"].is<JsonVariant>()) {
      deviceInfo["sw1"] = receivedInfo["sw1"];
      digitalWrite(LED_RGB_PIN, deviceInfo["sw1"].as<bool>() ? LED_RGB_ACTIVE : !LED_RGB_ACTIVE);
      if (deviceInfo["sw1"].as<bool>()) {
        setColor((uint8_t)deviceInfo["green"].as<int>(), (uint8_t)deviceInfo["red"].as<int>(), (uint8_t)deviceInfo["blue"].as<int>());
      } else {
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
    }

    ReplyBuffer.clear();
    if (cmd == "STATUS_WRITE") deviceInfo["commands"] = "STATUS_WRITE_OK";
    if (cmd == "SWITCH_WRITE") deviceInfo["commands"] = "SWITCH_WRITE_OK";

    if (b_isAPStarted) {
      sendReplyToSender();
    } else {
      sendReplyToAll();
      sendReplyToAll();
      sendReplyToAll();
    }
  } else if (cmd == "RGB_READ") {
    deviceInfo["commands"] = "RGB_READ_OK";
    sendReplyToSender();
  } else if (cmd == "RGB_WRITE") {
    if (receivedInfo["blue"].is<JsonVariant>() && receivedInfo["green"].is<JsonVariant>() && receivedInfo["red"].is<JsonVariant>()) {
      deviceInfo["green"] = receivedInfo["green"];
      deviceInfo["red"] = receivedInfo["red"];
      deviceInfo["blue"] = receivedInfo["blue"];

      if (deviceInfo["sw1"].as<bool>()) {
        setColor((uint8_t)deviceInfo["green"].as<int>(), (uint8_t)deviceInfo["red"].as<int>(), (uint8_t)deviceInfo["blue"].as<int>());
      } else {
        setColor(0, 0, 0);
      }

      saveDataToEEPROM();
      deviceInfo["commands"] = "RGB_WRITE_OK";

      if (b_isAPStarted) {
        sendReplyToSender();
      } else {
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
    deviceInfo["commands"] = "DEVICE_CONFIG_WRITE_OK";
    sendReplyToSender();
  } else if (cmd == "READ") {
    deviceInfo["commands"] = "READ_OK";
    sendReplyToSender();
  } else if (cmd == "CHECK_FOR_NEW_FIRMWARE") {
    WiFiClientSecure wifiClient;
    wifiClient.setInsecure();
    HTTPClient http;
    http.begin(wifiClient, firmwareVersionLink);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      if (payload != deviceInfo["firmware_version"]) {
        deviceInfo["commands"] = "CHECK_FOR_NEW_FIRMWARE_OK";
      } else {
        deviceInfo["commands"] = "CHECK_FOR_NEW_FIRMWARE_SAME";
      }
    } else {
      deviceInfo["commands"] = "CHECK_FOR_NEW_FIRMWARE_FAIL";
    }
    http.end();
    sendReplyToSender();
  } else if (cmd == "DOWNLOAD_NEW_FIRMWARE") {
    WiFiClientSecure wifiClient;
    wifiClient.setInsecure();

    HTTPClient http;
    http.begin(wifiClient, firmwareVersionLink);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      payload = http.getString();
      http.end();

      if (payload != deviceInfo["firmware_version"]) {
        
        httpUpdate.rebootOnUpdate(false);
        httpUpdate.onStart(onStartOfUpdate);
        httpUpdate.onProgress(onProgressOfUpdate);
        httpUpdate.onEnd(onEndOfUpdate);
        
        // NOTE: certificate validation is off (as in the original). Pin the server CA before production.
        HTTPUpdateResult ret = httpUpdate.update(wifiClient, firmwareDownloadLink);

        switch (ret) {
          case HTTP_UPDATE_OK:
            if (isDebugOn) {
              Serial.println("Update success!");
            }
            deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_OK";
            sendReplyToSender();
            sendReplyToAll();
            sendReplyToAll();
            sendReplyToAll();

            delay(2000);
            ESP.restart();
            break;
          case HTTP_UPDATE_FAILED:
            deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_FAIL";
            sendReplyToSender();
            sendReplyToAll();
            sendReplyToAll();
            sendReplyToAll();
            if (isDebugOn) {
              Serial.printf("Update failed. Error1 (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            }
            break;

          case HTTP_UPDATE_NO_UPDATES:
            deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_FAIL";
            sendReplyToSender();
            sendReplyToAll();
            sendReplyToAll();
            sendReplyToAll();
            if (isDebugOn) {
              Serial.printf("Update failed. Error2 (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            }
            break;
        }

      } else {
        deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_SAME";
        sendReplyToSender();
        sendReplyToAll();
        sendReplyToAll();
        sendReplyToAll();
      }
    } else {
      http.end();
      deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_FAIL";
      sendReplyToSender();
      sendReplyToAll();
      sendReplyToAll();
      sendReplyToAll();
    }
  } else if (cmd == "RESTART") {
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
  Udp.stop();
  while (!Udp.begin(deviceInfo["listen_on_port"])) {
    if (isDebugOn) {
      Serial.println("Try to begin listening after failure in updating ota!");
    }
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
    sendReplyToSender();
    sendReplyToAll();
  } else if (i1 == i2) {
    deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_UPDATING_" + String(i1) + "_" + String(i2);
    sendReplyToSender();
    sendReplyToAll();
    sendReplyToAll();
    sendReplyToAll();
  }
}

void onEndOfUpdate(void) {
  deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_OK";
  deviceInfo["firmware_version"] = payload;
  saveDataToEEPROM();
  Udp.stop();
  Udp.begin(deviceInfo["listen_on_port"]);
  sendReplyToSender();
  sendReplyToAll();
  sendReplyToAll();
  sendReplyToAll();

  delay(2000);
  ESP.restart();
}

void onErrorUpdate(void) {
  deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_FAIL";
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
}


void AutoStatusUpdate(void) {
  if (((gSecCounter % deviceInfo["update_period"].as<int>()) == 0) && (deviceInfo["update_period"].as<int>() != 0)) {
    if (deviceInfo["sw1"].as<bool>()) {
      setColor((uint8_t)deviceInfo["green"].as<int>(), (uint8_t)deviceInfo["red"].as<int>(), (uint8_t)deviceInfo["blue"].as<int>());
    } else {
      setColor(0, 0, 0);
    }

    deviceInfo["commands"] = "UPDATE_OK";

    if (b_isAPStarted) {
      sendReplyToSender();
    } else {
      sendReplyToAll();
    }
  }
}

String generateRandomHexString(uint8_t bytesLength) {
  const char hexChars[] = "0123456789ABCDEF";
  String result = "";

  randomSeed(micros());

  uint8_t tMac[6];
  WiFi.macAddress(tMac);

  for (int i = 0; i < 6; i++) {
    if (tMac[i] < 16) result += '0';
    result += String(tMac[i], HEX);
  }

  result.toUpperCase();

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

  if (millis() - lastTapTime < 1000) {
    if (pos == 0) {
      tapCount++;
    }

    if ((tapCount >= 5) && (currentStage == 0) && (pos == 0)) {
      currentStage = 1;
    } else if ((currentStage == 1) && (pos == 1)) {
      currentStage = 2;
    } else if ((currentStage == 2) && (pos == 2)) {
      g_forceReset = true;
    }
  } else {
    tapCount = 1;
    currentStage = 0;
  }

  lastTapTime = millis();
}