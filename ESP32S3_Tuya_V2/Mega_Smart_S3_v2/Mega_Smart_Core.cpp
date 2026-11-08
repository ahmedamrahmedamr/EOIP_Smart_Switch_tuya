// Mega Smart switch core - ESP32-S3 port of the ESP8266 implementation file.
// Protocol (UDP + JSON commands/replies) is unchanged.
#include <Arduino.h>
#include "tuya_cloud.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <FastLED.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include "Mega_Smart_Config.h"
#include "Mega_Smart_Interface.h"
#include "Mega_Smart_Private.h"
#include "LEDIndicator.h"

bool isDebugOn = DEBUG_MODE;

JsonDocument deviceInfo;     // persistent config + live state
JsonDocument receivedInfo;   // last received command

String firmwareDownloadLink = FIRMWARE_DOWNLOAD_URL;
String firmwareVersionLink = FIRMWARE_VERSION_URL;

LEDIndicator ledIndicator(LED_INDICATOR_PIN, LED_TICK_SEC, LED_INDICATOR_ACTIVE);

int gResetSecCounter = 0;
int gSecCounter = 0;

IPAddress senderIP;

//######################### Access Point Data ############################
String macAddress = "";
bool b_isAPStarted = 0;
unsigned long ul_APTimeOut = 0;

//########################### UDP #################################
char packetBuffer[PACKET_BUF_SIZE + 1];
String ReplyBuffer = "";
WiFiUDP Udp;

//########################### Switches ##############################
uint8_t touchSen_LastRead_0 = 0;
uint8_t touchSen_LastRead_1 = 0;
uint8_t touchSen_LastRead_2 = 0;
uint8_t flashButton_LastRead = !FLASH_BUTTON_ACTIVE;

//########################### RGB ##############################
CRGB leds[NUM_LEDS];

//###################### Connecting To WiFi #########################
Status currentConnectionStatusESP = ROUTER_NOT_CONNECTED, Last_state = ROUTER_NOT_CONNECTED;

//###################### Storage (NVS) #########################
static Preferences prefs;

// Wi-Fi password is intentionally empty: a fresh board starts the "MegaSmart" AP for provisioning.
const String defaultData = "{\"device_type\":\"switch\",\"mac_address\":\"00:11:22:33:44:55\",\"device_location\":\"Living Room\",\"firmware_version\":\"" FIRMWARE_VERSION "\",\"wifi_ssid\":\"\",\"wifi_password\":\"\",\"server_ip\":\"server_ip\",\"server_port\":1234,\"sending_to_port\":8081,\"listen_on_port\":8888,\"commands\":\"command\",\"sw0\":0,\"sw1\":0,\"sw2\":0,\"led\":1,\"red\":255,\"green\":255,\"blue\":255,\"update_period\":10,\"tuya_pid\":\"\",\"tuya_device_id\":\"\",\"tuya_device_secret\":\"\",\"tuya_region\":\"" TUYA_DEFAULT_REGION "\"}";
String payload;
int counter = 0;

//###################### Scheduler (replaces Tickers) #########################
static uint32_t lastTouchMs = 0;
static uint32_t lastSecMs = 0;

void Mega_Smart_startScheduler() {
  lastTouchMs = millis();
  lastSecMs = millis();
}

// Call from loop() and from any long blocking wait. Everything runs in the caller's task,
// so deviceInfo / ReplyBuffer / UDP / FastLED are never touched from two tasks at once.
void Mega_Smart_poll() {
  static bool busy = false;
  if (busy) return;
  busy = true;

  uint32_t now = millis();

  if ((uint32_t)(now - lastTouchMs) >= TOUCH_POLL_MS) {
    lastTouchMs = now;
    MonitorTouchSensors();
  }

  if ((uint32_t)(now - lastSecMs) >= SEC_TICK_MS) {
    lastSecMs += SEC_TICK_MS;
    if ((uint32_t)(now - lastSecMs) >= SEC_TICK_MS) lastSecMs = now;  // far behind: resync, no burst
    secCounter();
  }

  ledIndicator.service();

  busy = false;
}

//###################### Helpers #########################
static bool hasValidCredentials() {
  return deviceInfo["wifi_ssid"].as<String>().length() >= 1 && deviceInfo["wifi_password"].as<String>().length() >= 8;
}

