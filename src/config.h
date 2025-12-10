// [Revision: v2.2] [Path: src/config.h] [Date: 2025-12-10]
// Description: Added PHYSICAL_FPS and FRAME_DELAY_MS for game loop timing.

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --------------------------------------------------------------------------
// HARDWARE PINS (ESP32-S3)
// --------------------------------------------------------------------------

#define PIN_CS  10
#define PIN_DC  5
#define PIN_RST 4

const byte ROWS = 4;
const byte COLS = 5;

extern byte rowPins[ROWS];
extern byte colPins[COLS];
extern char keyMap[ROWS][COLS];

// --------------------------------------------------------------------------
// TIMING & CONFIG CONSTANTS
// --------------------------------------------------------------------------

#define PHYSICAL_FPS 10
#define FRAME_DELAY_MS (1000 / PHYSICAL_FPS)

#define MULTITAP_TIMEOUT 800   
#define CURSOR_BLINK_RATE 500
#define DEFAULT_CONTRAST 0     

#endif