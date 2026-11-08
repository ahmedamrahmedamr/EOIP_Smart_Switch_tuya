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







#endif