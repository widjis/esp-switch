# C3 flashing bridge

Source copied from the physically tested C3 bridge. Arduino ESP32 core 3.3.12, ESP32C3 Dev Module, USB CDC On Boot enabled. UART 57600 baud: TX GPIO21, RX GPIO20. USB receive buffer 16 KB; transmit buffer 4 KB.

Upload this sketch to C3 first. USB DTR/RTS do not control the ESP8266 boot/reset pins. Enter ESP8266 download mode manually with IO0–GND and RESET. Connect TX to RX, RX to TX, and common GND; disconnect all BA129 mains wiring during USB/UART use.

Local evidence: ESP8266EX detection, stub upload, 4 MB flash identification and 64 KB read succeeded. The firmware in `src/main.cpp` is a separate application for the BA129, not this bridge.
