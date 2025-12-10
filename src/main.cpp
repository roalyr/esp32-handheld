// [Revision: v2.4] [Path: src/main.cpp] [Date: 2025-12-10]
// Description: Added Stopwatch App to system.

#include <Arduino.h>
#include "config.h"
#include "hal.h"

// App Modules
#include "apps/t9_editor.h"
#include "apps/key_tester.h"
#include "apps/snake.h"
#include "apps/gfx_test.h"
#include "apps/menu.h"
#include "apps/asteroids.h"
#include "apps/stopwatch.h" // New

// --------------------------------------------------------------------------
// SYSTEM STATE
// --------------------------------------------------------------------------

T9EditorApp appT9Editor;
KeyTesterApp appKeyTester;
SnakeApp appSnake;
GfxTestApp appGfxTest;
MenuApp appMenu;
AsteroidsApp appAsteroids;
StopwatchApp appStopwatch; // New

App* currentApp = nullptr;

// Timing Control
unsigned long lastFrameTime = 0;

void switchApp(App* newApp) {
  if (currentApp) currentApp->stop();
  currentApp = newApp;
  if (currentApp) currentApp->start();
}

void setup() {
  Serial.begin(115200);
  setupHardware();
  switchApp(&appMenu);
}

void loop() {
  unsigned long now = millis();
  
  // Enforce 60 FPS
  if (now - lastFrameTime >= FRAME_DELAY_MS) {
      lastFrameTime = now;

      // 1. HARDWARE SCAN
      scanMatrix();
    
      // 2. EVENT HANDLING
      for(int i=0; i<activeKeyCount; i++) {
        char key = activeKeys[i];
        
        if (isJustPressed(key)) {
            if (key == 'D') {
                if (currentApp != &appMenu) switchApp(&appMenu);
                continue;
            }
            if (currentApp) currentApp->handleInput(key);
        }
      }
    
      // 3. LOGIC UPDATE
      if (currentApp) {
          currentApp->update();
          
          // Menu Switching Logic
          if (currentApp == &appMenu) {
              int req = appMenu.getPendingSwitch();
              if (req != -1) {
                  switch(req) {
                      case 0: switchApp(&appT9Editor); break;
                      case 1: switchApp(&appKeyTester); break;
                      case 2: switchApp(&appSnake); break;
                      case 3: switchApp(&appGfxTest); break;
                      case 4: switchApp(&appAsteroids); break; 
                      case 5: switchApp(&appStopwatch); break; // New
                  }
              }
          }
      }
    
      // 4. RENDER
      u8g2.clearBuffer();
      if (currentApp) currentApp->render();
      u8g2.sendBuffer();
  }
}