// [Revision: v1.0] [Path: src/hal.cpp] [Date: 2025-12-09]
// Description: Implementation of hardware initialization, display setup, and matrix scanning logic.

#include "hal.h"

// --------------------------------------------------------------------------
// DISPLAY OBJECTS
// --------------------------------------------------------------------------

// Initialize ST7565 Display with Hardware SPI
// Pins: Clock=SCK, Data=MOSI (Hardware default), CS, DC, RST
U8G2_ST7565_ERC12864_F_4W_HW_SPI u8g2(U8G2_R0, PIN_CS, PIN_DC, PIN_RST);

// Define the shared font here to ensure single storage in Flash
const uint8_t* FONT_SMALL = u8g2_font_5x7_t_cyrillic;

// --------------------------------------------------------------------------
// INPUT MATRIX CONFIG
// --------------------------------------------------------------------------

// Pin Definitions
byte rowPins[ROWS] = {42, 41, 40, 39};
byte colPins[COLS] = {1, 2, 6, 7, 15}; 

// Key Character Map
char keyMap[ROWS][COLS] = {
  {'*','0','#','D', 'M'}, 
  {'7','8','9','C', 'Z'}, 
  {'4','5','6','B', 'Y'}, 
  {'1','2','3','A', 'X'}  
};

// Scanning State
char activeKeys[MAX_PRESSED_KEYS];
int activeKeyCount = 0;

// Previous frame state for edge detection
char prevActiveKeys[MAX_PRESSED_KEYS];
int prevKeyCount = 0;

// --------------------------------------------------------------------------
// HARDWARE SETUP
// --------------------------------------------------------------------------

void setupHardware() {
  // Initialize Row Pins (Inputs with Pullup)
  for(int i=0; i<ROWS; i++) {
      pinMode(rowPins[i], INPUT_PULLUP);
  }

  // Initialize Col Pins (Inputs - High Z state initially)
  for(int i=0; i<COLS; i++) {
      pinMode(colPins[i], INPUT);
  }

  // Initialize Display
  u8g2.begin();
  u8g2.setContrast(0); // Start with neutral contrast
  u8g2.setFontMode(1); // Transparent font background
  u8g2.setBitmapMode(1);
  u8g2.enableUTF8Print(); // Enable Cyrillic support
}

// --------------------------------------------------------------------------
// MATRIX SCANNING LOGIC
// --------------------------------------------------------------------------

void scanMatrix() {
  // 1. Archive current state to previous state
  prevKeyCount = activeKeyCount;
  memcpy(prevActiveKeys, activeKeys, sizeof(activeKeys));
  
  // 2. Reset current counter
  activeKeyCount = 0;

  // 3. Scan Columns
  for (int c = 0; c < COLS; c++) {
    // Drive current column LOW
    pinMode(colPins[c], OUTPUT);
    digitalWrite(colPins[c], LOW); 
    
    // Short delay for signal settling
    delayMicroseconds(50);

    // Check Rows
    for (int r = 0; r < ROWS; r++) {
      // If Row is LOW, the switch is closed
      if (digitalRead(rowPins[r]) == LOW) {
        if (activeKeyCount < MAX_PRESSED_KEYS) {
           activeKeys[activeKeyCount++] = keyMap[r][c];
        }
      }
    }
    
    // Return column to High-Z (Input)
    pinMode(colPins[c], INPUT);
  }
}

// --------------------------------------------------------------------------
// INPUT UTILITIES
// --------------------------------------------------------------------------

// Returns true only on the specific frame a key is first pressed
bool isJustPressed(char key) {
  // 1. Verify key is currently held
  bool currentlyPressed = false;
  for(int i=0; i<activeKeyCount; i++) {
    if(activeKeys[i] == key) { 
        currentlyPressed = true;
        break; 
    }
  }
  
  if (!currentlyPressed) return false;

  // 2. Verify key was NOT held in previous frame
  for(int i=0; i<prevKeyCount; i++) {
    if(prevActiveKeys[i] == key) return false; 
  }
  
  return true;
}