#include "tuya_dp_handler.h"
#include "Mega_Smart_Config.h"
#include "Mega_Smart_Interface.h"
#include "tuya_cloud.h"

// Cloud property set -> real outputs. The signature MUST match tuya_dp_handler.h
// (JsonDocument&); a JsonObject& version links as a different function and the call is unresolved.
void handleTuyaPropertySet(JsonDocument& properties) {
    bool changed = false;

    // switch_1 -> sw0 -> RELAY0
    if (!properties[TUYA_PROP_SW0].isNull()) {
        deviceInfo["sw0"] = properties[TUYA_PROP_SW0].as<bool>();
        digitalWrite(RELAY0_PIN, deviceInfo["sw0"].as<bool>() ? RELAY0_ACTIVE : !RELAY0_ACTIVE);
        changed = true;
    }

    // switch_2 -> sw1 -> RGB strip + its enable pin
    if (!properties[TUYA_PROP_SW1].isNull()) {
        deviceInfo["sw1"] = properties[TUYA_PROP_SW1].as<bool>();
        digitalWrite(LED_RGB_PIN, deviceInfo["sw1"].as<bool>() ? LED_RGB_ACTIVE : !LED_RGB_ACTIVE);
        if (deviceInfo["sw1"].as<bool>()) {
            setColor((uint8_t)deviceInfo["green"].as<int>(), (uint8_t)deviceInfo["red"].as<int>(), (uint8_t)deviceInfo["blue"].as<int>());
        } else {
            setColor(0, 0, 0);
        }
        changed = true;
    }

#ifdef TUYA_PROP_SW2
    // switch_3 -> sw2 -> RELAY1
    if (!properties[TUYA_PROP_SW2].isNull()) {
        deviceInfo["sw2"] = properties[TUYA_PROP_SW2].as<bool>();
        digitalWrite(RELAY1_PIN, deviceInfo["sw2"].as<bool>() ? RELAY1_ACTIVE : !RELAY1_ACTIVE);
        changed = true;
    }
#endif

    if (changed) {
        deviceInfo["commands"] = "UPDATE_OK";  // LAN reply must not carry a stale command
        sendReplyToAll();
        tuya_sync_states();                    // confirm the new state to the Tuya app
    }
}
