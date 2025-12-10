// [Revision: v2.1] [Path: src/hal.cpp] [Date: 2025-12-10]
// Description: Added SPIFFS and SD card initialization for dual storage support.

#include "hal.h"
#include <FS.h>
#include <SPIFFS.h>
#include <SD.h>

// --------------------------------------------------------------------------
// DISPLAY OBJECTS
// --------------------------------------------------------------------------

U8G2_ST7565_ERC12864_F_4W_HW_SPI u8g2(U8G2_R0, PIN_CS, PIN_DC, PIN_RST);
const uint8_t* FONT_SMALL = u8g2_font_5x7_t_cyrillic;

// Global Settings
int systemContrast = DEFAULT_CONTRAST;

// --------------------------------------------------------------------------
// INPUT MATRIX CONFIG
// --------------------------------------------------------------------------

byte rowPins[ROWS] = {42, 41, 40, 39};
byte colPins[COLS] = {1, 2, 6, 7, 15}; 

char keyMap[ROWS][COLS] = {
  {'*','0','#','D', 'M'}, 
  {'7','8','9','C', 'Z'}, 
  {'4','5','6','B', 'Y'}, 
  {'1','2','3','A', 'X'}  
};

char activeKeys[MAX_PRESSED_KEYS];
int activeKeyCount = 0;
char prevActiveKeys[MAX_PRESSED_KEYS];
int prevKeyCount = 0;

// --------------------------------------------------------------------------
// HARDWARE SETUP
// --------------------------------------------------------------------------

void setupHardware() {
  for(int i=0; i<ROWS; i++) pinMode(rowPins[i], INPUT_PULLUP);
  for(int i=0; i<COLS; i++) pinMode(colPins[i], INPUT);

  u8g2.begin();
  u8g2.setContrast(systemContrast); // Apply global default
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.enableUTF8Print();
  
  // Initialize SPIFFS (built-in 16MB flash storage)
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  } else {
    Serial.println("SPIFFS mounted successfully");
  }
  
  // Initialize SD card (SPI mode, will be added later)
  // For now, attempt initialization but don't fail if unavailable
  if (!SD.begin(-1, SPI, 4000000)) {
    Serial.println("SD card not available (will be added later)");
  } else {
    Serial.println("SD card mounted successfully");
  }
}

// --------------------------------------------------------------------------
// MATRIX SCANNING LOGIC
// --------------------------------------------------------------------------

void scanMatrix() {
  prevKeyCount = activeKeyCount;
  memcpy(prevActiveKeys, activeKeys, sizeof(activeKeys));
  activeKeyCount = 0;

  for (int c = 0; c < COLS; c++) {
    pinMode(colPins[c], OUTPUT);
    digitalWrite(colPins[c], LOW); 
    delayMicroseconds(50);

    for (int r = 0; r < ROWS; r++) {
      if (digitalRead(rowPins[r]) == LOW) {
        if (activeKeyCount < MAX_PRESSED_KEYS) {
           activeKeys[activeKeyCount++] = keyMap[r][c];
        }
      }
    }
    pinMode(colPins[c], INPUT);
  }
}

bool isJustPressed(char key) {
  bool currentlyPressed = false;
  for(int i=0; i<activeKeyCount; i++) {
    if(activeKeys[i] == key) { 
        currentlyPressed = true;
        break; 
    }
  }
  if (!currentlyPressed) return false;

  for(int i=0; i<prevKeyCount; i++) {
    if(prevActiveKeys[i] == key) return false; 
  }
  return true;
}