static IPAddress getBroadcastIP() {
  if (WiFi.status() == WL_CONNECTED) return WiFi.broadcastIP();
  if (b_isAPStarted) return WiFi.softAPBroadcastIP();
  return IPAddress(255, 255, 255, 255);
}

static void startAccessPoint() {
  WiFi.disconnect();
  delay(50);
  WiFi.mode(WIFI_AP_STA);
  bool ok = WiFi.softAP(AP_ssid, AP_password);
  if (isDebugOn) {
    Serial.printf("softAP \"%s\": %s\n", AP_ssid, ok ? "started" : "FAILED");
  }

  if (WiFi.softAPgetStationNum() == 0) {
    currentConnectionStatusESP = AP_NOT_CONNECTED;
  } else {
    currentConnectionStatusESP = AP_CONNECTED;
  }

  b_isAPStarted = true;
  ul_APTimeOut = millis();
}

static void stopAccessPoint() {
  WiFi.softAPdisconnect(true);  // true = really switch the AP interface off
}

// Reply = deviceInfo without the Tuya device secret (replies go out as plain UDP broadcast)
static void buildReplyBuffer() {
  JsonDocument out = deviceInfo;
  out.remove("tuya_device_secret");
  ReplyBuffer.clear();
  serializeJsonPretty(out, ReplyBuffer);
}

// update burst used by touch / flash button
static void broadcastUpdate() {
  if (b_isAPStarted) {
    sendReplyToSender();
    sendReplyToAll();
  } else {
    sendReplyToAll();
    sendReplyToAll();
    sendReplyToAll();
  }
  tuya_sync_states();  // keep the Tuya app in step with touch / local changes
}

//###################### Device info (NVS instead of EEPROM) #########################
void initDeviceInfo(void) {

  prefs.begin(NVS_NAMESPACE, false);

  if (isDebugOn) {
    Serial.print("latest_firmware_url: ");
    Serial.println(firmwareVersionLink);

    Serial.print("download_firmware_url: ");
    Serial.println(firmwareDownloadLink);

    Serial.print("defaultData: ");
    Serial.println(defaultData.length());
  }

  if (!boolGetDeviceInfoFromStorage()) {

    deserializeJson(deviceInfo, defaultData);

    if (isDebugOn) {
      Serial.print("defaultData before saving");
      Serial.println(defaultData);
    }

    saveDataToStorage();
  }
  // data exists but the firmware version changed: re-seed defaults, keep only what is listed
  else if (deviceInfo["firmware_version"] != FIRMWARE_VERSION) {

    JsonDocument tempDeviceInfo = deviceInfo;

    deserializeJson(deviceInfo, defaultData);

    deviceInfo["firmware_version"] = FIRMWARE_VERSION;

    // data to keep; anything not listed is taken from the default data
    deviceInfo["wifi_ssid"] = tempDeviceInfo["wifi_ssid"];
    deviceInfo["wifi_password"] = tempDeviceInfo["wifi_password"];

    Serial.print("defaultData before saving");
    Serial.println(defaultData);

    saveDataToStorage();
  }
}

bool boolGetDeviceInfoFromStorage() {
  String saved = prefs.getString("info", "");

  if (isDebugOn) {
    Serial.print("savedDeviceInfo: ");
    Serial.println(saved);
  }

  JsonDocument tempDeviceInfo;
  DeserializationError err = deserializeJson(tempDeviceInfo, saved);

  if (!err && !tempDeviceInfo["device_type"].isNull()) {
    deviceInfo = tempDeviceInfo;
    if (isDebugOn) Serial.println("device_type exists.");
    return true;
  }

  if (isDebugOn) Serial.println("device_type does not exist.");
  return false;
}

bool boolSetDeviceInfoToStorage(const String& data) {
  return prefs.putString("info", data) > 0;
}

void saveDataToStorage(void) {
  String out;
  serializeJson(deviceInfo, out);  // compact: smaller NVS write
  boolSetDeviceInfoToStorage(out);
}

