#ifndef TUYA_CLOUD_H
#define TUYA_CLOUD_H

#include <Arduino.h>

// Callback function type triggered when Tuya sends a property command
// propertyName: e.g. "switch_1", "switch_2", "colour_data"
// valueJson: e.g. "true", "false", "\"000003E803E8\""
typedef void (*TuyaPropertyCallback)(const String& propertyName, const String& valueJson);

// Initialize TuyaLink connection
void tuya_init(TuyaPropertyCallback callback);

// Call this regularly in loop() to keep MQTT connection alive and process packets
void tuya_loop();

// Check if currently connected to Tuya MQTT broker
bool tuya_is_connected();

// Report a boolean property (e.g., switch_1: true)
void tuya_report_bool(const char* propertyCode, bool val);

// Report a string/JSON property (e.g. colour_data: "000003E803E8")
void tuya_report_string(const char* propertyCode, const char* strVal);

// Report an integer property
void tuya_report_int(const char* propertyCode, int val);

#endif
