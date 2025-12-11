// [Revision: v3.4] [Path: src/main.cpp] [Date: 2025-12-11]
// Description: Main loop with boot splash, sleep mode, settings app, key repeat.

#include <Arduino.h>
#include "config.h"
#include "hal.h"
#include "clock.h"
#include "gui.h"
#include <esp_sleep.h>

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
#include "apps/settings.h"
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
SettingsApp appSettings;

App* currentApp = nullptr;

// Timing Control
unsigned long lastFrameTime = 0;
unsigned long lastActivityTime = 0;
bool sleepEnabled = SLEEP_ENABLED;
bool isAsleep = false;

// --------------------------------------------------------------------------
// BOOT SPLASH
// --------------------------------------------------------------------------

void showBootSplash() {
    u8g2.clearBuffer();
    
    // Title
    u8g2.setFont(u8g2_font_ncenB10_tr);
    const char* title = FIRMWARE_NAME;
    int titleWidth = u8g2.getStrWidth(title);
    u8g2.drawStr((GUI::SCREEN_WIDTH - titleWidth) / 2, 25, title);
    
    // Version
    u8g2.setFont(u8g2_font_5x7_tf);
    char verStr[32];
    snprintf(verStr, sizeof(verStr), "v%s", FIRMWARE_VERSION);
    int verWidth = u8g2.getStrWidth(verStr);
    u8g2.drawStr((GUI::SCREEN_WIDTH - verWidth) / 2, 40, verStr);
    
    // Loading text
    u8g2.drawStr((GUI::SCREEN_WIDTH - u8g2.getStrWidth("Initializing...")) / 2, 55, "Initializing...");
    
    u8g2.sendBuffer();
    delay(1000);  // Show splash for 1 second
}

// --------------------------------------------------------------------------
// SLEEP MODE
// --------------------------------------------------------------------------

void enterSleep() {
    isAsleep = true;
    
    // Turn off display
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    u8g2.setPowerSave(1);  // Display power save mode
    
    // Configure wake-up on any GPIO (simplified - wake on any key)
    // For ESP32-S3, we use light sleep with GPIO wake-up
    esp_sleep_enable_timer_wakeup(100000);  // Wake every 100ms to check keys
    
    Serial.println("Entering sleep mode...");
}

void wakeUp() {
    isAsleep = false;
    u8g2.setPowerSave(0);  // Restore display
    u8g2.setContrast(systemContrast);
    lastActivityTime = millis();
    Serial.println("Waking up...");
}

void checkSleepMode() {
    if (!sleepEnabled) return;
    
    // Check for any key activity
    if (activeKeyCount > 0) {
        if (isAsleep) {
            wakeUp();
        }
        lastActivityTime = millis();
        return;
    }
    
    // Check timeout
    if (!isAsleep && (millis() - lastActivityTime >= SLEEP_TIMEOUT_MS)) {
        enterSleep();
    }
}

// --------------------------------------------------------------------------
// APP MANAGEMENT
// --------------------------------------------------------------------------

void switchApp(App* newApp) {
  if (currentApp) currentApp->stop();
  currentApp = newApp;
  if (currentApp) currentApp->start();
}

void setup() {
  Serial.begin(115200);
  setupHardware();
  
  showBootSplash();
  
  SystemClock::init();
  lastActivityTime = millis();
  
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
      
      // 2. SLEEP MODE CHECK
      checkSleepMode();
      if (isAsleep) {
          esp_light_sleep_start();  // Enter light sleep briefly
          return;  // Skip rest of loop while asleep
      }
    
      // 3. EVENT HANDLING
      for(int i=0; i<activeKeyCount; i++) {
        char key = activeKeys[i];
        
        // Fire on initial press OR on repeat interval for repeatable keys
        bool shouldFire = isJustPressed(key) || isRepeating(key);
        
        if (shouldFire) {
            // Reset activity timer on any input
            lastActivityTime = now;
            
            // GLOBAL HOME KEY EXCEPTION (D key - no repeat for menu navigation)
            if (key == 'D' && isJustPressed(key)) {
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

            // REGULAR APP INPUT (including repeats)
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
                      case APP_KEY_TESTER:   switchApp(&appKeyTester); break;
                      case APP_GFX_TEST:     switchApp(&appGfxTest); break;
                      case APP_STOPWATCH:    switchApp(&appStopwatch); break;
                      case APP_CLOCK:        switchApp(&appClock); break;
                      case APP_FILE_BROWSER: switchApp(&appFileBrowser); break;
                      case APP_LUA_RUNNER:   switchApp(&appLuaRunner); break;
                      case APP_SETTINGS:     switchApp(&appSettings); break;
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