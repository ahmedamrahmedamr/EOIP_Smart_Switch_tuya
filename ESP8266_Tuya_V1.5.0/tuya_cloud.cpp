#define MQTT_MAX_PACKET_SIZE 1024
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "tuya_cloud.h"
#include "Mega_Smart_Config.h"

// BearSSL / TLS Client
static WiFiClientSecure espClient;
static PubSubClient mqttClient(espClient);

static TuyaPropertyCallback g_propertyCallback = nullptr;
static unsigned long g_lastReconnectAttempt = 0;

// Forward declaration of internal functions
static bool tuya_mqtt_connect();
static void tuya_mqtt_callback(char* topic, byte* payload, unsigned int length);

// HMAC-SHA256 calculation using BearSSL included in ESP8266 Core
static String hmacSha256(const String& key, const String& data) {
  br_hmac_key_context kc;
  br_hmac_context hc;
  unsigned char out[32];

  br_hmac_key_init(&kc, &br_sha256_vtable, key.c_str(), key.length());
  br_hmac_init(&hc, &kc, 0);
  br_hmac_update(&hc, data.c_str(), data.length());
  br_hmac_out(&hc, out);

  String hexStr = "";
  hexStr.reserve(64);
  for (int i = 0; i < 32; i++) {
    char buf[3];
    sprintf(buf, "%02x", out[i]);
    hexStr += buf;
  }
  return hexStr;
}

void tuya_init(TuyaPropertyCallback callback) {
  g_propertyCallback = callback;

  // Use insecure mode / MFLN to avoid large certificate validation footprint on ESP8266
  espClient.setInsecure();
  espClient.setBufferSizes(1024, 512); // Reduce BearSSL TLS buffer to preserve heap

  mqttClient.setServer(TUYA_MQTT_HOST, TUYA_MQTT_PORT);
  mqttClient.setCallback(tuya_mqtt_callback);
  mqttClient.setBufferSize(1024);

  Serial.println("[TuyaLink] Client initialized.");
}

bool tuya_is_connected() {
  return mqttClient.connected();
}

static bool tuya_mqtt_connect() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  Serial.print("[TuyaLink] Connecting to broker ");
  Serial.print(TUYA_MQTT_HOST);
  Serial.print(":");
  Serial.println(TUYA_MQTT_PORT);

  // 1. Client ID: "tuyalink_" + DEVICE_ID
  String clientId = "tuyalink_" + String(TUYA_DEVICE_ID);

  // 2. Timestamp: standard 10-digit epoch or ms timestamp (or millis based)
  unsigned long timestamp = millis() + 1600000000UL; 

  // 3. Username format: "${deviceId}|signMethod=hmacSha256,timestamp=${timestamp},secureMode=1,accessType=1"
  String username = String(TUYA_DEVICE_ID) + 
                    "|signMethod=hmacSha256,timestamp=" + String(timestamp) + 
                    ",secureMode=1,accessType=1";

  // 4. Password content: "deviceId=${deviceId},timestamp=${timestamp},secureMode=1,accessType=1"
  String signContent = "deviceId=" + String(TUYA_DEVICE_ID) + 
                       ",timestamp=" + String(timestamp) + 
                       ",secureMode=1,accessType=1";

  String password = hmacSha256(TUYA_DEVICE_SECRET, signContent);

  if (mqttClient.connect(clientId.c_str(), username.c_str(), password.c_str())) {
    Serial.println("[TuyaLink] Connected successfully!");

    // Subscribe to property set topic: tylink/${deviceId}/thing/property/set
    String setTopic = "tylink/" + String(TUYA_DEVICE_ID) + "/thing/property/set";
    mqttClient.subscribe(setTopic.c_str());
    Serial.print("[TuyaLink] Subscribed to: ");
    Serial.println(setTopic);

    return true;
  } else {
    Serial.print("[TuyaLink] Connect failed, rc=");
    Serial.println(mqttClient.state());
    return false;
  }
}

void tuya_loop() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!mqttClient.connected()) {
    unsigned long now = millis();
    // Non-blocking reconnect attempt every 5 seconds
    if (now - g_lastReconnectAttempt > 5000) {
      g_lastReconnectAttempt = now;
      if (tuya_mqtt_connect()) {
        g_lastReconnectAttempt = 0;
      }
    }
  } else {
    mqttClient.loop();
  }
}

