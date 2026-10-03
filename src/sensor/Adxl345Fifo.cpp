#include "Adxl345Fifo.h"

#include <driver/gpio.h>
#include <esp_sleep.h>

#include "SharedSpi.h"
#include "config.h"

namespace {

constexpr uint8_t REG_DEVID = 0x00;
constexpr uint8_t REG_THRESH_ACT = 0x24;
constexpr uint8_t REG_ACT_INACT_CTL = 0x27;
constexpr uint8_t REG_BW_RATE = 0x2C;
constexpr uint8_t REG_POWER_CTL = 0x2D;
constexpr uint8_t REG_INT_ENABLE = 0x2E;
constexpr uint8_t REG_INT_MAP = 0x2F;
constexpr uint8_t REG_INT_SOURCE = 0x30;
constexpr uint8_t REG_DATA_FORMAT = 0x31;
constexpr uint8_t REG_DATAX0 = 0x32;
constexpr uint8_t REG_FIFO_CTL = 0x38;
constexpr uint8_t REG_FIFO_STATUS = 0x39;

constexpr uint8_t DEVID_VALUE = 0xE5;
constexpr uint8_t INT_WATERMARK = 0x02;
constexpr uint8_t INT_OVERRUN = 0x01;
constexpr uint8_t INT_ACTIVITY = 0x10;
constexpr uint8_t POWER_MEASURE = 0x08;
constexpr uint8_t POWER_STANDBY = 0x00;

// FIFO_CTL: stream mode (0b10 << 6) | watermark samples (v1.2: 0x98 for 24).
constexpr uint8_t FIFO_STREAM = 0x80;

// 24 samples at 1600 Hz fill in 15 ms; 50 ms is far beyond a healthy wake.
constexpr uint64_t kLightSleepGuardUs = 50000ULL;

SPISettings gSettings(kAdxlSpiHz, MSBFIRST, SPI_MODE3);

}  // namespace

void Adxl345Fifo::begin(int cs, int int1, int int2) {
  cs_ = cs;
  int1_ = int1;
  int2_ = int2;
  pinMode(cs_, OUTPUT);
  digitalWrite(cs_, HIGH);
  pinMode(int1_, INPUT);
  if (int2_ >= 0) pinMode(int2_, INPUT);
}

void Adxl345Fifo::write(uint8_t reg, uint8_t value) {
  SPIClass& spi = sharedSpi();
  spi.beginTransaction(gSettings);
  digitalWrite(cs_, LOW);
  spi.transfer(reg);
  spi.transfer(value);
  digitalWrite(cs_, HIGH);
  spi.endTransaction();
}

uint8_t Adxl345Fifo::read(uint8_t reg) {
  SPIClass& spi = sharedSpi();
  spi.beginTransaction(gSettings);
  digitalWrite(cs_, LOW);
  spi.transfer(reg | 0x80);
  const uint8_t v = spi.transfer(0x00);
  digitalWrite(cs_, HIGH);
  spi.endTransaction();
  return v;
}

// One FIFO entry per transaction, with CS high for >5 us between pops.
void Adxl345Fifo::readEntry(int16_t* xyz) {
  uint8_t raw[6];
  SPIClass& spi = sharedSpi();
  spi.beginTransaction(gSettings);
  digitalWrite(cs_, LOW);
  spi.transfer(REG_DATAX0 | 0x80 | 0x40);
  for (uint8_t i = 0; i < 6; ++i) raw[i] = spi.transfer(0x00);
  digitalWrite(cs_, HIGH);
  spi.endTransaction();
  for (uint8_t a = 0; a < 3; ++a) {
    xyz[a] = static_cast<int16_t>((raw[2 * a + 1] << 8) | raw[2 * a]);
  }
  delayMicroseconds(5);
}

bool Adxl345Fifo::probe() { return read(REG_DEVID) == DEVID_VALUE; }

void Adxl345Fifo::standby() { write(REG_POWER_CTL, POWER_STANDBY); }

AdxlWindow Adxl345Fifo::capture(int16_t* buffer, uint32_t max_samples,
                                uint32_t timeout_ms, bool arm_activity) {
  AdxlWindow w = {};
  w.device_ok = probe();
  if (!w.device_ok || buffer == nullptr) return w;

  // A latched activity event from the period between windows (D51).
  if (kMechEventEnabled) {
    w.activity = (read(REG_INT_SOURCE) & INT_ACTIVITY) != 0;
  }

  write(REG_POWER_CTL, POWER_STANDBY);
  write(REG_INT_ENABLE, 0x00);
  write(REG_BW_RATE, kAdxlOdrCode);  // 1600 Hz, normal power
  write(REG_DATA_FORMAT, 0x08);      // full resolution, +-2 g
  write(REG_FIFO_CTL, 0x00);         // bypass: flush the FIFO
  write(REG_FIFO_CTL, FIFO_STREAM | kAdxlWatermark);
  write(REG_INT_MAP, 0x00);          // every interrupt on INT1
  read(REG_INT_SOURCE);              // clear pending flags
  write(REG_INT_ENABLE, INT_WATERMARK);

#ifndef BEE_ADXL_NO_INT1
  esp_sleep_enable_gpio_wakeup();
  gpio_wakeup_enable(static_cast<gpio_num_t>(int1_), GPIO_INTR_HIGH_LEVEL);
#endif

  const uint32_t start = millis();
  write(REG_POWER_CTL, POWER_MEASURE);

  uint32_t captured = 0;
  while (captured < max_samples) {
    if (millis() - start > timeout_ms) break;

#ifdef BEE_ADXL_NO_INT1
    // Bench build without INT1 wired: poll the FIFO instead of light-sleeping.
    // 5 ms is ~8 samples at 1600 Hz, well inside the 32-entry FIFO.
    delay(5);
#else
    // Sleep until the watermark; INT1 stays high until the FIFO is drained.
    // The timer is a safety net: with a dead ADXL (no INT1) the node would
    // otherwise sleep forever inside the window and the timeout never fires.
    if (digitalRead(int1_) == LOW) {
      esp_sleep_enable_timer_wakeup(kLightSleepGuardUs);
      esp_light_sleep_start();
    }
#endif

    const uint8_t source = read(REG_INT_SOURCE);
    if (source & INT_OVERRUN) ++w.overruns;

    uint8_t entries = read(REG_FIFO_STATUS) & 0x3F;
    while (entries > 0 && captured < max_samples) {
      readEntry(buffer + 3U * captured);
      ++captured;
      --entries;
    }
  }

#ifndef BEE_ADXL_NO_INT1
  gpio_wakeup_disable(static_cast<gpio_num_t>(int1_));
#endif
  write(REG_INT_ENABLE, 0x00);
  w.samples = captured;
  w.complete = captured >= max_samples;
  w.duration_ms = millis() - start;

  if (arm_activity && kMechEventEnabled) {
    // Low-power activity detection between windows: ~12.5 Hz in low-power
    // mode, AC-coupled threshold of ~0.5 g, event on INT2 and latched in
    // INT_SOURCE until the next window.
    write(REG_POWER_CTL, POWER_STANDBY);
    write(REG_BW_RATE, 0x17);
    write(REG_THRESH_ACT, 0x08);
    write(REG_ACT_INACT_CTL, 0xF0);
    write(REG_INT_MAP, INT_ACTIVITY);  // activity -> INT2
    write(REG_INT_ENABLE, INT_ACTIVITY);
    write(REG_POWER_CTL, POWER_MEASURE);
  } else {
    write(REG_POWER_CTL, POWER_STANDBY);
  }
  return w;
}
