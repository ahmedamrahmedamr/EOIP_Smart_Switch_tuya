#ifndef MEGA_SMART_PRIVATE_H
#define MEGA_SMART_PRIVATE_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

void ClearBuffer(char* buffer);

void onStartOfUpdate(void);
void onProgressOfUpdate(int i1, int i2);
void onEndOfUpdate(void);

#endif
