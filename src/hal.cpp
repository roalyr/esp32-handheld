//
// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/hal.cpp
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TRUTH_HARDWARE.md Section 2
// LOG_REF: 2026-04-22
//

#include "hal.h"

// --------------------------------------------------------------------------
// DISPLAY OBJECTS
// --------------------------------------------------------------------------

// SW_SPI — bit-bangs dedicated LCD GPIO pins, no SPI peripheral involvement.
// HW_SPI causes display artifacts on ST7920 (non-standard serial protocol).
// LCD uses dedicated GPIO 33 (SID) and GPIO 34 (E), isolated from SD SPI bus.
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, PIN_LCD_E, PIN_LCD_SID, PIN_CS);
const uint8_t* FONT_SMALL = u8g2_font_5x7_t_cyrillic;

// Global Settings
int systemContrast = DEFAULT_CONTRAST;
int systemBrightness = 38;  // Default ~15% brightness (0-255)

// --------------------------------------------------------------------------
// SD CARD STATE
// --------------------------------------------------------------------------

// SD card uses HW SPI (FSPI) on dedicated pins GPIO 35/36/37.
// LCD is on separate GPIO 33/34 — no bus contention.
// Uses SdFat library — Arduino SD library's ESP-IDF SDSPI driver fails on S2.
SdFat sdFat;
static SPIClass sdSpi(FSPI);
static bool sdCardDetected = false;   // Card was found (cached between sessions)
static bool sdSessionActive = false;  // HW SPI bus is currently held for SD
static uint64_t sdCachedTotal = 0;    // Cached total bytes (refreshed on mount)
static uint64_t sdCachedUsed = 0;     // Cached used bytes (refreshed on mount)

// --------------------------------------------------------------------------
// INPUT MATRIX CONFIG
// --------------------------------------------------------------------------

byte rowPins[ROWS] = {3, 7, 5, 9};
byte colPins[COLS] = {1, 2, 4, 6, 8}; 

char keyMap[ROWS][COLS] = {
  {KEY_ALT,  KEY_LEFT, '0',      KEY_RIGHT, KEY_DOWN },
  {KEY_TAB,  '4',      '5',      '6',       KEY_ENTER},
  {KEY_SHIFT,'7',      '8',      '9',       KEY_UP   },
  {KEY_ESC,  '1',      '2',      '3',       KEY_BKSP }
};

// Repeatable key flags - mirrors keyMap layout
// true = key will auto-repeat when held (non-T9 keys only)
bool keyRepeatMap[ROWS][COLS] = {
    {false, true,  false, true,  true },  // ALT, LEFT, 0, RIGHT, DOWN
  {false, false, false, false, true },  // TAB, 4, 5, 6, ENTER
  {false, false, false, false, true },  // SHIFT, 7, 8, 9, UP
  {false, false, false, false, true }   // ESC, 1, 2, 3, BKSP
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
    bool longPressFired;          // One-shot flag for isLongPressed()
};

static KeyRepeatState repeatStates[MAX_PRESSED_KEYS];
static int repeatStateCount = 0;

// Forward declaration — called from scanMatrix(), defined in key repeat section
static void updateRepeatStates();

// --------------------------------------------------------------------------
// HARDWARE SETUP
// --------------------------------------------------------------------------

void setupHardware() {
  for(int i=0; i<ROWS; i++) pinMode(rowPins[i], INPUT_PULLUP);
  for(int i=0; i<COLS; i++) pinMode(colPins[i], INPUT);

  // Deselect SD card CS to prevent bus noise during LCD init
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);

  // Configure backlight with PWM for brightness control
  ledcSetup(0, 5000, 8);         // Channel 0, 5kHz, 8-bit resolution
  ledcAttachPin(PIN_BACKLIGHT, 0);
  ledcWrite(0, systemBrightness);

  // Init LCD first — uses dedicated pins (GPIO 33/34), no bus conflict with SD
  u8g2.begin();
  u8g2.setContrast(systemContrast); // Sets Vop via ST7920 extended instruction set (RE=1 mode). Works independently of hardware V0 pot.
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.enableUTF8Print();

  // Probe SD card on HW SPI (FSPI) — dedicated pins, no LCD interference
  Serial.println("[HAL] Probing SD card...");
  if (mountSD()) {
      Serial.printf("[HAL] SD card: detected (%llu/%llu bytes used)\n",
                    (unsigned long long)sdUsedBytes(),
                    (unsigned long long)sdTotalBytes());
  } else {
      Serial.println("[HAL] SD card: not found");
  }
}

// --------------------------------------------------------------------------
// SD CARD SESSION MANAGEMENT
// --------------------------------------------------------------------------

