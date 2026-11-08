# Mega Smart ESP32-S3 (N16R8) firmware

Open `Mega_Smart_S3.ino` in Arduino IDE (the folder name must stay `Mega_Smart_S3`).

Libraries: FastLED (3.9+ for Arduino-ESP32 core 3.x), ArduinoJson 7.x. WiFi, HTTPClient, HTTPUpdate, Preferences come with the ESP32 core.

Arduino IDE settings (ESP32 core, board "ESP32S3 Dev Module"):
- Flash Size: 16MB (128Mb)
- PSRAM: OPI PSRAM (the firmware does not use it, but GPIO33-37 stay reserved either way)
- Partition Scheme: any scheme whose name does NOT say "No OTA" (needed for OTA)
- USB CDC On Boot: Disabled if you use the board's UART port (CP210x), Enabled if you use the native USB port
- Upload Speed: 921600 is fine; Serial Monitor: 921600 baud

First boot (blank NVS): the device starts the "MegaSmart" AP (password "MegaSmart"), as the old firmware did.
