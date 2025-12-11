// [Revision: v3.1] [Path: src/main.cpp] [Date: 2025-12-11]
// Description: Main loop with clock system and per-app high-FPS support.

#include <Arduino.h>
#include "config.h"
#include "hal.h"
#include "clock.h"

// App Modules
#include "apps/t9_editor.h"
#include "apps/key_tester.h"
#include "apps/gfx_test.h"
#include "apps/menu.h"
#include "apps/stopwatch.h"
#include "apps/file_browser.h"
#include "apps/yes_no_prompt.h"
#include "apps/lua_runner.h"
#include "apps/clock.h"
#include "app_transfer.h"

// --------------------------------------------------------------------------
// SYSTEM STATE
// --------------------------------------------------------------------------

T9EditorApp appT9Editor;
KeyTesterApp appKeyTester;
GfxTestApp appGfxTest;
MenuApp appMenu;
StopwatchApp appStopwatch;
FileBrowserApp appFileBrowser;
LuaRunnerApp appLuaRunner;
ClockApp appClock;

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
  SystemClock::init();
  switchApp(&appMenu);
}

void loop() {
  unsigned long now = millis();
  
  // Check if current app needs high-FPS mode (e.g., GfxTest ghosting mode)
  bool highFpsMode = (currentApp == &appGfxTest && appGfxTest.needsHighFps());
  
  // Use minimal delay for high-FPS mode, normal frame delay otherwise
  unsigned long frameDelay = highFpsMode ? 1 : FRAME_DELAY_MS;
  
  if (now - lastFrameTime >= frameDelay) {
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
                 
                 // If we are in Menu submenu, let the menu handle it (go back)
                 if (currentApp == &appMenu && appMenu.isInSubmenu()) {
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
              // If a caller requested the editor for a special action, return there
              if (appTransferCaller != nullptr) {
                  App* ret = appTransferCaller;
                  appTransferCaller = nullptr;
                  appT9Editor.exitRequested = false;
                  switchApp(ret);
              } else {
                  appT9Editor.exitRequested = false;
                  switchApp(&appMenu);
              }
          }
          
          // Menu Switching Logic
          if (currentApp == &appMenu) {
              int req = appMenu.getPendingSwitch();
              if (req != -1) {
                  switch(req) {
                      case 0: switchApp(&appKeyTester); break;
                      case 1: switchApp(&appGfxTest); break;
                      case 2: switchApp(&appStopwatch); break;
                      case 3: switchApp(&appClock); break;
                      case 4: switchApp(&appFileBrowser); break;
                      case 5: switchApp(&appLuaRunner); break;
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