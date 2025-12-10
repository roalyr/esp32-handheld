// [Revision: v2.1] [Path: src/main.cpp] [Date: 2025-12-10]
// Description: Updated Main to include MenuApp and handle app switching logic.

#include <Arduino.h>
#include "config.h"
#include "hal.h"

// App Modules
#include "apps/t9_editor.h"
#include "apps/key_tester.h"
#include "apps/snake.h"
#include "apps/gfx_test.h"
#include "apps/menu.h"  // <--- NEW

// --------------------------------------------------------------------------
// SYSTEM STATE
// --------------------------------------------------------------------------

// App Instances
T9EditorApp appT9Editor;
KeyTesterApp appKeyTester;
SnakeApp appSnake;
GfxTestApp appGfxTest;
MenuApp appMenu;        // <--- NEW

// Current Active App
App* currentApp = nullptr;

// --------------------------------------------------------------------------
// HELPER FUNCTIONS
// --------------------------------------------------------------------------

void switchApp(App* newApp) {
  if (currentApp) {
    currentApp->stop();
  }
  currentApp = newApp;
  if (currentApp) {
    currentApp->start();
  }
}

// --------------------------------------------------------------------------
// MAIN SETUP
// --------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  setupHardware();
  
  // Start with Menu
  switchApp(&appMenu);
}

// --------------------------------------------------------------------------
// MAIN LOOP
// --------------------------------------------------------------------------

void loop() {
  // 1. HARDWARE SCAN
  scanMatrix();

  // 2. EVENT HANDLING
  for(int i=0; i<activeKeyCount; i++) {
    char key = activeKeys[i];
    
    if (isJustPressed(key)) {
        
        // --- EMERGENCY GLOBAL HOME KEY ---
        // 'D' acts as a "Back to Menu" button global override
        if (key == 'D') {
            if (currentApp != &appMenu) switchApp(&appMenu);
            continue;
        }
        
        // --- APP INPUT DELEGATION ---
        if (currentApp) {
            currentApp->handleInput(key);
        }
    }
  }

  // 3. LOGIC UPDATE
  if (currentApp) {
      currentApp->update();
      
      // Check if the current app is the Menu, and if it requested a switch
      if (currentApp == &appMenu) {
          int req = appMenu.getPendingSwitch();
          if (req != -1) {
              switch(req) {
                  case 0: switchApp(&appT9Editor); break;
                  case 1: switchApp(&appKeyTester); break;
                  case 2: switchApp(&appSnake); break;
                  case 3: switchApp(&appGfxTest); break;
              }
          }
      }
  }

  // 4. RENDER
  u8g2.clearBuffer();
  if (currentApp) {
      currentApp->render();
  }
  u8g2.sendBuffer();
}