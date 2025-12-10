// [Revision: v1.0] [Path: src/hal.h] [Date: 2025-12-09]
// Description: Hardware Abstraction Layer declarations for Display and Input Matrix.

#ifndef HAL_H
#define HAL_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

// --------------------------------------------------------------------------
// DISPLAY EXPORTS
// --------------------------------------------------------------------------

// Main display object (ST7565 via SPI)
extern U8G2_ST7565_ERC12864_F_4W_HW_SPI u8g2;

// Shared font resource to reduce memory duplication
extern const uint8_t* FONT_SMALL; 

// --------------------------------------------------------------------------
// INPUT EXPORTS
// --------------------------------------------------------------------------

#define MAX_PRESSED_KEYS 6

// Array holding keys currently pressed in this frame
extern char activeKeys[MAX_PRESSED_KEYS];
extern int activeKeyCount;

// --------------------------------------------------------------------------
// CORE FUNCTIONS
// --------------------------------------------------------------------------

void setupHardware();           // Initialize pins and display
void scanMatrix();              // Poll the keypad matrix (call every loop)
bool isJustPressed(char key);   // True if key was not pressed in previous frame

#endif