#include <Arduino.h>

#include <BeeCoolerSensors.h>

void setup() {
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println("==================================");
    Serial.println("ADXL345 + 2x SHT30");
    Serial.println("==================================");

    BeeCoolerSensors::begin(Serial);
}

void loop() {
    const BeeCoolerSensors::SensorReading reading = BeeCoolerSensors::read();

    Serial.println("----------------------------------");

    Serial.println("SHT30 INTERNO");

    Serial.print("Temperatura: ");

    if (!isnan(reading.internalTemperatureC)) {
        Serial.print(reading.internalTemperatureC, 2);
        Serial.println(" C");
    } else {
        Serial.println("ERRO");
    }

    Serial.print("Umidade: ");

    if (!isnan(reading.internalHumidityPercent)) {
        Serial.print(reading.internalHumidityPercent, 2);
        Serial.println(" %");
    } else {
        Serial.println("ERRO");
    }

    Serial.println();

    Serial.println("SHT30 EXTERNO");

    Serial.print("Temperatura: ");

    if (!isnan(reading.externalTemperatureC)) {
        Serial.print(reading.externalTemperatureC, 2);
        Serial.println(" C");
    } else {
        Serial.println("ERRO");
    }

    Serial.print("Umidade: ");

    if (!isnan(reading.externalHumidityPercent)) {
        Serial.print(reading.externalHumidityPercent, 2);
        Serial.println(" %");
    } else {
        Serial.println("ERRO");
    }

    Serial.println();

    Serial.print("ADXL X: ");
    Serial.print(reading.accelerationXG, 4);
    Serial.print(" g");

    Serial.print(" | Y: ");
    Serial.print(reading.accelerationYG, 4);
    Serial.print(" g");

    Serial.print(" | Z: ");
    Serial.print(reading.accelerationZG, 4);
    Serial.print(" g");

    Serial.print(" | Magnitude: ");
    Serial.print(reading.accelerationMagnitudeG, 4);
    Serial.println(" g");

    delay(500);
}
