// [Revision: v1.0] [Path: src/config.h] [Date: 2025-12-09]
// Description: Global hardware definitions, timing constants, and system mode enums.

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --------------------------------------------------------------------------
// HARDWARE PINS (ESP32-S3)
// --------------------------------------------------------------------------

// Display (ST7565 / ERC12864-4)
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
// TIMING CONSTANTS
// --------------------------------------------------------------------------

#define MULTITAP_TIMEOUT 800   // Max time (ms) between presses for T9 cycling
#define CURSOR_BLINK_RATE 500  // Blinking cursor interval (ms)

// --------------------------------------------------------------------------
// SYSTEM STATES
// --------------------------------------------------------------------------

enum SystemMode {
  MODE_T9_EDITOR,
  MODE_KEY_TESTER,
  MODE_SNAKE,
  MODE_GFX_TEST
};

#endif