#ifndef TUYA_DP_HANDLER_H
#define TUYA_DP_HANDLER_H

#include <Arduino.h>

// Initialize the DP handler (registers callback with tuya_cloud)
void tuya_handler_init();

// Called when Tuya sends a command from the cloud
void tuya_on_property_command(const String& propertyName, const String& valueJson);

// Sync all current states (relays, RGB, backlight) to Tuya Cloud
void tuya_sync_states();

// Helper called when hardware touch button toggles relay 0 (switch_1)
void tuya_notify_relay0(bool state);

// Helper called when hardware touch button toggles relay 1 (switch_2)
void tuya_notify_relay1(bool state);

// Helper called when hardware touch button toggles RGB light switch
void tuya_notify_rgb_switch(bool state);

// Helper to notify backlight state change
void tuya_notify_backlight(bool state);

#endif
