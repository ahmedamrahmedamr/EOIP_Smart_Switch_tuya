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

// Real UTC seconds, or 0 while SNTP has not synced.
// (The old fallback "millis()/1000 + 1700000000" produced a wrong signature timestamp,
//  so the login was rejected and the "NTP not synced" check below could never trigger.)
static uint32_t getEpochTime() {
    time_t now;
    time(&now);
    if (now > 1600000000) {
        return (uint32_t)now;
    }
    return 0;
}

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
    static char buffer[1025];
    if (length >= sizeof(buffer)) return;
    memcpy(buffer, payload, length);
    buffer[length] = '\0';

    // PubSubClient reuses one buffer for rx and tx: copy everything out BEFORE publishing.
    String topicStr = String(topic);

    JsonDocument rxDoc;
    DeserializationError err = deserializeJson(rxDoc, buffer);
    if (err) return;

    String deviceId = getTuyaDeviceId();
    String setTopic = "tylink/" + deviceId + "/thing/property/set";

    if (topicStr == setTopic) {
        String msgId = rxDoc["msgId"].as<String>();
        if (rxDoc["data"].is<JsonObject>()) {
            JsonDocument props;
            props = rxDoc["data"];
            handleTuyaPropertySet(props);

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

// Called once from setup(). Without this the TLS client has no trust setting, MQTT has no server,
// no callback and a 256-byte buffer (too small for the property report), and SNTP never starts.
void tuya_cloud_init() {
    secureClient.setInsecure();  // NOTE: no certificate check (bring-up only; pin Tuya's CA later)
    mqttClient.setServer(tuyaHost.c_str(), tuyaPort);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(1024);
    mqttClient.setKeepAlive(60);
    configTime(0, 0, "pool.ntp.org", "time.google.com");  // UTC; syncs when Wi-Fi comes up
}

// Property report. Tuya Link expects each property as {"value":..,"time":<ms>} and time in ms
// (verify against the device log in the Tuya IoT console if reports do not show up).
void tuya_sync_states() {
    if (!mqttClient.connected()) return;

    String deviceId = getTuyaDeviceId();
    if (deviceId.length() == 0) return;

    uint32_t sec = getEpochTime();
    if (sec == 0) return;
    uint64_t nowMs = (uint64_t)sec * 1000ULL;

    JsonDocument payloadDoc;
    payloadDoc["msgId"] = String(msgCounter++);
    payloadDoc["time"] = nowMs;

    JsonObject data = payloadDoc["data"].to<JsonObject>();
    const char* boolKeys[] = {"sw0", "sw1", "sw2", "led"};
    for (const char* k : boolKeys) {
        JsonObject o = data[k].to<JsonObject>();
        o["value"] = deviceInfo[k].as<bool>();
        o["time"] = nowMs;
    }
    const char* intKeys[] = {"red", "green", "blue"};
    for (const char* k : intKeys) {
        JsonObject o = data[k].to<JsonObject>();
        o["value"] = deviceInfo[k].as<int>();
        o["time"] = nowMs;
    }

    String outPayload;
    serializeJson(payloadDoc, outPayload);

    String topicReport = "tylink/" + deviceId + "/thing/property/report";
    if (!mqttClient.publish(topicReport.c_str(), outPayload.c_str())) {
        Serial.println("[Tuya] publish failed (buffer too small or link down)");
    }
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
            connectMQTT();   // blocks for the TLS handshake (about 1-3 s)
        }
    } else {
        mqttClient.loop();
    }
}

bool tuya_is_connected() {
    return mqttClient.connected();
}
