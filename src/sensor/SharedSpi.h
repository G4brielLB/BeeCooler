#pragma once

#include <SPI.h>

// ADXL345 and the microSD share one SPI bus (v1.2, 10.2: the RAW window stays
// in RAM and is only written to the SD after acquisition, so there is no
// contention).
SPIClass& sharedSpi();
void sharedSpiBegin();
