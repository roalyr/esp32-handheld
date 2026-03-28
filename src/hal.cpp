//
// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/hal.cpp
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TRUTH_HARDWARE.md Sections 0, 1, 2
// LOG_REF: 2026-03-28
//

#include "hal.h"

// --------------------------------------------------------------------------
// DISPLAY OBJECTS
// --------------------------------------------------------------------------

U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, PIN_SPI_SCLK, PIN_SPI_SID, PIN_CS);
const uint8_t* FONT_SMALL = u8g2_font_5x7_t_cyrillic;

// Global Settings
int systemContrast = DEFAULT_CONTRAST;
int systemBrightness = 255;  // Default full brightness (0-255)

// --------------------------------------------------------------------------
// INPUT MATRIX CONFIG
// --------------------------------------------------------------------------

byte rowPins[ROWS] = {3, 7, 5, 9};
byte colPins[COLS] = {1, 2, 4, 6, 8}; 

char keyMap[ROWS][COLS] = {
  {KEY_ESC,  '1',      '2',      '3',       KEY_BKSP },
  {KEY_TAB,  '4',      '5',      '6',       KEY_ENTER},
  {KEY_SHIFT,'7',      '8',      '9',       KEY_UP   },
  {KEY_ALT,  KEY_LEFT, '0',      KEY_RIGHT, KEY_DOWN }
};

// Repeatable key flags - mirrors keyMap layout
// true = key will auto-repeat when held (non-T9 keys only)
bool keyRepeatMap[ROWS][COLS] = {
  {false, false, false, false, true },  // ESC, 1, 2, 3, BKSP
  {false, false, false, false, false},  // TAB, 4, 5, 6, ENTER
  {false, false, false, false, true },  // SHIFT, 7, 8, 9, UP
  {false, true,  false, true,  true }   // ALT, LEFT, 0, RIGHT, DOWN
};

char activeKeys[MAX_PRESSED_KEYS];
int activeKeyCount = 0;
char prevActiveKeys[MAX_PRESSED_KEYS];
int prevKeyCount = 0;

// Inter-frame key latching: accumulates presses between frames
static char latchedKeys[MAX_PRESSED_KEYS];
static int latchedKeyCount = 0;

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

  // Configure backlight with PWM for brightness control
  ledcSetup(0, 5000, 8);         // Channel 0, 5kHz, 8-bit resolution
  ledcAttachPin(PIN_BACKLIGHT, 0);
  ledcWrite(0, systemBrightness);

  u8g2.begin();
  u8g2.setContrast(systemContrast); // No-op on ST7920 (contrast via hardware pot)
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.enableUTF8Print();
}

// --------------------------------------------------------------------------
// MATRIX SCANNING LOGIC
// --------------------------------------------------------------------------

// Add a key to the latched set if not already present
static void latchKey(char key) {
    for (int i = 0; i < latchedKeyCount; i++) {
        if (latchedKeys[i] == key) return;
    }
    if (latchedKeyCount < MAX_PRESSED_KEYS) {
        latchedKeys[latchedKeyCount++] = key;
    }
}

// Lightweight scan — call between frames to catch brief key presses.
// Detected keys are OR'd into the latch; they persist until the next
// frame-level scanMatrix() consumes them.
void pollMatrix() {
    for (int c = 0; c < COLS; c++) {
        pinMode(colPins[c], OUTPUT);
        digitalWrite(colPins[c], LOW);
        delayMicroseconds(50);

        for (int r = 0; r < ROWS; r++) {
            if (digitalRead(rowPins[r]) == LOW) {
                latchKey(keyMap[r][c]);
            }
        }
        pinMode(colPins[c], INPUT);
    }
}

// Frame-level scan — call once per frame. Copies latched keys to activeKeys,
// updates previous-frame state, then resets the latch.
void scanMatrix() {
  prevKeyCount = activeKeyCount;
  memcpy(prevActiveKeys, activeKeys, sizeof(activeKeys));

  // Do one final hardware scan to include keys pressed right now
  pollMatrix();

  // Copy latch → activeKeys
  activeKeyCount = latchedKeyCount;
  memcpy(activeKeys, latchedKeys, latchedKeyCount);

  // Reset latch for next inter-frame period
  latchedKeyCount = 0;
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