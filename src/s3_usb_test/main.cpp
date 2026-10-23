#include <Arduino.h>

void setup() {
  Serial.begin(115200);

  // Apaga o LED RGB da placa.
  neopixelWrite(48, 0, 0, 0);

  delay(1000);
  Serial.println("BeeCooler: ESP-S3 iniciou!");
}

void loop() {
  Serial.print("ESP funcionando | segundos: ");
  Serial.println(millis() / 1000);
  delay(1000);
}