// [Revision: v2.1] [Path: src/hal.h] [Date: 2025-12-11]
// Description: Added key repeat support for non-T9 keys.

#ifndef HAL_H
#define HAL_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

// --------------------------------------------------------------------------
// DISPLAY EXPORTS
// --------------------------------------------------------------------------

extern U8G2_ST7565_ERC12864_F_4W_HW_SPI u8g2;
extern const uint8_t* FONT_SMALL; 
extern int systemContrast; // Global system contrast variable

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