//###################################################################
void Mega_Smart_init() {

  WiFi.persistent(false);   // credentials live in our own JSON, not in the Wi-Fi driver's NVS
  WiFi.mode(WIFI_STA);      // bring the network stack up before UDP / MAC
  WiFi.setSleep(false);     // keep broadcast UDP reliable (mains powered)

  FastLED.addLeds<WS2812, DATA_PIN, RGB>(leds, NUM_LEDS);  // same strip type / order as before

  if (isDebugOn) {
    Serial.print("Fetched SSID :: ");
    Serial.println(deviceInfo["wifi_ssid"].as<String>());
    Serial.print("Fetched Pass :: ");
    Serial.println(deviceInfo["wifi_password"].as<String>());
  }

  pinMode(RELAY0_PIN, OUTPUT);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(LED_RGB_PIN, OUTPUT);

  // Active-HIGH sensors: pulldown keeps unconnected pins at a steady LOW (no phantom toggles)
  pinMode(TOUCH0_PIN, INPUT_PULLDOWN);
  pinMode(TOUCH1_PIN, INPUT_PULLDOWN);
  pinMode(TOUCH2_PIN, INPUT_PULLDOWN);

  pinMode(FLASH_BUTTON_PIN, INPUT_PULLUP);  // GPIO0 = BOOT button

  ledIndicator.begin();
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

  // RGB control (note: colour arguments are passed G,R,B on purpose, as in the original)
  if (deviceInfo["sw1"].as<bool>()) {
    setColor((uint8_t)deviceInfo["green"].as<int>(), (uint8_t)deviceInfo["red"].as<int>(), (uint8_t)deviceInfo["blue"].as<int>());
  } else {
    setColor(0, 0, 0);
  }

  Udp.begin(deviceInfo["listen_on_port"].as<uint16_t>());

  deviceInfo["mac_address"] = Get_Mac_Address();
}

void MonitorTouchSensors() {
  if (digitalRead(TOUCH2_PIN) != touchSen_LastRead_2) {
    deviceInfo["sw2"] = deviceInfo["sw2"].as<bool>() ^ 1;
    touchSen_LastRead_2 = digitalRead(TOUCH2_PIN);
    digitalWrite(RELAY1_PIN, deviceInfo["sw2"].as<bool>() ? RELAY1_ACTIVE : !RELAY1_ACTIVE);

    deviceInfo["commands"] = "UPDATE_OK";
    broadcastUpdate();
    checkResetPattern(1);
  } else {
    touchSen_LastRead_2 = digitalRead(TOUCH2_PIN);
  }

  if (digitalRead(TOUCH0_PIN) != touchSen_LastRead_0) {
    deviceInfo["sw0"] = deviceInfo["sw0"].as<bool>() ^ 1;
    touchSen_LastRead_0 = digitalRead(TOUCH0_PIN);
    digitalWrite(RELAY0_PIN, deviceInfo["sw0"].as<bool>() ? RELAY0_ACTIVE : !RELAY0_ACTIVE);
    deviceInfo["commands"] = "UPDATE_OK";
    broadcastUpdate();
    checkResetPattern(2);
  } else {
    touchSen_LastRead_0 = digitalRead(TOUCH0_PIN);
  }

  if (digitalRead(TOUCH1_PIN) != touchSen_LastRead_1) {
    deviceInfo["sw1"] = deviceInfo["sw1"].as<bool>() ^ 1;
    touchSen_LastRead_1 = digitalRead(TOUCH1_PIN);

    if (deviceInfo["sw1"].as<bool>()) {
      setColor((uint8_t)deviceInfo["green"].as<int>(), (uint8_t)deviceInfo["red"].as<int>(), (uint8_t)deviceInfo["blue"].as<int>());
    } else {
      setColor(0, 0, 0);
    }

    digitalWrite(LED_RGB_PIN, deviceInfo["sw1"].as<bool>() ? LED_RGB_ACTIVE : !LED_RGB_ACTIVE);
    deviceInfo["commands"] = "UPDATE_OK";
    broadcastUpdate();
    checkResetPattern(0);
  } else {
    touchSen_LastRead_1 = digitalRead(TOUCH1_PIN);
  }

  if (digitalRead(FLASH_BUTTON_PIN) != flashButton_LastRead && digitalRead(FLASH_BUTTON_PIN) == FLASH_BUTTON_ACTIVE) {

    flashButton_LastRead = digitalRead(FLASH_BUTTON_PIN);

    if (isDebugOn)
      Serial.printf("flashLed_status: %d ", deviceInfo["led"].as<bool>());

    deviceInfo["commands"] = "UPDATE_OK";
    broadcastUpdate();

  } else {
    flashButton_LastRead = digitalRead(FLASH_BUTTON_PIN);
  }
}

