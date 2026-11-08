#include "tuya_dp_handler.h"
#include "Mega_Smart_Config.h"
#include "Mega_Smart_Interface.h"
#include "tuya_cloud.h"

void handleTuyaPropertySet(JsonObject& props) {
    if (props.containsKey("switch_1")) {
        bool state1 = props["switch_1"].as<bool>();
        digitalWrite(13, state1 ? HIGH : LOW); // Update physical GPIO 13 (Switch 1)
        deviceInfo["sw0"] = state1;            // Update local state for cloud sync
    }
    if (props.containsKey("switch_2")) {
        bool state2 = props["switch_2"].as<bool>();
        digitalWrite(14, state2 ? HIGH : LOW); // Update physical GPIO 14 (Switch 2)
        deviceInfo["sw1"] = state2;            // Update local state for cloud sync
    }
}