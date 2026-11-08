#ifndef TUYA_CLOUD_H
#define TUYA_CLOUD_H

#include <Arduino.h>

void tuya_cloud_init();
void tuya_loop();
void tuya_sync_states();
bool tuya_is_connected();

#endif // TUYA_CLOUD_H
