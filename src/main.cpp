// [Revision: v2.6] [Path: src/main.cpp] [Date: 2025-12-10]
// Description: Added handling for app exit requests (fixes T9 Editor exit bug).

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
#include "apps/stopwatch.h"

// --------------------------------------------------------------------------
// SYSTEM STATE
// --------------------------------------------------------------------------

T9EditorApp appT9Editor;
KeyTesterApp appKeyTester;
SnakeApp appSnake;
GfxTestApp appGfxTest;
MenuApp appMenu;
AsteroidsApp appAsteroids;
StopwatchApp appStopwatch;

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
            // GLOBAL HOME KEY EXCEPTION
            if (key == 'D') {
                 // If we are in T9 Editor, let the app handle it (for popup)
                 if (currentApp == &appT9Editor) {
                     currentApp->handleInput(key);
                     continue;
                 }
                 
                 // Otherwise, go to Menu
                 if (currentApp != &appMenu) switchApp(&appMenu);
                 continue;
            }

            // REGULAR APP INPUT
            if (currentApp) {
                currentApp->handleInput(key);
            }
        }
      } 
    
      // 3. LOGIC UPDATE
      if (currentApp) {
          currentApp->update();

          // Check for T9 Editor Exit Request
          if (currentApp == &appT9Editor && appT9Editor.exitRequested) {
              switchApp(&appMenu);
          }
          
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
                      case 5: switchApp(&appStopwatch); break;
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