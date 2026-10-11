#include <Arduino.h>

// Native USB CDC <-> UART1. No status messages on the binary data stream.
// C3 GPIO21 (TX) -> ESP8266 RX; GPIO20 (RX) <- ESP8266 TX; common GND.
// UART baud is fixed: set the ESP8266 uploader to 57600 as well.
constexpr int BRIDGE_RX = 20;
constexpr int BRIDGE_TX = 21;
constexpr uint32_t BRIDGE_BAUD = 57600;

void setup() {
  Serial.setRxBufferSize(16384);
  Serial.setTxBufferSize(4096);
  Serial1.setRxBufferSize(4096);
  Serial1.begin(BRIDGE_BAUD, SERIAL_8N1, BRIDGE_RX, BRIDGE_TX);
  Serial.begin(BRIDGE_BAUD);
}

void loop() {
  uint8_t data[256];
  int count = min(Serial.available(), Serial1.availableForWrite());
  count = min(count, static_cast<int>(sizeof(data)));
  if (count > 0) {
    size_t received = Serial.read(data, count);
    Serial1.write(data, received);
  }
  count = min(Serial1.available(), Serial.availableForWrite());
  count = min(count, static_cast<int>(sizeof(data)));
  if (count > 0) {
    size_t received = Serial1.read(data, count);
    Serial.write(data, received);
  }
  delay(1);
}
