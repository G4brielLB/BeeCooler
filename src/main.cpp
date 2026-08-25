#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>

// =========================
// ADXL345 - SPI
// =========================

#define ADXL_CS   5
#define ADXL_SCK  18
#define ADXL_MISO 19
#define ADXL_MOSI 23

SPISettings adxlSPI(1000000, MSBFIRST, SPI_MODE3);

#define REG_DEVID       0x00
#define REG_BW_RATE     0x2C
#define REG_POWER_CTL   0x2D
#define REG_DATA_FORMAT 0x31
#define REG_DATAX0      0x32

// =========================
// SHT30 - I2C
// =========================

#define SHT_SDA 21
#define SHT_SCL 22

Adafruit_SHT31 sht30 = Adafruit_SHT31();

// =========================
// Funcoes ADXL345
// =========================

void writeADXLRegister(uint8_t reg, uint8_t value) {
    SPI.beginTransaction(adxlSPI);

    digitalWrite(ADXL_CS, LOW);

    SPI.transfer(reg);
    SPI.transfer(value);

    digitalWrite(ADXL_CS, HIGH);

    SPI.endTransaction();
}

uint8_t readADXLRegister(uint8_t reg) {
    SPI.beginTransaction(adxlSPI);

    digitalWrite(ADXL_CS, LOW);

    SPI.transfer(reg | 0x80);
    uint8_t value = SPI.transfer(0x00);

    digitalWrite(ADXL_CS, HIGH);

    SPI.endTransaction();

    return value;
}

void readADXLXYZ(int16_t &x, int16_t &y, int16_t &z) {
    uint8_t data[6];

    SPI.beginTransaction(adxlSPI);

    digitalWrite(ADXL_CS, LOW);

    // READ + MULTI-BYTE
    SPI.transfer(REG_DATAX0 | 0x80 | 0x40);

    for (int i = 0; i < 6; i++) {
        data[i] = SPI.transfer(0x00);
    }

    digitalWrite(ADXL_CS, HIGH);

    SPI.endTransaction();

    x = (int16_t)((data[1] << 8) | data[0]);
    y = (int16_t)((data[3] << 8) | data[2]);
    z = (int16_t)((data[5] << 8) | data[4]);
}

// =========================
// Setup
// =========================

void setup() {
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println("==================================");
    Serial.println("Teste ADXL345 + SHT30");
    Serial.println("==================================");

    // -------------------------
    // Inicializa ADXL345
    // -------------------------

    pinMode(ADXL_CS, OUTPUT);
    digitalWrite(ADXL_CS, HIGH);

    SPI.begin(
        ADXL_SCK,
        ADXL_MISO,
        ADXL_MOSI,
        ADXL_CS
    );

    delay(100);

    uint8_t id = readADXLRegister(REG_DEVID);

    Serial.print("ADXL345 DEVID: 0x");
    Serial.println(id, HEX);

    if (id == 0xE5) {
        Serial.println("ADXL345 detectado!");

        // Full resolution + ±2g
        writeADXLRegister(REG_DATA_FORMAT, 0x08);

        // 100 Hz
        writeADXLRegister(REG_BW_RATE, 0x0A);

        // Measurement mode
        writeADXLRegister(REG_POWER_CTL, 0x08);

    } else {
        Serial.println("ERRO: ADXL345 nao detectado.");
    }

    // -------------------------
// Inicializa SHT30 / I2C
// -------------------------

Wire.begin(SHT_SDA, SHT_SCL);
Wire.setClock(100000);

delay(500);

Serial.println();
Serial.println("Escaneando barramento I2C...");

int encontrados = 0;

for (uint8_t endereco = 1; endereco < 127; endereco++) {
    Wire.beginTransmission(endereco);
    uint8_t erro = Wire.endTransmission();

    if (erro == 0) {
        Serial.print("Dispositivo encontrado em 0x");

        if (endereco < 16)
            Serial.print("0");

        Serial.println(endereco, HEX);
        encontrados++;
    }
}

if (encontrados == 0) {
    Serial.println("NENHUM dispositivo I2C encontrado!");
}

Serial.println();

if (sht30.begin(0x44)) {
    Serial.println("SHT30 detectado em 0x44!");
} else if (sht30.begin(0x45)) {
    Serial.println("SHT30 detectado em 0x45!");
} else {
    Serial.println("SHT30 nao detectado.");
}
}

// =========================
// Loop
// =========================

void loop() {
    // -------------------------
    // ADXL345
    // -------------------------

    int16_t rawX, rawY, rawZ;

    readADXLXYZ(rawX, rawY, rawZ);

    const float scale = 0.0039f;

    float x = rawX * scale;
    float y = rawY * scale;
    float z = rawZ * scale;

    float magnitude = sqrt(
        x * x +
        y * y +
        z * z
    );

    // -------------------------
    // SHT30
    // -------------------------

    float temperatura = sht30.readTemperature();
    float umidade = sht30.readHumidity();

    // -------------------------
    // Serial
    // -------------------------

    Serial.println("----------------------------------");

    Serial.print("Temperatura: ");

    if (!isnan(temperatura)) {
        Serial.print(temperatura, 2);
        Serial.println(" °C");
    } else {
        Serial.println("ERRO");
    }

    Serial.print("Umidade: ");

    if (!isnan(umidade)) {
        Serial.print(umidade, 2);
        Serial.println(" %");
    } else {
        Serial.println("ERRO");
    }

    Serial.print("ADXL X: ");
    Serial.print(x, 4);
    Serial.print(" g");

    Serial.print(" | Y: ");
    Serial.print(y, 4);
    Serial.print(" g");

    Serial.print(" | Z: ");
    Serial.print(z, 4);
    Serial.print(" g");

    Serial.print(" | Magnitude: ");
    Serial.print(magnitude, 4);
    Serial.println(" g");

    delay(500);
}