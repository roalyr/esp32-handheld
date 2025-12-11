// [Revision: v2.5] [Path: src/config.h] [Date: 2025-12-11]
// Description: Global hardware definitions, timing, app IDs, version, sleep config.

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --------------------------------------------------------------------------
// VERSION INFO
// --------------------------------------------------------------------------

#define FIRMWARE_VERSION "1.0.0"
#define FIRMWARE_NAME "ESP32 Handheld"

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
    APP_LUA_RUNNER = 5,
    APP_SETTINGS = 6
};

// --------------------------------------------------------------------------
// TIMING & CONFIG CONSTANTS
// --------------------------------------------------------------------------

#define PHYSICAL_FPS 30
#define FRAME_DELAY_MS (1000 / PHYSICAL_FPS)

#define MULTITAP_TIMEOUT 800   
#define CURSOR_BLINK_RATE 500
#define DEFAULT_CONTRAST 0     // Global default contrast (0-255)

// Key repeat settings (for non-T9 keys like arrows/nav)
#define KEY_REPEAT_DELAY_MS  400   // Initial delay before repeat starts
#define KEY_REPEAT_RATE_MS   100   // Interval between repeats

// Sleep/power settings
#define SLEEP_TIMEOUT_MS     60000  // 60 seconds of inactivity before sleep
#define SLEEP_ENABLED        true   // Enable sleep mode

#endif