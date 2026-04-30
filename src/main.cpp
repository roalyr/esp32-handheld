//
// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/main.cpp
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TACTICAL_TODO TASK_1
// LOG_REF: 2026-04-22
//

#include <Arduino.h>
#include "config.h"
#include "hal.h"
#include "clock.h"
#include "gui.h"
#include "lua_vm.h"
#include "lua_scripts.h"
#include "app_transfer.h"
#include <esp_sleep.h>

// App Modules (only Settings remains as CPP app)
#include "apps/settings.h"
#include "apps/t9_editor.h"

// --------------------------------------------------------------------------
// SYSTEM STATE
// --------------------------------------------------------------------------

SettingsApp appSettings;
T9EditorApp appT9Editor;

// System mode: two states only
enum SystemMode { MODE_LUA, MODE_SETTINGS };
static SystemMode currentMode = MODE_LUA;
static bool luaError = false;
static String luaErrorMsg = "";
static App* activeSettingsApp = nullptr;
static bool returnToLuaOnAppExit = false;

static void clearAppTransferState() {
    appTransferAction = ACTION_NONE;
    appTransferBool = false;
    appTransferString = "";
    appTransferPath = "";
    appTransferEditorMode = APP_TRANSFER_EDITOR_DEFAULT;
    appTransferSourceKind = APP_TRANSFER_SOURCE_DEFAULT;
    appTransferLabel = "";
}

void switchApp(App* newApp) {
    if (newApp == nullptr || newApp == activeSettingsApp) {
        return;
    }

    App* previousApp = activeSettingsApp;
    if (previousApp != nullptr && previousApp != &appSettings) {
        previousApp->stop();
    }

    activeSettingsApp = newApp;
    if (newApp != &appSettings || previousApp == nullptr) {
        newApp->start();
    }
}

void launchLuaOwnedApp(App* newApp) {
    if (newApp == nullptr) {
        return;
    }

    if (currentMode == MODE_LUA) {
        currentMode = MODE_SETTINGS;
        activeSettingsApp = nullptr;
        returnToLuaOnAppExit = true;
    }

    switchApp(newApp);
}

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
    
    // Draw screensaver frame (stays visible on LCD with backlight off)
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_t_cyrillic);
    const char* line1 = FIRMWARE_NAME;
    char line2[32];
    snprintf(line2, sizeof(line2), "v%s", FIRMWARE_VERSION);
    const char* line3 = "Press any key";
    int w1 = u8g2.getStrWidth(line1);
    int w2 = u8g2.getStrWidth(line2);
    int w3 = u8g2.getStrWidth(line3);
    u8g2.drawStr((128 - w1) / 2, 24, line1);
    u8g2.drawStr((128 - w2) / 2, 34, line2);
    u8g2.drawStr((128 - w3) / 2, 48, line3);
    u8g2.drawFrame(0, 0, 128, 64);
    u8g2.sendBuffer();
    
    // Turn off backlight — LCD keeps showing screensaver
    ledcWrite(0, 0);
    
    // Configure wake-up on any GPIO (simplified - wake on any key)
    // For ESP32-S2, we use light sleep with GPIO wake-up
    esp_sleep_enable_timer_wakeup(100000);  // Wake every 100ms to check keys
    
    Serial.println("Entering sleep mode...");
}

void wakeUp() {
    isAsleep = false;
    ledcWrite(0, systemBrightness);  // Restore backlight (app will redraw on next loop)
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
// LUA ERROR SCREEN
// --------------------------------------------------------------------------

void renderLuaError() {
    u8g2.clearBuffer();
    
    // Header
    GUI::drawHeader("LUA ERROR");
    
    // Error message (word-wrapped)
    u8g2.setFont(u8g2_font_5x7_tf);
    const char* msg = luaErrorMsg.c_str();
    int y = GUI::CONTENT_START_Y;
    int maxWidth = GUI::SCREEN_WIDTH - 4;
    int lineH = 8;
    
    // Simple line-by-line rendering (truncate long lines)
    int len = strlen(msg);
    int pos = 0;
    while (pos < len && y < 54) {
        // Find how many chars fit on this line
        char lineBuf[32];
        int lineLen = 0;
        while (pos + lineLen < len && lineLen < 31) {
            if (msg[pos + lineLen] == '\n') { lineLen++; break; }
            lineLen++;
        }
        memcpy(lineBuf, msg + pos, lineLen);
        lineBuf[lineLen] = '\0';
        // Strip trailing newline for display
        if (lineLen > 0 && lineBuf[lineLen - 1] == '\n') lineBuf[lineLen - 1] = '\0';
        u8g2.drawStr(2, y, lineBuf);
        pos += lineLen;
        y += lineH;
    }
    
    // Footer
    GUI::drawFooterHints("ESC:Settings", "Enter:Retry");
    
    u8g2.sendBuffer();
}

// --------------------------------------------------------------------------
// SETUP & LOOP
// --------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(2000);

    setupHardware();
    
    showBootSplash();
    
    SystemClock::init();
    lastActivityTime = millis();
    
    // Initialize Lua VM
    if (!LuaVM::init()) {
        luaError = true;
        luaErrorMsg = LuaVM::getLastError();
        Serial.println("[main] LuaVM init failed");
    } else {
        // Run embedded desktop script
        Serial.println("[main] Running embedded Lua...");
        if (!LuaVM::executeString(LUA_DESKTOP, "desktop")) {
            luaError = true;
            luaErrorMsg = LuaVM::getLastError();
            Serial.print("[main] Lua failed: ");
            Serial.println(luaErrorMsg);
        } else {
            LuaVM::callGlobalFunction("_init");
            Serial.println("[main] Embedded Lua loaded OK");
        }
    }
    
    // SD was already probed in setupHardware() (before LCD init).
    // Log the cached result here for visibility in the boot sequence.
    if (isSDMounted()) {
        Serial.printf("[main] SD card: detected (%llu/%llu bytes used)\n",
                      (unsigned long long)sdUsedBytes(),
                      (unsigned long long)sdTotalBytes());
    } else {
        Serial.println("[main] SD card: not found");
    }
    
    currentMode = MODE_LUA;
}

