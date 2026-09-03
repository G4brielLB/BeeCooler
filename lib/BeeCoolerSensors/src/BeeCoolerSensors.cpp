#include "BeeCoolerSensors.h"

#include <Adafruit_SHT31.h>
#include <SPI.h>
#include <Wire.h>

namespace {

constexpr uint8_t ADXL_CS = 5;
constexpr uint8_t ADXL_SCK = 18;
constexpr uint8_t ADXL_MISO = 19;
constexpr uint8_t ADXL_MOSI = 23;

constexpr uint32_t ADXL_SPI_CLOCK_HZ = 1000000;
SPISettings adxlSpiSettings(ADXL_SPI_CLOCK_HZ, MSBFIRST, SPI_MODE3);

constexpr uint8_t REG_DEVID = 0x00;
constexpr uint8_t REG_BW_RATE = 0x2C;
constexpr uint8_t REG_POWER_CTL = 0x2D;
constexpr uint8_t REG_DATA_FORMAT = 0x31;
constexpr uint8_t REG_DATAX0 = 0x32;

constexpr uint8_t SHT_INTERNAL_SDA = 16;
constexpr uint8_t SHT_INTERNAL_SCL = 17;
constexpr uint8_t SHT_EXTERNAL_SDA = 21;
constexpr uint8_t SHT_EXTERNAL_SCL = 22;

constexpr uint32_t SHT_I2C_CLOCK_HZ = 100000;
constexpr float ADXL_SCALE_G_PER_LSB = 0.0039f;

// Separate controllers are required because both encapsulated probes may use
// the same fixed I2C address.
TwoWire externalI2c = TwoWire(1);
Adafruit_SHT31 internalSht30 = Adafruit_SHT31(&Wire);
Adafruit_SHT31 externalSht30 = Adafruit_SHT31(&externalI2c);

bool internalSht30Detected = false;
bool externalSht30Detected = false;

void writeAdxlRegister(uint8_t reg, uint8_t value) {
    SPI.beginTransaction(adxlSpiSettings);

    digitalWrite(ADXL_CS, LOW);
    SPI.transfer(reg);
    SPI.transfer(value);
    digitalWrite(ADXL_CS, HIGH);

    SPI.endTransaction();
}

uint8_t readAdxlRegister(uint8_t reg) {
    SPI.beginTransaction(adxlSpiSettings);

    digitalWrite(ADXL_CS, LOW);
    SPI.transfer(reg | 0x80);
    const uint8_t value = SPI.transfer(0x00);
    digitalWrite(ADXL_CS, HIGH);

    SPI.endTransaction();

    return value;
}

void readAdxlXyz(int16_t &x, int16_t &y, int16_t &z) {
    uint8_t data[6];

    SPI.beginTransaction(adxlSpiSettings);

    digitalWrite(ADXL_CS, LOW);

    // ADXL345 SPI read and multi-byte bits are required for the six data bytes.
    SPI.transfer(REG_DATAX0 | 0x80 | 0x40);

    for (int index = 0; index < 6; index++) {
        data[index] = SPI.transfer(0x00);
    }

    digitalWrite(ADXL_CS, HIGH);
    SPI.endTransaction();

    x = static_cast<int16_t>((data[1] << 8) | data[0]);
    y = static_cast<int16_t>((data[3] << 8) | data[2]);
    z = static_cast<int16_t>((data[5] << 8) | data[4]);
}

void scanI2c(TwoWire &bus, const char *name, Print &diagnostics) {
    diagnostics.println();
    diagnostics.print("Escaneando ");
    diagnostics.println(name);

    int detectedCount = 0;

    for (uint8_t address = 1; address < 127; address++) {
        bus.beginTransmission(address);
        const uint8_t error = bus.endTransmission();

        if (error == 0) {
            diagnostics.print("Dispositivo encontrado em 0x");

            if (address < 16) {
                diagnostics.print("0");
            }

            diagnostics.println(address, HEX);
            detectedCount++;
        }
    }

    if (detectedCount == 0) {
        diagnostics.println("NENHUM dispositivo encontrado.");
    }
}

bool beginSht30(Adafruit_SHT31 &sensor, const char *name, Print &diagnostics) {
    if (sensor.begin(0x44)) {
        diagnostics.print(name);
        diagnostics.println(" detectado em 0x44!");
        return true;
    }

    if (sensor.begin(0x45)) {
        diagnostics.print(name);
        diagnostics.println(" detectado em 0x45!");
        return true;
    }

    diagnostics.print(name);
    diagnostics.println(" NAO detectado.");
    return false;
}

}  // namespace

namespace BeeCoolerSensors {

SensorStatus begin(Print &diagnostics) {
    pinMode(ADXL_CS, OUTPUT);
    digitalWrite(ADXL_CS, HIGH);

    SPI.begin(ADXL_SCK, ADXL_MISO, ADXL_MOSI, ADXL_CS);

    delay(100);

    const uint8_t adxlDeviceId = readAdxlRegister(REG_DEVID);
    const bool adxlDetected = adxlDeviceId == 0xE5;

    diagnostics.print("ADXL345 DEVID: 0x");
    diagnostics.println(adxlDeviceId, HEX);

    if (adxlDetected) {
        diagnostics.println("ADXL345 detectado!");

        // Full resolution and +/-2 g, as validated on the bench hardware.
        writeAdxlRegister(REG_DATA_FORMAT, 0x08);

        // The 100 Hz ODR remains the current bench-test setting.
        writeAdxlRegister(REG_BW_RATE, 0x0A);

        writeAdxlRegister(REG_POWER_CTL, 0x08);
    } else {
        diagnostics.println("ERRO: ADXL345 nao detectado.");
    }

    Wire.begin(SHT_INTERNAL_SDA, SHT_INTERNAL_SCL);
    Wire.setClock(SHT_I2C_CLOCK_HZ);

    externalI2c.begin(SHT_EXTERNAL_SDA, SHT_EXTERNAL_SCL);
    externalI2c.setClock(SHT_I2C_CLOCK_HZ);

    delay(500);

    scanI2c(Wire, "I2C INTERNO", diagnostics);
    scanI2c(externalI2c, "I2C EXTERNO", diagnostics);

    diagnostics.println();

    internalSht30Detected =
        beginSht30(internalSht30, "SHT30 INTERNO", diagnostics);
    externalSht30Detected =
        beginSht30(externalSht30, "SHT30 EXTERNO", diagnostics);

    return {
        internalSht30Detected,
        externalSht30Detected,
        adxlDetected,
        adxlDeviceId,
    };
}

SensorReading read() {
    int16_t rawX;
    int16_t rawY;
    int16_t rawZ;
    readAdxlXyz(rawX, rawY, rawZ);

    SensorReading reading = {
        NAN,
        NAN,
        NAN,
        NAN,
        rawX * ADXL_SCALE_G_PER_LSB,
        rawY * ADXL_SCALE_G_PER_LSB,
        rawZ * ADXL_SCALE_G_PER_LSB,
        0.0f,
    };

    reading.accelerationMagnitudeG = sqrt(
        reading.accelerationXG * reading.accelerationXG +
        reading.accelerationYG * reading.accelerationYG +
        reading.accelerationZG * reading.accelerationZG
    );

    if (internalSht30Detected) {
        reading.internalTemperatureC = internalSht30.readTemperature();
        reading.internalHumidityPercent = internalSht30.readHumidity();
    }

    if (externalSht30Detected) {
        reading.externalTemperatureC = externalSht30.readTemperature();
        reading.externalHumidityPercent = externalSht30.readHumidity();
    }

    return reading;
}

}  // namespace BeeCoolerSensors
