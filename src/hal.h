//
// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/hal.h
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TRUTH_HARDWARE.md Sections 0.1, 1, 2, 3
// LOG_REF: 2026-03-30
//

#ifndef HAL_H
#define HAL_H

#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include <U8g2lib.h>
#include "config.h"

// --------------------------------------------------------------------------
// DISPLAY EXPORTS
// --------------------------------------------------------------------------

extern U8G2_ST7920_128X64_F_SW_SPI u8g2;
extern const uint8_t* FONT_SMALL; 
extern int systemContrast;    // Global system contrast variable
extern int systemBrightness;  // Global backlight brightness (0-255)

// --------------------------------------------------------------------------
// SD CARD EXPORTS
// --------------------------------------------------------------------------

// Session-based SD access: LCD uses SW_SPI (GPIO bit-bang), SD needs HW SPI.
// sdBeginSession/sdEndSession acquire/release the shared SPI bus.
// Uses SdFat library (Arduino SD library's ESP-IDF driver fails on S2).
bool sdBeginSession();    // Acquire HW SPI bus + mount SD. LCD won't work until end.
void sdEndSession();      // Release HW SPI bus. LCD resumes.

// Public API (cached values — no SPI bus needed)
bool mountSD();           // Full cycle: acquire, cache info, release. Returns success.
void unmountSD();         // Clear cached state + release if active
bool isSDMounted();       // Card was detected (cached, no SPI needed)
uint64_t sdTotalBytes();  // Cached total SD card space (0 if not detected)
uint64_t sdUsedBytes();   // Cached used SD card space (0 if not detected)

// SdFat filesystem access (only valid between sdBeginSession/sdEndSession)
extern SdFat sdFat;

// --------------------------------------------------------------------------
// INPUT EXPORTS
// --------------------------------------------------------------------------

#define MAX_PRESSED_KEYS 6

extern char activeKeys[MAX_PRESSED_KEYS];
extern int activeKeyCount;

// --------------------------------------------------------------------------
// CORE FUNCTIONS
// --------------------------------------------------------------------------

void setupHardware();           
void scanMatrix();              // Frame-level: finalize latched keys, update prev state
void pollMatrix();              // Inter-frame: lightweight scan, latches detected keys
bool isJustPressed(char key);
bool isRepeating(char key);
bool isKeyHeld(char key);
bool isLongPressed(char key);     // Returns true on repeat interval for held repeatable keys

#endif