// Handler for incoming MQTT messages
static void tuya_mqtt_callback(char* topic, byte* payload, unsigned int length) {
  char msg[length + 1];
  memcpy(msg, payload, length);
  msg[length] = '\0';

  Serial.print("[TuyaLink] Message arrived on topic: ");
  Serial.println(topic);
  Serial.print("[TuyaLink] Payload: ");
  Serial.println(msg);

  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, msg);
  if (error) {
    Serial.print("[TuyaLink] deserializeJson() failed: ");
    Serial.println(error.f_str());
    return;
  }

  // Extract msgId for set_response
  const char* msgId = doc["msgId"] | "";

  // The payload format from Tuya:
  // { "msgId": "...", "time": ..., "data": { "switch_1": true, ... } }
  JsonObject dataObj = doc["data"].as<JsonObject>();
  for (JsonPair kv : dataObj) {
    String propName = kv.key().c_str();
    String propVal = kv.value().as<String>();

    Serial.print("[TuyaLink] Property command: ");
    Serial.print(propName);
    Serial.print(" = ");
    Serial.println(propVal);

    if (g_propertyCallback != nullptr) {
      g_propertyCallback(propName, propVal);
    }
  }

  // Send set_response acknowledgment back to Tuya
  if (strlen(msgId) > 0) {
    String respTopic = "tylink/" + String(TUYA_DEVICE_ID) + "/thing/property/set_response";
    JsonDocument respDoc;
    respDoc["msgId"] = msgId;
    respDoc["code"] = 0; // 0 = Success
    respDoc["data"] = JsonObject();

    String respStr;
    serializeJson(respDoc, respStr);
    mqttClient.publish(respTopic.c_str(), respStr.c_str());
  }
}

// Report boolean state (e.g., switch_1: true)
void tuya_report_bool(const char* propertyCode, bool val) {
  if (!mqttClient.connected()) return;

  String reportTopic = "tylink/" + String(TUYA_DEVICE_ID) + "/thing/property/report";

  JsonDocument doc;
  doc["msgId"] = String(millis());
  doc["time"] = millis();

  JsonObject dataObj = doc["data"].to<JsonObject>();
  JsonObject valObj = dataObj[propertyCode].to<JsonObject>();
  valObj["value"] = val;
  valObj["time"] = millis();

  String payload;
  serializeJson(doc, payload);

  mqttClient.publish(reportTopic.c_str(), payload.c_str());
  Serial.print("[TuyaLink] Reported bool: ");
  Serial.println(payload);
}

// Report string/raw state (e.g., colour_data)
void tuya_report_string(const char* propertyCode, const char* strVal) {
  if (!mqttClient.connected()) return;

  String reportTopic = "tylink/" + String(TUYA_DEVICE_ID) + "/thing/property/report";

  JsonDocument doc;
  doc["msgId"] = String(millis());
  doc["time"] = millis();

  JsonObject dataObj = doc["data"].to<JsonObject>();
  JsonObject valObj = dataObj[propertyCode].to<JsonObject>();
  valObj["value"] = strVal;
  valObj["time"] = millis();

  String payload;
  serializeJson(doc, payload);

  mqttClient.publish(reportTopic.c_str(), payload.c_str());
  Serial.print("[TuyaLink] Reported string: ");
  Serial.println(payload);
}

// Report integer state
void tuya_report_int(const char* propertyCode, int val) {
  if (!mqttClient.connected()) return;

  String reportTopic = "tylink/" + String(TUYA_DEVICE_ID) + "/thing/property/report";

  JsonDocument doc;
  doc["msgId"] = String(millis());
  doc["time"] = millis();

  JsonObject dataObj = doc["data"].to<JsonObject>();
  JsonObject valObj = dataObj[propertyCode].to<JsonObject>();
  valObj["value"] = val;
  valObj["time"] = millis();

  String payload;
  serializeJson(doc, payload);

  mqttClient.publish(reportTopic.c_str(), payload.c_str());
}
