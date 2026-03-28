//
// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/hal.h
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TRUTH_HARDWARE.md Sections 1, 2
// LOG_REF: 2026-03-28
//

#ifndef HAL_H
#define HAL_H

#include <Arduino.h>
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
// INPUT EXPORTS
// --------------------------------------------------------------------------

#define MAX_PRESSED_KEYS 6

extern char activeKeys[MAX_PRESSED_KEYS];
extern int activeKeyCount;

// --------------------------------------------------------------------------
// CORE FUNCTIONS
// --------------------------------------------------------------------------

void setupHardware();           
void scanMatrix();              
bool isJustPressed(char key);
bool isRepeating(char key);     // Returns true on repeat interval for held repeatable keys

#endif