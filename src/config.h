//
// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/config.h
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TRUTH_HARDWARE.md Sections 0.1, 1, 2, 3, 4
// LOG_REF: 2026-03-30
//

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --------------------------------------------------------------------------
// VERSION INFO
// --------------------------------------------------------------------------

#define FIRMWARE_VERSION "1.1.0"
#define FIRMWARE_NAME "ESP32 Handheld"

// --------------------------------------------------------------------------
// HARDWARE PINS (ESP32-S2-Mini) — per TRUTH_HARDWARE.md
// --------------------------------------------------------------------------

// SPI bus (shared by LCD and SD card) — per TRUTH_HARDWARE.md Section 0.1
#define PIN_SPI_MOSI  35   // GPIO 35 — MOSI (LCD SID + SD MOSI)
#define PIN_SPI_MISO  37   // GPIO 37 — MISO (SD card only, LCD doesn't use)
#define PIN_SPI_SCLK  36   // GPIO 36 — SCK  (LCD E + SD SCK)

// Display (ST7920 128x64, HW SPI) — per TRUTH_HARDWARE.md Section 1
#define PIN_CS        38   // LCD pin 4 (RS) — Chip Select
#define PIN_BACKLIGHT 40   // LCD pin 19 (BLA) — PWM backlight anode
// Note: PIN_DC removed (ST7920 SPI has no DC line)
// Note: PIN_RST removed (RST tied to 3V3 in hardware)

// SD card module (SPI bus) — per TRUTH_HARDWARE.md Section 3
#define PIN_SD_CS     39   // SD card chip select

// Buzzer pins — NOT YET WIRED
// #define PIN_BUZZER_1 ??
// #define PIN_BUZZER_2 ??
// #define PIN_BUZZER_3 ??

// Key Matrix Dimensions
const byte ROWS = 4;
const byte COLS = 5;

// Global hardware pin arrays (Defined in hal.cpp)
extern byte rowPins[ROWS];
extern byte colPins[COLS];
extern char keyMap[ROWS][COLS];

// --------------------------------------------------------------------------
// KEY CODE CONSTANTS
// --------------------------------------------------------------------------
// Layout:
//   ESC,   1,    2,    3,    BKSP
//   TAB,   4,    5,    6,    ENTER
//   SHIFT, 7,    8,    9,    UP
//   ALT,   LEFT, 0,    RIGHT,DOWN

#define KEY_ESC     27    // Escape - exit apps, cancel (ASCII ESC)
#define KEY_BKSP    8     // Backspace - delete (ASCII BS)
#define KEY_TAB     9     // Tab (ASCII TAB)
#define KEY_ENTER   13    // Enter - select, confirm (ASCII CR)
#define KEY_SHIFT   14    // Shift modifier (ASCII SO)
#define KEY_ALT     15    // Alt modifier (ASCII SI)
#define KEY_UP      16    // Arrow up (ASCII DLE)
#define KEY_DOWN    17    // Arrow down (ASCII DC1)
#define KEY_LEFT    18    // Arrow left (ASCII DC2)
#define KEY_RIGHT   19    // Arrow right (ASCII DC3)

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