#ifndef MEGA_SMART_CONFIG_H
#define MEGA_SMART_CONFIG_H

//###################### Pin configuration ###########################

#define FIRMWARE_VERSION "1.4.0.2"

#define DEBUG_MODE 1

#define FLASH_BUTTON_PIN 0
#define FLASH_BUTTON_ACTIVE LOW


#define TOUCH0_PIN 12
#define TOUCH0_ACIVE HIGH

#define TOUCH1_PIN 14
#define TOUCH1_ACIVE HIGH

#define TOUCH2_PIN 13
#define TOUCH2_ACIVE HIGH


#define RELAY0_PIN 4
#define RELAY0_ACTIVE HIGH

#define RELAY1_PIN 5
#define RELAY1_ACTIVE HIGH

#define LED_RGB_PIN 16
#define LED_RGB_ACTIVE LOW

#define LED_INDICATOR_PIN 2
#define LED_INDICATOR_ACTIVE LOW



//######################  WiFi configuration  #########################
#define AP_ssid "MegaSmart"
#define AP_password "MegaSmart"
#define WIFI_CONNEECTING_WAITING_COUNT 300  //300 * 50mSec

//######################  AP configuration  #########################

#define AP_TIMEOUT_SEC_MAX 60  // in seconds AP time out

//######################  TuyaLink Cloud Configuration  #########################
// Tuya MQTT Broker for Central Europe / Egypt
#define TUYA_MQTT_HOST          "m1.tuyaeu.com"
#define TUYA_MQTT_PORT          8883  // 8883 for TLS/SSL

// Paste your private TuyaLink credentials here:
#define TUYA_PRODUCT_ID         "ctcujxdbfx5a2pl2"
#define TUYA_DEVICE_ID          "268213ddda6ec2c79byy7x"
#define TUYA_DEVICE_SECRET      "alrZv4Z7u1yVIK4M"

// Tuya Property Identifiers (match these with your TuyaLink product schema):
#define TUYA_PROP_SWITCH_1      "switch_1"     // Relay 0
#define TUYA_PROP_SWITCH_2      "switch_2"     // Relay 1
#define TUYA_PROP_SWITCH_LED    "switch_led"    // RGB Light Strip Power
#define TUYA_PROP_COLOUR_DATA   "colour_data"   // RGB Color HSV hex
#define TUYA_PROP_BACKLIGHT     "backlight"     // Touch button backlight LED

#endif