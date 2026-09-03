#pragma once

#include <Arduino.h>

namespace BeeCoolerSensors {

struct SensorStatus {
    bool internalSht30Detected;
    bool externalSht30Detected;
    bool adxl345Detected;
    uint8_t adxl345DeviceId;
};

struct SensorReading {
    float internalTemperatureC;
    float internalHumidityPercent;
    float externalTemperatureC;
    float externalHumidityPercent;
    float accelerationXG;
    float accelerationYG;
    float accelerationZG;
    float accelerationMagnitudeG;
};

SensorStatus begin(Print &diagnostics);
SensorReading read();

}  // namespace BeeCoolerSensors
