#ifndef MEGA_SMART_PRIVATE_H
#define MEGA_SMART_PRIVATE_H





//###################### Board Information ##########################
// #define HARDWARE_VERSION 1
// #define HARDWARE_REVESION 0
// #define FIRMWARE_VERSION 1
// #define FIRMWARE_REVESION 0
//###################################################################

//############################ EEPROM ###############################
/*  EEPROM Size is 4096 bytes from 0x0000 to 0x0FFF
 *  Board Information area  :: 0x0000 ---> 0x00FF == 256 bytes
 *  WiFi Information area   :: 0x0100 ---> 0x01FF == 256 bytes
*/

#define EEPROM_SIZE 1024U


void WriteTo_EEPROM(int const address, uint8_t const val);

//###################################################################


void ClearBuffer(char* buffer);


void setColor(uint8_t redValue, uint8_t greenValue, uint8_t blueValue);

void onStartOfUpdate(void);
void onProgressOfUpdate(int i1, int i2);
void onEndOfUpdate(void);

#endif