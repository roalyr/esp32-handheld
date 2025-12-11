// [Revision: v2.2] [Path: src/hal.cpp] [Date: 2025-12-11]
// Description: Added key repeat support for non-T9 navigation keys.

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

// Repeatable key flags - mirrors keyMap layout
// true = key will auto-repeat when held (non-T9 keys only)
bool keyRepeatMap[ROWS][COLS] = {
  {false, false, false, true,  true },  // *, 0, #, D, M
  {false, false, false, true,  true },  // 7, 8, 9, C, Z
  {false, false, false, true,  true },  // 4, 5, 6, B, Y
  {false, false, false, true,  true }   // 1, 2, 3, A, X
};

char activeKeys[MAX_PRESSED_KEYS];
int activeKeyCount = 0;
char prevActiveKeys[MAX_PRESSED_KEYS];
int prevKeyCount = 0;

// Key repeat state tracking
struct KeyRepeatState {
    char key;
    unsigned long pressStartTime;
    unsigned long lastRepeatTime;
    bool initialDelayPassed;
};

static KeyRepeatState repeatStates[MAX_PRESSED_KEYS];
static int repeatStateCount = 0;

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

// --------------------------------------------------------------------------
// KEY REPEAT LOGIC
// --------------------------------------------------------------------------

// Check if a key is marked as repeatable in the keyRepeatMap
static bool isKeyRepeatable(char key) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (keyMap[r][c] == key) {
                return keyRepeatMap[r][c];
            }
        }
    }
    return false;
}

// Find or create repeat state for a key
static KeyRepeatState* getRepeatState(char key) {
    // Find existing
    for (int i = 0; i < repeatStateCount; i++) {
        if (repeatStates[i].key == key) return &repeatStates[i];
    }
    // Create new if space available
    if (repeatStateCount < MAX_PRESSED_KEYS) {
        KeyRepeatState* state = &repeatStates[repeatStateCount++];
        state->key = key;
        state->pressStartTime = millis();
        state->lastRepeatTime = 0;
        state->initialDelayPassed = false;
        return state;
    }
    return nullptr;
}

// Remove repeat state for a key
static void removeRepeatState(char key) {
    for (int i = 0; i < repeatStateCount; i++) {
        if (repeatStates[i].key == key) {
            // Shift remaining states down
            for (int j = i; j < repeatStateCount - 1; j++) {
                repeatStates[j] = repeatStates[j + 1];
            }
            repeatStateCount--;
            return;
        }
    }
}

// Update repeat states based on currently pressed keys (call after scanMatrix)
static void updateRepeatStates() {
    // Remove states for keys no longer pressed
    for (int i = repeatStateCount - 1; i >= 0; i--) {
        bool stillPressed = false;
        for (int j = 0; j < activeKeyCount; j++) {
            if (activeKeys[j] == repeatStates[i].key) {
                stillPressed = true;
                break;
            }
        }
        if (!stillPressed) {
            removeRepeatState(repeatStates[i].key);
        }
    }
    
    // Add states for newly pressed repeatable keys
    for (int i = 0; i < activeKeyCount; i++) {
        char key = activeKeys[i];
        if (isKeyRepeatable(key)) {
            // Check if this is a new press
            bool wasPressed = false;
            for (int j = 0; j < prevKeyCount; j++) {
                if (prevActiveKeys[j] == key) {
                    wasPressed = true;
                    break;
                }
            }
            if (!wasPressed) {
                getRepeatState(key);  // Creates new state
            }
        }
    }
}

// Check if a repeatable key should fire a repeat event this frame
bool isRepeating(char key) {
    if (!isKeyRepeatable(key)) return false;
    
    // Update states first
    updateRepeatStates();
    
    // Find the repeat state
    KeyRepeatState* state = nullptr;
    for (int i = 0; i < repeatStateCount; i++) {
        if (repeatStates[i].key == key) {
            state = &repeatStates[i];
            break;
        }
    }
    
    if (!state) return false;
    
    unsigned long now = millis();
    unsigned long heldTime = now - state->pressStartTime;
    
    // Check if initial delay has passed
    if (!state->initialDelayPassed) {
        if (heldTime >= KEY_REPEAT_DELAY_MS) {
            state->initialDelayPassed = true;
            state->lastRepeatTime = now;
            return true;  // First repeat
        }
        return false;
    }
    
    // Check if it's time for another repeat
    if (now - state->lastRepeatTime >= KEY_REPEAT_RATE_MS) {
        state->lastRepeatTime = now;
        return true;
    }
    
    return false;
}