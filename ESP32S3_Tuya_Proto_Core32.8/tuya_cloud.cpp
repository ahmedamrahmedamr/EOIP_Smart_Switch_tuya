#include "tuya_cloud.h"
#include "Mega_Smart_Config.h"
#include "Mega_Smart_Interface.h"
#include "tuya_dp_handler.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "mbedtls/md.h"
#include <time.h>

static WiFiClientSecure secureClient;
static PubSubClient mqttClient(secureClient);

static uint32_t lastReconnectAttempt = 0;
static uint32_t msgCounter = 1000;
static String tuyaHost = TUYA_MQTT_HOST;
static uint16_t tuyaPort = 8883;

static String calculateHMACSHA256(const String& key, const String& content) {
    unsigned char hmacOutput[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char*)key.c_str(), key.length());
    mbedtls_md_hmac_update(&ctx, (const unsigned char*)content.c_str(), content.length());
    mbedtls_md_hmac_finish(&ctx, hmacOutput);
    mbedtls_md_free(&ctx);

    String hexSignature = "";
    for (int i = 0; i < 32; i++) {
        if (hmacOutput[i] < 16) hexSignature += "0";
        hexSignature += String(hmacOutput[i], HEX);
    }
    return hexSignature;
}

static uint32_t getEpochTime() {
    time_t now;
    time(&now);
    if (now > 1600000000) {
        return (uint32_t)now;
    }
    return (uint32_t)(millis() / 1000) + 1700000000;
}
// static uint32_t getEpochTime() {
//     time_t now;
//     time(&now);
//     if (now > 1600000000) {
//         return (uint32_t)now;
//     }
//     return (uint32_t)(millis() / 1000) + 1700000000;
// }

static String getTuyaDeviceId() {
    String devId = deviceInfo["tuya_device_id"].as<String>();
    if (devId.length() == 0) devId = TUYA_DEFAULT_DEVICE_ID;
    return devId;
}

static String getTuyaDeviceSecret() {
    String sec = deviceInfo["tuya_device_secret"].as<String>();
    if (sec.length() == 0) sec = TUYA_DEFAULT_DEVICE_SECRET;
    return sec;
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    char buffer[length + 1];
    memcpy(buffer, payload, length);
    buffer[length] = '\0';

    JsonDocument rxDoc;
    DeserializationError err = deserializeJson(rxDoc, buffer);
    if (err) return;

    String topicStr = String(topic);
    String deviceId = getTuyaDeviceId();
    String setTopic = "tylink/" + deviceId + "/thing/property/set";

    if (topicStr == setTopic) {
        String msgId = rxDoc["msgId"].as<String>();
        if (rxDoc["data"].is<JsonObject>()) {
            JsonDocument props;
            props = rxDoc["data"];
            handleTuyaPropertySet(props);

            // Respond to set (QoS 0, retain = false)
            JsonDocument resDoc;
            resDoc["msgId"] = msgId;
            resDoc["code"] = 0;
            resDoc["msg"] = "success";
            String responsePayload;
            serializeJson(resDoc, responsePayload);
            String respTopic = "tylink/" + deviceId + "/thing/property/set_response";
            mqttClient.publish(respTopic.c_str(), responsePayload.c_str());
        }
    }
}

Mega_Smart_init();   // WiFi mode, UDP, pins, RGB

  void_connectToRouter();   // connect Wi-Fi first so NTP can sync

  tuya_cloud_init();   // Initialize Tuya Cloud connection parameters & NTP sync

void tuya_sync_states() {
    if (!mqttClient.connected()) return;

    String deviceId = getTuyaDeviceId();
    if (deviceId.length() == 0) return;

    JsonDocument payloadDoc;
    payloadDoc["msgId"] = String(msgCounter++);
    payloadDoc["time"] = getEpochTime();

    payloadDoc["data"]["sw0"] = deviceInfo["sw0"].as<bool>();
    payloadDoc["data"]["sw1"] = deviceInfo["sw1"].as<bool>();
    payloadDoc["data"]["sw2"] = deviceInfo["sw2"].as<bool>();
    payloadDoc["data"]["led"] = deviceInfo["led"].as<bool>();
    payloadDoc["data"]["red"] = deviceInfo["red"].as<int>();
    payloadDoc["data"]["green"] = deviceInfo["green"].as<int>();
    payloadDoc["data"]["blue"] = deviceInfo["blue"].as<int>();

    String outPayload;
    serializeJson(payloadDoc, outPayload);

    String topicReport = "tylink/" + deviceId + "/thing/property/report";
    mqttClient.publish(topicReport.c_str(), outPayload.c_str());
}

static bool connectMQTT() {
    String deviceId     = getTuyaDeviceId();
    String deviceSecret = getTuyaDeviceSecret();

    if (deviceId.length() == 0 || deviceSecret.length() == 0) {
        Serial.println("[Tuya] Missing credentials in deviceInfo - cannot connect");
        return false;
    }

    uint32_t timestamp = getEpochTime();
    if (timestamp == 0) {
        Serial.println("[Tuya] NTP not synced - skipping connect");
        return false;
    }

    String clientId   = "tuyalink_" + deviceId;
    String username   = deviceId + "|signMethod=hmacSha256,timestamp=" + String(timestamp) +
                        ",secureMode=1,accessType=1";
    String signContent = "deviceId=" + deviceId + ",timestamp=" + String(timestamp) +
                         ",secureMode=1,accessType=1";
    String password    = calculateHMACSHA256(deviceSecret, signContent);

    Serial.printf("[Tuya] Connecting to %s:%d as %s\n",
                  tuyaHost.c_str(), tuyaPort, clientId.c_str());

    if (mqttClient.connect(clientId.c_str(), username.c_str(), password.c_str())) {
        Serial.println("[Tuya] Connected!");
        String setTopic = "tylink/" + deviceId + "/thing/property/set";
        mqttClient.subscribe(setTopic.c_str(), 1);
        tuya_sync_states();
        return true;
    }

    Serial.printf("[Tuya] Connect failed, rc=%d\n", mqttClient.state());
    return false;
}

void tuya_loop() {
    if (WiFi.status() != WL_CONNECTED) return;

    if (!mqttClient.connected()) {
        uint32_t now = millis();
        if (now - lastReconnectAttempt > 10000 || lastReconnectAttempt == 0) {
            lastReconnectAttempt = now;
            connectMQTT();
        }
    } else {
        mqttClient.loop();
    }
}

bool tuya_is_connected() {
    return mqttClient.connected();
}