// Acquire HW SPI bus and mount SD card for file operations.
// LCD is on separate pins — no conflict.
bool sdBeginSession() {
    if (sdSessionActive) return true;

    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);

    sdSpi.begin(PIN_SPI_SCLK, PIN_SPI_MISO, PIN_SPI_MOSI);
    SdSpiConfig spiCfg(PIN_SD_CS, DEDICATED_SPI, SD_SCK_MHZ(4), &sdSpi);
    if (sdFat.begin(spiCfg)) {
        sdSessionActive = true;
        Serial.println("[HAL] SdFat OK (HW SPI, 4MHz)");
        return true;
    }
    Serial.printf("[HAL] SdFat fail type=%d code=0x%02X\n",
                   (int)sdFat.sdErrorCode(),
                   (int)sdFat.sdErrorData());
    sdSpi.end();
    sdCardDetected = false;
    sdCachedTotal = 0;
    sdCachedUsed = 0;
    return false;
}

// Release HW SPI bus.
void sdEndSession() {
    if (!sdSessionActive) return;
    sdFat.end();
    sdSpi.end();
    sdSessionActive = false;
}

// --------------------------------------------------------------------------
// SD CARD PUBLIC API (uses cached values — no SPI bus needed)
// --------------------------------------------------------------------------

bool mountSD() {
    // Full mount cycle: acquire bus, read info, release bus
    if (sdBeginSession()) {
        uint32_t clusterCount = sdFat.clusterCount();
        uint32_t sectorsPerCluster = sdFat.sectorsPerCluster();
        sdCachedTotal = (uint64_t)clusterCount * sectorsPerCluster * 512ULL;
        uint32_t freeClusterCount = sdFat.freeClusterCount();
        uint64_t freeBytes = (uint64_t)freeClusterCount * sectorsPerCluster * 512ULL;
        sdCachedUsed = sdCachedTotal - freeBytes;
        sdEndSession();
        sdCardDetected = true;
        return true;
    }
    sdCardDetected = false;
    sdCachedTotal = 0;
    sdCachedUsed = 0;
    return false;
}

void unmountSD() {
    sdEndSession();
    sdCardDetected = false;
    sdCachedTotal = 0;
    sdCachedUsed = 0;
}

bool isSDMounted() {
    return sdCardDetected;
}

uint64_t sdTotalBytes() {
    return sdCachedTotal;
}

uint64_t sdUsedBytes() {
    return sdCachedUsed;
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

  // Update repeat states once per frame (must run here, not inside
  // isRepeating(), because short-circuit evaluation of
  // isJustPressed(key) || isRepeating(key) would skip the call on
  // the first frame of a keypress, preventing state creation).
  updateRepeatStates();
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
        state->longPressFired = false;
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
    
    // Add states for ALL newly pressed keys (not just repeatable ones)
    // so isKeyHeld() works universally (e.g. digit long-press detection).
    for (int i = 0; i < activeKeyCount; i++) {
        char key = activeKeys[i];
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
    
    // Update initialDelayPassed for ALL tracked keys (not just repeatable ones).
    // isRepeating() only processes repeatable keys, so without this, non-repeatable
    // keys (digits) would never get initialDelayPassed set — breaking isLongPressed().
    unsigned long now = millis();
    for (int i = 0; i < repeatStateCount; i++) {
        if (!repeatStates[i].initialDelayPassed) {
            if (now - repeatStates[i].pressStartTime >= KEY_REPEAT_DELAY_MS) {
                repeatStates[i].initialDelayPassed = true;
            }
        }
    }
}

// Check if a repeatable key should fire a repeat event this frame
bool isRepeating(char key) {
    if (!isKeyRepeatable(key)) return false;
    
    // Find the repeat state (updateRepeatStates already called by scanMatrix)
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

// Read-only query: has a key been held past the initial repeat delay?
// Unlike isRepeating(), this does NOT consume/update any timing state.
bool isKeyHeld(char key) {
    for (int i = 0; i < repeatStateCount; i++) {
        if (repeatStates[i].key == key) {
            return repeatStates[i].initialDelayPassed;
        }
    }
    return false;
}

// One-shot long-press detection: returns true exactly ONCE when a key
// has been held past KEY_REPEAT_DELAY_MS. Works for all keys (including
// non-repeatable ones like digit keys). After firing, will not fire again
// until the key is released and re-pressed.
bool isLongPressed(char key) {
    for (int i = 0; i < repeatStateCount; i++) {
        if (repeatStates[i].key == key) {
            if (repeatStates[i].initialDelayPassed && !repeatStates[i].longPressFired) {
                repeatStates[i].longPressFired = true;
                return true;
            }
            return false;
        }
    }
    return false;
}