// [Revision: v1.0] [Path: emulator_mocks/pgmspace.h] [Date: 2026-06-09]
// Description: pgmspace.h mock for supporting flash memory reading macros on desktop.

#ifndef EMULATOR_PGMSPACE_H
#define EMULATOR_PGMSPACE_H

#include "Arduino.h"

#define pgm_read_byte(addr) (*(const uint8_t*)(addr))
#define pgm_read_word(addr) (*(const uint16_t*)(addr))
#define pgm_read_dword(addr) (*(const uint32_t*)(addr))
#define pgm_read_float(addr) (*(const float*)(addr))
#define pgm_read_ptr(addr) (*(const void**)(addr))
#define memcpy_P memcpy

#endif // EMULATOR_PGMSPACE_H
