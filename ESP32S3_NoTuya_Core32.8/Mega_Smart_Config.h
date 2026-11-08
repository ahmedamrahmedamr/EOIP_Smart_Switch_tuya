#ifndef MEGA_SMART_CONFIG_H
#define MEGA_SMART_CONFIG_H

//###################### Firmware ###########################
#define FIRMWARE_VERSION "1.4.0.2"
#define DEBUG_MODE 1
#define SERIAL_BAUD 921600

//###################### Pin configuration (ESP32-S3 N16R8) ###########################
// Same pin map as the ESP8266 board.
// Do NOT use: GPIO26-32 (flash), GPIO33-37 (octal PSRAM on N16R8), GPIO19/20 (USB),
//             GPIO43/44 (UART0 = serial monitor), GPIO3/45/46 (strap pins).

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

//######################  Scheduling (replaces the ESP8266 Tickers)  #########################
#define TOUCH_POLL_MS 50      // was tickerMonitorTouchSensors.attach(0.05, ...)
#define SEC_TICK_MS 1000      // was tickerResetPin.attach(1, secCounter)
#define LED_TICK_SEC 0.1f     // LED indicator pattern step

//######################  UDP / storage  #########################
#define PACKET_BUF_SIZE 1024
#define NVS_NAMESPACE "megasmart"

//######################  OTA (separate path so ESP8266 units never fetch the S3 binary)  #########################
// Upload the sketch .bin (not the merged bin) and a text file holding only the version string here.
#define FIRMWARE_DOWNLOAD_URL "https://firebasestorage.googleapis.com/v0/b/smart-home-aae4e.appspot.com/o/firmware-update%2Fswitch-esp32s3%2Ffirmware.ino.bin?alt=media"
#define FIRMWARE_VERSION_URL "https://firebasestorage.googleapis.com/v0/b/smart-home-aae4e.appspot.com/o/firmware-update%2Fswitch-esp32s3%2Ffirmware_version.txt?alt=media"

#endif
