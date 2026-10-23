#include "SharedSpi.h"

#include "pins.h"

SPIClass& sharedSpi() {
  static SPIClass spi(FSPI);
  return spi;
}

void sharedSpiBegin() {
  sharedSpi().begin(kPinSpiSck, kPinSpiMiso, kPinSpiMosi, -1);
}
