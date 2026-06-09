// [Revision: v1.0] [Path: emulator_mocks/SPI.h] [Date: 2026-06-09]
// Description: SPI library mock interface for the emulator.

#ifndef EMULATOR_SPI_H
#define EMULATOR_SPI_H

#define FSPI 0
class SPIClass {
public:
    SPIClass(int bus) {}
    void begin(int sclk, int miso, int mosi) {}
    void end() {}
};

#endif // EMULATOR_SPI_H