void setColor(uint8_t redValue, uint8_t greenValue, uint8_t blueValue) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(redValue, greenValue, blueValue);
  }
  FastLED.show();  // once, after all LEDs are set
}

bool ConnectToWiFi(String ssid, String password, int waitCount, bool restartConnection) {

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
    Serial.print("Restart WiFi connection\n");
    WiFi.disconnect();
    delay(50);

    WiFi.begin(ssid.c_str(), password.c_str());  // enables STA, keeps the AP if it is running
  }

  i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    i++;
    delay(50);
    Mega_Smart_poll();  // keep touch, LED pattern and reset counter alive while waiting
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

void ClearBuffer(char* buffer) {
  memset(buffer, 0, strlen(buffer));
}

String Get_Mac_Address() {
  byte mac[6];
  WiFi.macAddress(mac);
  macAddress = "";
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

void void_connectToRouter() {

  static int connectingTimeOut = 30;
  static bool restartConnection = 0;

  if (!b_isAPStarted) {

    if (!hasValidCredentials()) {

      if (isDebugOn) {
        Serial.print("\nIn void_connectToRouter => can not Connect To: ");
        Serial.print(deviceInfo["wifi_ssid"].as<String>());
        Serial.print(":");
        Serial.println(deviceInfo["wifi_password"].as<String>());
      }

      startAccessPoint();
    }
    else if (WiFi.status() != WL_CONNECTED) {
      connectingTimeOut++;

      if (connectingTimeOut >= 30) {
        connectingTimeOut = 0;
        restartConnection = 1;
      } else {
        restartConnection = 0;
      }

      if (ConnectToWiFi(deviceInfo["wifi_ssid"].as<String>(), deviceInfo["wifi_password"].as<String>(), 10, restartConnection) == WIFI_NOT_CONNECTED) {

        if (isDebugOn)
          Serial.print("The Connection to Router Failed\n");

      } else {
        currentConnectionStatusESP = ROUTER_CONNECTED;

        stopAccessPoint();

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
    // Access point already started

    if (WiFi.softAPgetStationNum() == 0) {
      currentConnectionStatusESP = AP_NOT_CONNECTED;

      if (currentConnectionStatusESP != Last_state) {
        Last_state = currentConnectionStatusESP;
        if (isDebugOn)
          Serial.print("The Esp can't find any network connected to it  \n");
      }

      if (((millis() - ul_APTimeOut) / 1000) >= AP_TIMEOUT_SEC_MAX) {

        if (!hasValidCredentials()) {
          // nothing to fall back to: keep the AP up (the original tore it down and restarted it at once)
          ul_APTimeOut = millis();
        } else {
          b_isAPStarted = false;
          stopAccessPoint();
        }
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

// Called once per second from Mega_Smart_poll()
void secCounter() {

  gSecCounter++;

  if (digitalRead(FLASH_BUTTON_PIN) == FLASH_BUTTON_ACTIVE) {
    gResetSecCounter++;
  }

  AutoStatusUpdate();
}

//###################### UDP send #########################
static void udpSendReplyBuffer(const IPAddress& ip) {
  int tries = 0;
  uint16_t port = deviceInfo["sending_to_port"].as<uint16_t>();

  while (!Udp.beginPacket(ip, port)) {
    if (++tries >= 3) {
      if (isDebugOn)
        Serial.printf("Error in begin \n");
      return;  // nothing to write to
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

void sendOKToSender(void) {
  ReplyBuffer.clear();
  ReplyBuffer = "OK";

  if (isDebugOn)
    Serial.printf("\nSend to IP %s:%d, OK\n", senderIP.toString().c_str(), deviceInfo["sending_to_port"].as<int>());

  udpSendReplyBuffer(senderIP);
}

void sendReplyToAll(void) {
  buildReplyBuffer();

  IPAddress bcast = getBroadcastIP();

  if (isDebugOn)
    Serial.printf("\nSend to IP %s:%d, command: %s\n", bcast.toString().c_str(), deviceInfo["sending_to_port"].as<int>(), deviceInfo["commands"].as<String>().c_str());

  udpSendReplyBuffer(bcast);
}

void sendReplyToSender(void) {
  buildReplyBuffer();

  if (isDebugOn)
    Serial.printf("\nSend to IP %s:%d, command: %s\n", senderIP.toString().c_str(), deviceInfo["sending_to_port"].as<int>(), deviceInfo["commands"].as<String>().c_str());

  udpSendReplyBuffer(senderIP);
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

  startAccessPoint();
}

void GetData_And_TakeAction() {
  int packetSize = Udp.parsePacket();
  if (packetSize) {

    unsigned long start = millis();

    senderIP = Udp.remoteIP();

    int n = Udp.read(packetBuffer, PACKET_BUF_SIZE);
    if (n < 0) n = 0;
    packetBuffer[n] = 0;  // always terminate

    if (isDebugOn) {
      Serial.print("Contents:: ");
      Serial.println(packetBuffer);
    }

    DeserializationError err = deserializeJson(receivedInfo, packetBuffer);
    if (err) {
      if (isDebugOn) {
        Serial.print("JSON error: ");
        Serial.println(err.c_str());
      }
      return;
    }

    if (receivedInfo["mac_address"].is<JsonVariant>()) {
      if (isDebugOn)
        Serial.println("1");

      if ((receivedInfo["mac_address"] == macAddress) || (receivedInfo["mac_address"] == "HH:HH:HH:HH:HH:HH")) {

        if (isDebugOn)
          Serial.println("2");

        sendOKToSender();

        if (receivedInfo["commands"].is<JsonVariant>()) {
          if (isDebugOn)
            Serial.println("3");
          takeAction(receivedInfo["commands"].as<String>());
        }
      } else if (receivedInfo["commands"].is<JsonVariant>()) {
        if (isDebugOn)
          Serial.println("4");

        if (receivedInfo["commands"] == "MAC_ADDRESS_READ") {

          sendOKToSender();

          if (isDebugOn)
            Serial.println("5");
          takeAction(receivedInfo["commands"].as<String>());
        }
      }
    } else if (receivedInfo["commands"].is<JsonVariant>()) {
      if (isDebugOn)
        Serial.println("6");

      if (receivedInfo["commands"] == "MAC_ADDRESS_READ") {

        sendOKToSender();

        if (isDebugOn)
          Serial.println("7");
        takeAction(receivedInfo["commands"].as<String>());
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

void takeAction(String cmd) {

  if (cmd == "WIFI_CONFIG") {

    //###################### Connecting To WiFi #######################
    if (receivedInfo["wifi_ssid"].is<JsonVariant>() && receivedInfo["wifi_password"].is<JsonVariant>()) {

      if (receivedInfo["wifi_ssid"] == deviceInfo["wifi_ssid"] && receivedInfo["wifi_password"] == deviceInfo["wifi_password"]) {
        deviceInfo["commands"] = "WIFI_CONFIG_SAME";
        sendReplyToSender();
      }

      deviceInfo["commands"] = "WIFI_CONFIG_CONNECTING";
      sendReplyToSender();
      delay(1000);

      if (ConnectToWiFi(receivedInfo["wifi_ssid"].as<String>(), receivedInfo["wifi_password"].as<String>(), WIFI_CONNEECTING_WAITING_COUNT) == WIFI_CONNECTED) {

        deviceInfo["wifi_ssid"] = receivedInfo["wifi_ssid"];
        deviceInfo["wifi_password"] = receivedInfo["wifi_password"];

        saveDataToStorage();

        deviceInfo["commands"] = "WIFI_CONFIG_OK";
        sendReplyToSender();

        stopAccessPoint();
        if (isDebugOn) {
          Serial.printf("Succefuly connected with Remote IP Address is %s with Port  %d ", senderIP.toString().c_str(), Udp.remotePort());
        }
      } else {
        //------------------------- Remain In Access Point------------------
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
      saveDataToStorage();
    }

    ReplyBuffer.clear();

    if (cmd == "STATUS_WRITE") {
      deviceInfo["commands"] = "STATUS_WRITE_OK";
    }

    if (cmd == "SWITCH_WRITE") {
      deviceInfo["commands"] = "SWITCH_WRITE_OK";
    }

    tuya_sync_states();

    if (b_isAPStarted) {
      sendReplyToSender();
    } else {
      sendReplyToAll();
      sendReplyToAll();
      sendReplyToAll();
    }

  } else if (cmd == "RGB_READ") {

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

      if (deviceInfo["sw1"].as<bool>()) {
        setColor((uint8_t)deviceInfo["green"].as<int>(), (uint8_t)deviceInfo["red"].as<int>(), (uint8_t)deviceInfo["blue"].as<int>());
      } else {
        setColor(0, 0, 0);
      }

      saveDataToStorage();

      deviceInfo["commands"] = "RGB_WRITE_OK";
      tuya_sync_states();
      if (isDebugOn) {
        Serial.printf("\n%d  %d  %d  \n", deviceInfo["red"].as<int>(), deviceInfo["green"].as<int>(), deviceInfo["blue"].as<int>());
      }

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

    saveDataToStorage();

    deviceInfo["commands"] = "DEVICE_CONFIG_WRITE_OK";

    sendReplyToSender();

  } else if (cmd == "READ") {
    deviceInfo["commands"] = "READ_OK";
    sendReplyToSender();
  } else if (cmd == "CHECK_FOR_NEW_FIRMWARE") {

    WiFiClientSecure wifiClient;
    wifiClient.setInsecure();  // NOTE: no certificate check (same as the original)
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(wifiClient, firmwareVersionLink);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      payload.trim();
      if (isDebugOn) {
        Serial.println("Received: " + payload);
      }

      if (payload != deviceInfo["firmware_version"]) {
        deviceInfo["commands"] = "CHECK_FOR_NEW_FIRMWARE_OK";
      } else {
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

    WiFiClientSecure wifiClient;
    wifiClient.setInsecure();  // NOTE: no certificate check (same as the original)

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(wifiClient, firmwareVersionLink);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      payload = http.getString();
      payload.trim();
      http.end();
      if (isDebugOn) {
        Serial.println("Received: " + payload);
      }

      if (payload != deviceInfo["firmware_version"]) {

        httpUpdate.rebootOnUpdate(false);

        httpUpdate.onStart(onStartOfUpdate);
        httpUpdate.onProgress(onProgressOfUpdate);
        httpUpdate.onEnd(onEndOfUpdate);

        HTTPClient httpClient;
        httpClient.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        httpClient.begin(wifiClient, firmwareDownloadLink);

        counter = 0;

        t_httpUpdate_return ret = httpUpdate.update(httpClient);

        httpClient.end();
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

//###################### OTA callbacks #########################
static void restartUdpListener() {
  Udp.stop();
  for (int tries = 0; tries < 50; tries++) {  // bounded: the original looped forever
    if (Udp.begin(deviceInfo["listen_on_port"].as<uint16_t>())) return;
    if (isDebugOn) {
      Serial.println("Try to begin listening after failure in updating ota!");
    }
    delay(10);
  }
}

void onStartOfUpdate(void) {

  deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_START";
  restartUdpListener();

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

    sendReplyToSender();
    sendReplyToAll();

  } else if (i1 == i2) {

    deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_UPDATING_" + String(i1) + "_" + String(i2);

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
  deviceInfo["commands"] = "DOWNLOAD_NEW_FIRMWARE_OK";
  deviceInfo["firmware_version"] = payload;
  saveDataToStorage();
  restartUdpListener();
  sendReplyToSender();
  sendReplyToAll();
  sendReplyToAll();
  sendReplyToAll();

  if (isDebugOn) {
    Serial.println("Update success!");
  }
  delay(2000);
  ESP.restart();
}

void AutoStatusUpdate(void) {

  int period = deviceInfo["update_period"].as<int>();

  // period is tested first: on ESP32-S3 an integer modulo by zero is a hardware exception
  if ((period != 0) && ((gSecCounter % period) == 0)) {

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

    Serial.printf("tapCount: %d\n", tapCount);

    if ((tapCount >= 5) && (currentStage == 0) && (pos == 0)) {
      currentStage = 1;
      Serial.println("Resetting: go to Stage = 1");
    } else if ((currentStage == 1) && (pos == 1)) {
      currentStage = 2;
      Serial.println("Resetting: go to Stage = 2");
    } else if ((currentStage == 2) && (pos == 2)) {
      Serial.println("Resetting: go to Stage = 3 and resetting");
      Serial.print("New token: ");
      Serial.println(generateRandomToken());

      gResetSecCounter = 10;
    }

  } else {
    tapCount = 1;
    currentStage = 0;
  }

  lastTapTime = millis();
}