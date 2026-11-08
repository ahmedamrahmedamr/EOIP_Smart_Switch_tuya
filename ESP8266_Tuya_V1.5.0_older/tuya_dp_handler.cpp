#include "tuya_dp_handler.h"
#include "tuya_cloud.h"
#include "Mega_Smart_Config.h"
#include "Mega_Smart_Interface.h"
#include "Mega_Smart_Private.h"
#include <ArduinoJson.h>

extern JsonDocument deviceInfo;
extern void setColor(uint8_t redValue, uint8_t greenValue, uint8_t blueValue);
extern void saveDataToEEPROM();

// Helper: Convert HSV (Tuya H: 0-360, S: 0-1000, V: 0-1000) to RGB (0-255)
static void hsvToRgb(uint16_t h, uint16_t s, uint16_t v, uint8_t& r, uint8_t& g, uint8_t& b) {
  float hf = (float)h;
  float sf = (float)s / 1000.0f;
  float vf = (float)v / 1000.0f;

  float c = vf * sf;
  float x = c * (1.0f - fabs(fmod(hf / 60.0f, 2.0f) - 1.0f));
  float m = vf - c;

  float rf = 0, gf = 0, bf = 0;

  if (hf >= 0 && hf < 60) {
    rf = c; gf = x; bf = 0;
  } else if (hf >= 60 && hf < 120) {
    rf = x; gf = c; bf = 0;
  } else if (hf >= 120 && hf < 180) {
    rf = 0; gf = c; bf = x;
  } else if (hf >= 180 && hf < 240) {
    rf = 0; gf = x; bf = c;
  } else if (hf >= 240 && hf < 300) {
    rf = x; gf = 0; bf = c;
  } else if (hf >= 300 && hf <= 360) {
    rf = c; gf = 0; bf = x;
  }

  r = (uint8_t)((rf + m) * 255.0f);
  g = (uint8_t)((gf + m) * 255.0f);
  b = (uint8_t)((bf + m) * 255.0f);
}

// Convert 12-char hex string "HHHHSSSSVVVV" to RGB
static void parseTuyaHsv(const String& hsvHex, uint8_t& r, uint8_t& g, uint8_t& b) {
  if (hsvHex.length() < 12) return;

  uint16_t h = (uint16_t) strtol(hsvHex.substring(0, 4).c_str(), NULL, 16);
  uint16_t s = (uint16_t) strtol(hsvHex.substring(4, 8).c_str(), NULL, 16);
  uint16_t v = (uint16_t) strtol(hsvHex.substring(8, 12).c_str(), NULL, 16);

  hsvToRgb(h, s, v, r, g, b);
}

void tuya_handler_init() {
  tuya_init(tuya_on_property_command);
}

void tuya_on_property_command(const String& propertyName, const String& valueJson) {
  // 1. Relay 0 (switch_1)
  if (propertyName == TUYA_PROP_SWITCH_1) {
    bool state = (valueJson == "true" || valueJson == "1");
    deviceInfo["sw0"] = state ? 1 : 0;
    digitalWrite(RELAY0_PIN, state ? RELAY0_ACTIVE : !RELAY0_ACTIVE);
    tuya_report_bool(TUYA_PROP_SWITCH_1, state);
    Serial.printf("[Tuya] Switch 1 set to %d\n", state);
  }
  // 2. Relay 1 (switch_2)
  else if (propertyName == TUYA_PROP_SWITCH_2) {
    bool state = (valueJson == "true" || valueJson == "1");
    deviceInfo["sw2"] = state ? 1 : 0;
    digitalWrite(RELAY1_PIN, state ? RELAY1_ACTIVE : !RELAY1_ACTIVE);
    tuya_report_bool(TUYA_PROP_SWITCH_2, state);
    Serial.printf("[Tuya] Switch 2 set to %d\n", state);
  }
  // 3. RGB Light Strip Power (switch_led)
  else if (propertyName == TUYA_PROP_SWITCH_LED) {
    bool state = (valueJson == "true" || valueJson == "1");
    deviceInfo["sw1"] = state ? 1 : 0;
    digitalWrite(LED_RGB_PIN, state ? LED_RGB_ACTIVE : !LED_RGB_ACTIVE);

    if (state) {
      setColor((uint8_t)deviceInfo["green"].as<int>(), (uint8_t)deviceInfo["red"].as<int>(), (uint8_t)deviceInfo["blue"].as<int>());
    } else {
      setColor(0, 0, 0);
    }
    tuya_report_bool(TUYA_PROP_SWITCH_LED, state);
    Serial.printf("[Tuya] RGB Strip Power set to %d\n", state);
  }
  // 4. RGB Color (colour_data)
  else if (propertyName == TUYA_PROP_COLOUR_DATA) {
    String cleanHex = valueJson;
    cleanHex.replace("\"", ""); // Remove quotes if present
    uint8_t r = 0, g = 0, b = 0;
    parseTuyaHsv(cleanHex, r, g, b);

    deviceInfo["red"] = r;
    deviceInfo["green"] = g;
    deviceInfo["blue"] = b;

    if (deviceInfo["sw1"].as<bool>()) {
      setColor(g, r, b); // Note: existing code uses green, red, blue order
    }
    tuya_report_string(TUYA_PROP_COLOUR_DATA, cleanHex.c_str());
    Serial.printf("[Tuya] RGB Color set to R:%d G:%d B:%d\n", r, g, b);
  }
  // 5. Backlight (backlight)
  else if (propertyName == TUYA_PROP_BACKLIGHT) {
    bool state = (valueJson == "true" || valueJson == "1");
    deviceInfo["led"] = state ? 1 : 0;
    saveDataToEEPROM();
    tuya_report_bool(TUYA_PROP_BACKLIGHT, state);
    Serial.printf("[Tuya] Backlight set to %d\n", state);
  }
}

void tuya_sync_states() {
  tuya_report_bool(TUYA_PROP_SWITCH_1, deviceInfo["sw0"].as<bool>());
  tuya_report_bool(TUYA_PROP_SWITCH_2, deviceInfo["sw2"].as<bool>());
  tuya_report_bool(TUYA_PROP_SWITCH_LED, deviceInfo["sw1"].as<bool>());
  tuya_report_bool(TUYA_PROP_BACKLIGHT, deviceInfo["led"].as<bool>());
}

void tuya_notify_relay0(bool state) {
  tuya_report_bool(TUYA_PROP_SWITCH_1, state);
}

void tuya_notify_relay1(bool state) {
  tuya_report_bool(TUYA_PROP_SWITCH_2, state);
}

void tuya_notify_rgb_switch(bool state) {
  tuya_report_bool(TUYA_PROP_SWITCH_LED, state);
}

void tuya_notify_backlight(bool state) {
  tuya_report_bool(TUYA_PROP_BACKLIGHT, state);
}