void loop() {
    // Always poll keys — catches brief presses even during long frames
    pollMatrix();
    
    unsigned long now = millis();
    
    if (now - lastFrameTime >= FRAME_DELAY_MS) {
        lastFrameTime = now;
        
        // 1. HARDWARE SCAN (finalizes latched keys for this frame)
        scanMatrix();
        
        // 2. SLEEP MODE CHECK
        checkSleepMode();
        if (isAsleep) {
            delay(100);  // Idle poll — avoid esp_light_sleep which glitches HW SPI
            return;
        }
        
        // 3. ESC HANDLING (just-pressed only, before app/lua input)
        if (isJustPressed(KEY_ESC)) {
            lastActivityTime = now;
            
            if (currentMode == MODE_LUA) {
                currentMode = MODE_SETTINGS;
                activeSettingsApp = nullptr;
                switchApp(&appSettings);
            } else if (currentMode == MODE_SETTINGS) {
                if (activeSettingsApp != nullptr && activeSettingsApp != &appSettings) {
                    activeSettingsApp->handleInput(KEY_ESC);
                } else if (appSettings.isInSubmenu()) {
                    // Let settings handle ESC internally (e.g. exit key tester)
                    appSettings.handleInput(KEY_ESC);
                } else {
                    appSettings.stop();
                    currentMode = MODE_LUA;
                    activeSettingsApp = nullptr;
                }
            }
            // ESC consumed — skip to render
        } else {
            // 4. MODE-SPECIFIC INPUT + UPDATE
            if (currentMode == MODE_LUA) {
                if (luaError) {
                    // Check for retry
                    if (isJustPressed(KEY_ENTER)) {
                        lastActivityTime = now;
                        luaError = false;
                        luaErrorMsg = "";
                        LuaVM::clearError();
                        if (!LuaVM::executeString(LUA_DESKTOP, "desktop")) {
                            luaError = true;
                            luaErrorMsg = LuaVM::getLastError();
                        } else {
                            LuaVM::callGlobalFunction("_init");
                        }
                    }
                } else {
                    // Forward keys to Lua (just-pressed + repeats for repeatable keys)
                    for (int i = 0; i < activeKeyCount; i++) {
                        char key = activeKeys[i];
                        if (key != KEY_ESC) {
                            bool shouldFire = isJustPressed(key) || isRepeating(key) || isLongPressed(key);
                            if (shouldFire) {
                                lastActivityTime = now;
                                if (!LuaVM::callInputHandler(key)) {
                                    luaError = true;
                                    luaErrorMsg = LuaVM::getLastError();
                                }
                            }
                        }
                    }
                    
                    // Frame update
                    if (!luaError) {
                        if (!LuaVM::callGlobalFunction("_update")) {
                            luaError = true;
                            luaErrorMsg = LuaVM::getLastError();
                        }
                    }
                }
            } else if (currentMode == MODE_SETTINGS) {
                App* settingsApp = (activeSettingsApp != nullptr) ? activeSettingsApp : static_cast<App*>(&appSettings);
                // Forward non-ESC input to settings (including repeats)
                for (int i = 0; i < activeKeyCount; i++) {
                    char key = activeKeys[i];
                    if (key != KEY_ESC) {
                        bool shouldFire = isJustPressed(key) || isRepeating(key) || isLongPressed(key);
                        if (shouldFire) {
                            lastActivityTime = now;
                            settingsApp->handleInput(key);
                        }
                    }
                }

                settingsApp->update();
                if (settingsApp == &appT9Editor && appT9Editor.exitRequested) {
                    appT9Editor.exitRequested = false;
                    bool shouldReturnToLua = returnToLuaOnAppExit;
                    returnToLuaOnAppExit = false;
                    App* returnApp = (appTransferCaller != nullptr) ? appTransferCaller : static_cast<App*>(&appSettings);
                    appTransferCaller = nullptr;
                    if (shouldReturnToLua || returnApp == &appSettings) {
                        clearAppTransferState();
                    }
                    if (shouldReturnToLua) {
                        appT9Editor.stop();
                        activeSettingsApp = nullptr;
                        currentMode = MODE_LUA;
                    } else {
                        switchApp(returnApp);
                    }
                }
            }
        }
        
        // 5. RENDER
        if (currentMode == MODE_LUA) {
            if (luaError) {
                renderLuaError();
            } else {
                // Lua _draw() owns gfx.clear()/gfx.send() — no wrapping needed
                if (!LuaVM::callGlobalFunction("_draw")) {
                    luaError = true;
                    luaErrorMsg = LuaVM::getLastError();
                    renderLuaError();
                }
            }
        } else if (currentMode == MODE_SETTINGS) {
            u8g2.clearBuffer();
            if (activeSettingsApp != nullptr) {
                activeSettingsApp->render();
            } else {
                appSettings.render();
            }
            u8g2.sendBuffer();
        }
    }
}