#ifndef MEGA_SMART_CONFIG_H
#define MEGA_SMART_CONFIG_H

//###################### Pin configuration ###########################
// Target board: ESP32-S3 N16R8 (16 MB Flash, 8 MB octal PSRAM)
// UNSAFE GPIOs on S3 N16R8:
//   GPIO 26-32  → octal PSRAM (DO NOT USE)
//   GPIO 33-37  → octal Flash  (DO NOT USE)
//   GPIO 0      → strapping / BOOT button (safe as INPUT_PULLUP only)
//   GPIO 3,45,46→ strapping pins (use with care)
// All other GPIOs (1-21, 38-48) are safe for general use.

#define FIRMWARE_VERSION "1.5.0"

#define DEBUG_MODE 1

// ── Flash / Boot button ──────────────────────────────────────────────
#define FLASH_BUTTON_PIN    0        // BOOT button – INPUT_PULLUP only
#define FLASH_BUTTON_ACTIVE LOW

// ── Touch / button inputs ────────────────────────────────────────────
#define TOUCH0_PIN  12
#define TOUCH0_ACIVE HIGH

#define TOUCH1_PIN  14
#define TOUCH1_ACIVE HIGH

#define TOUCH2_PIN  13
#define TOUCH2_ACIVE HIGH

// ── Relay outputs ────────────────────────────────────────────────────
#define RELAY0_PIN    4
#define RELAY0_ACTIVE HIGH

#define RELAY1_PIN    5
#define RELAY1_ACTIVE HIGH

// ── RGB LED strip (SK6812) ───────────────────────────────────────────
// GPIO 16 is safe on ESP32-S3 (unlike ESP8266 where it was special)
#define LED_RGB_PIN    16
#define LED_RGB_ACTIVE LOW

// ── Status LED indicator ─────────────────────────────────────────────
// GPIO 2 is safe on ESP32-S3 (not a strapping pin on S3)
#define LED_INDICATOR_PIN    2
#define LED_INDICATOR_ACTIVE LOW

// ── Test LEDs (for board bring-up verification) ──────────────────────
// Connected to GPIO 13 & 14 → set HIGH in setup() to confirm the S3 is running
#define TEST_LED0_PIN  13
#define TEST_LED1_PIN  14



//######################  WiFi configuration  #########################
#define AP_ssid "MegaSmart"
#define AP_password "MegaSmart"
#define WIFI_CONNEECTING_WAITING_COUNT 300  //300 * 50mSec

//######################  AP configuration  #########################

#define AP_TIMEOUT_SEC_MAX 300  // 300 seconds (5 minutes) AP timeout







#endif