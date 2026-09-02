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
// SHT30 INTERNO - I2C 0
// =========================

#define SHT_INT_SDA 16
#define SHT_INT_SCL 17

// =========================
// SHT30 EXTERNO - I2C 1
// =========================

#define SHT_EXT_SDA 21
#define SHT_EXT_SCL 22

// Wire = I2C 0 do ESP32
// Criamos outro controlador para o segundo sensor
TwoWire I2CExterno = TwoWire(1);

// Cada SHT30 fica associado ao seu proprio barramento
Adafruit_SHT31 shtInterno = Adafruit_SHT31(&Wire);
Adafruit_SHT31 shtExterno = Adafruit_SHT31(&I2CExterno);

bool shtInternoOK = false;
bool shtExternoOK = false;

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
// Funcoes I2C
// =========================

void scanI2C(TwoWire &bus, const char *nome) {
    Serial.println();
    Serial.print("Escaneando ");
    Serial.println(nome);

    int encontrados = 0;

    for (uint8_t endereco = 1; endereco < 127; endereco++) {
        bus.beginTransmission(endereco);
        uint8_t erro = bus.endTransmission();

        if (erro == 0) {
            Serial.print("Dispositivo encontrado em 0x");

            if (endereco < 16) {
                Serial.print("0");
            }

            Serial.println(endereco, HEX);
            encontrados++;
        }
    }

    if (encontrados == 0) {
        Serial.println("NENHUM dispositivo encontrado.");
    }
}

bool iniciarSHT30(Adafruit_SHT31 &sensor, const char *nome) {
    if (sensor.begin(0x44)) {
        Serial.print(nome);
        Serial.println(" detectado em 0x44!");
        return true;
    }

    if (sensor.begin(0x45)) {
        Serial.print(nome);
        Serial.println(" detectado em 0x45!");
        return true;
    }

    Serial.print(nome);
    Serial.println(" NAO detectado.");
    return false;
}

// =========================
// Setup
// =========================

void setup() {
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println("==================================");
    Serial.println("ADXL345 + 2x SHT30");
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

        // Full resolution + +/-2g
        writeADXLRegister(REG_DATA_FORMAT, 0x08);

        // Mantendo 100 Hz por enquanto,
        // pois este ainda eh o codigo de teste.
        writeADXLRegister(REG_BW_RATE, 0x0A);

        // Measurement mode
        writeADXLRegister(REG_POWER_CTL, 0x08);

    } else {
        Serial.println("ERRO: ADXL345 nao detectado.");
    }

    // -------------------------
    // Inicializa I2C INTERNO
    // -------------------------

    Wire.begin(SHT_INT_SDA, SHT_INT_SCL);
    Wire.setClock(100000);

    // -------------------------
    // Inicializa I2C EXTERNO
    // -------------------------

    I2CExterno.begin(SHT_EXT_SDA, SHT_EXT_SCL);
    I2CExterno.setClock(100000);

    delay(500);

    // -------------------------
    // Scanners
    // -------------------------

    scanI2C(Wire, "I2C INTERNO");
    scanI2C(I2CExterno, "I2C EXTERNO");

    Serial.println();

    // -------------------------
    // Inicializa SHT30
    // -------------------------

    shtInternoOK =
        iniciarSHT30(shtInterno, "SHT30 INTERNO");

    shtExternoOK =
        iniciarSHT30(shtExterno, "SHT30 EXTERNO");
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
    // SHT30 INTERNO
    // -------------------------

    float tempInterna = NAN;
    float umidInterna = NAN;

    if (shtInternoOK) {
        tempInterna = shtInterno.readTemperature();
        umidInterna = shtInterno.readHumidity();
    }

    // -------------------------
    // SHT30 EXTERNO
    // -------------------------

    float tempExterna = NAN;
    float umidExterna = NAN;

    if (shtExternoOK) {
        tempExterna = shtExterno.readTemperature();
        umidExterna = shtExterno.readHumidity();
    }

    // -------------------------
    // Serial
    // -------------------------

    Serial.println("----------------------------------");

    Serial.println("SHT30 INTERNO");

    Serial.print("Temperatura: ");

    if (!isnan(tempInterna)) {
        Serial.print(tempInterna, 2);
        Serial.println(" C");
    } else {
        Serial.println("ERRO");
    }

    Serial.print("Umidade: ");

    if (!isnan(umidInterna)) {
        Serial.print(umidInterna, 2);
        Serial.println(" %");
    } else {
        Serial.println("ERRO");
    }

    Serial.println();

    Serial.println("SHT30 EXTERNO");

    Serial.print("Temperatura: ");

    if (!isnan(tempExterna)) {
        Serial.print(tempExterna, 2);
        Serial.println(" C");
    } else {
        Serial.println("ERRO");
    }

    Serial.print("Umidade: ");

    if (!isnan(umidExterna)) {
        Serial.print(umidExterna, 2);
        Serial.println(" %");
    } else {
        Serial.println("ERRO");
    }

    Serial.println();

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