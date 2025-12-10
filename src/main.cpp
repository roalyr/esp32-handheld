// [Revision: v1.0] [Path: src/main.cpp] [Date: 2025-12-09]
// Description: Main entry point. Manages the main run loop, system mode switching, and event dispatching.

#include <Arduino.h>
#include "config.h"
#include "hal.h"
#include "t9_engine.h"

// App Modules
#include "apps/t9_editor.h"
#include "apps/key_tester.h"
#include "apps/snake.h"
#include "apps/gfx_test.h"

// --------------------------------------------------------------------------
// SYSTEM STATE
// --------------------------------------------------------------------------

SystemMode currentMode = MODE_T9_EDITOR;

// Store the last pressed key for the Key Tester app
char lastTestKey = ' ';

// --------------------------------------------------------------------------
// MAIN SETUP
// --------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  
  // 1. Initialize Hardware (Display & Matrix)
  setupHardware();
  
  // 2. Initialize Apps
  setupTester();
  setupSnake(); 
  
  // 3. Initial State
  u8g2.setContrast(0);
}

// --------------------------------------------------------------------------
// MAIN LOOP
// --------------------------------------------------------------------------

void loop() {
  // 1. HARDWARE SCAN
  // Must be called once per frame to update key states
  scanMatrix();

  // 2. EVENT HANDLING
  for(int i=0; i<activeKeyCount; i++) {
    char key = activeKeys[i];
    
    // Only trigger actions on the *first* frame of a press
    if (isJustPressed(key)) {
        
        // --- GLOBAL APP SWITCHING ---
        // X = T9 Editor
        // Y = Key Tester
        // A = Snake
        // B = GFX Test
        
        if (key == 'X') {
          currentMode = MODE_T9_EDITOR;
          u8g2.setContrast(0); // Reset contrast in case we came from GFX Test
          continue;
        }
        if (key == 'Y') {
          currentMode = MODE_KEY_TESTER;
          u8g2.setContrast(0);
          continue; 
        }
        if (key == 'A') {
          currentMode = MODE_SNAKE;
          setupSnake(); // Reset game on entry
          u8g2.setContrast(0);
          continue;
        }
        if (key == 'B') {
          currentMode = MODE_GFX_TEST;
          // Note: GFX Test handles its own contrast
          continue;
        }
        
        // --- APP INPUT DELEGATION ---
        switch (currentMode) {
            case MODE_T9_EDITOR:
                engine.handleInput(key);
                break;
            case MODE_KEY_TESTER:
                lastTestKey = key;
                addToTesterHistory(key);
                break;
            case MODE_SNAKE:
                handleSnakeInput(key);
                break;
            case MODE_GFX_TEST:
                // GFX Test is passive / auto-running
                break;
        }
    }
  }

  // 3. LOGIC UPDATE
  // Handle continuous updates (timers, physics, etc.)
  if (currentMode == MODE_T9_EDITOR) engine.update();
  if (currentMode == MODE_SNAKE) updateSnake();

  // 4. RENDER
  u8g2.clearBuffer();
  
  switch (currentMode) {
      case MODE_T9_EDITOR:
          renderT9Editor();
          break;
      case MODE_KEY_TESTER:
          renderKeyTester(lastTestKey, activeKeys, activeKeyCount);
          break;
      case MODE_SNAKE:
          renderSnake();
          break;
      case MODE_GFX_TEST:
          renderGfxTest();
          break;
  }
  
  u8g2.sendBuffer();
}