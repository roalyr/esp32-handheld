// [Revision: v2.3] [Path: src/config.h] [Date: 2025-12-11]
// Description: Global hardware definitions, timing constants, and app ID enum.

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --------------------------------------------------------------------------
// HARDWARE PINS (ESP32-S3)
// --------------------------------------------------------------------------

#define PIN_CS  10
#define PIN_DC  5
#define PIN_RST 4

// Key Matrix Dimensions
const byte ROWS = 4;
const byte COLS = 5;

// Global hardware pin arrays (Defined in hal.cpp)
extern byte rowPins[ROWS];
extern byte colPins[COLS];
extern char keyMap[ROWS][COLS];

// --------------------------------------------------------------------------
// APP ID CONSTANTS (for menu switching)
// --------------------------------------------------------------------------

enum AppId {
    APP_KEY_TESTER = 0,
    APP_GFX_TEST = 1,
    APP_STOPWATCH = 2,
    APP_CLOCK = 3,
    APP_FILE_BROWSER = 4,
    APP_LUA_RUNNER = 5
};

// --------------------------------------------------------------------------
// TIMING & CONFIG CONSTANTS
// --------------------------------------------------------------------------

#define PHYSICAL_FPS 30
#define FRAME_DELAY_MS (1000 / PHYSICAL_FPS)

#define MULTITAP_TIMEOUT 800   
#define CURSOR_BLINK_RATE 500
#define DEFAULT_CONTRAST 0     // Global default contrast (0-255)

#endif