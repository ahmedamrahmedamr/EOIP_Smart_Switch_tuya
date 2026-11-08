#include "tuya_dp_handler.h"
#include "Mega_Smart_Config.h"
#include "Mega_Smart_Interface.h"
#include "tuya_cloud.h"

void handleTuyaPropertySet(JsonDocument& properties) {
    bool stateChanged = false;    // live state only (sw0/sw1/sw2): not persisted, boot resets them to 0
    bool persistChanged = false;  // led / colour: stored in NVS

    if (properties["sw0"].is<JsonVariant>()) {
        deviceInfo["sw0"] = properties["sw0"].as<int>();
        digitalWrite(RELAY0_PIN, deviceInfo["sw0"].as<bool>() ? RELAY0_ACTIVE : !RELAY0_ACTIVE);
        stateChanged = true;
    }

    if (properties["sw2"].is<JsonVariant>()) {
        deviceInfo["sw2"] = properties["sw2"].as<int>();
        digitalWrite(RELAY1_PIN, deviceInfo["sw2"].as<bool>() ? RELAY1_ACTIVE : !RELAY1_ACTIVE);
        stateChanged = true;
    }

    if (properties["sw1"].is<JsonVariant>()) {
        deviceInfo["sw1"] = properties["sw1"].as<int>();
        digitalWrite(LED_RGB_PIN, deviceInfo["sw1"].as<bool>() ? LED_RGB_ACTIVE : !LED_RGB_ACTIVE);
        if (deviceInfo["sw1"].as<bool>()) {
            setColor((uint8_t)deviceInfo["green"].as<int>(), (uint8_t)deviceInfo["red"].as<int>(), (uint8_t)deviceInfo["blue"].as<int>());
        } else {
            setColor(0, 0, 0);
        }
        stateChanged = true;
    }

    if (properties["red"].is<JsonVariant>() || properties["green"].is<JsonVariant>() || properties["blue"].is<JsonVariant>()) {
        if (properties["red"].is<JsonVariant>()) deviceInfo["red"] = properties["red"].as<int>();
        if (properties["green"].is<JsonVariant>()) deviceInfo["green"] = properties["green"].as<int>();
        if (properties["blue"].is<JsonVariant>()) deviceInfo["blue"] = properties["blue"].as<int>();

        if (deviceInfo["sw1"].as<bool>()) {
            setColor((uint8_t)deviceInfo["green"].as<int>(), (uint8_t)deviceInfo["red"].as<int>(), (uint8_t)deviceInfo["blue"].as<int>());
        }
        persistChanged = true;
    }

    if (properties["led"].is<JsonVariant>()) {
        deviceInfo["led"] = properties["led"].as<int>();
        persistChanged = true;
    }

    if (persistChanged) {
        saveDataToStorage();  // not on every relay toggle: avoids needless NVS writes
    }

    if (stateChanged || persistChanged) {
        deviceInfo["commands"] = "UPDATE_OK";  // otherwise the LAN reply carries a stale command
        sendReplyToAll();
        tuya_sync_states();
    }
}
