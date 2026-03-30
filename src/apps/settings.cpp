// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/apps/settings.cpp
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TRUTH_HARDWARE.md Sections 0, 3
// LOG_REF: 2026-03-30
// Description: Settings app — brightness, sleep toggle, key tester, SD card info.

#include "settings.h"
#include "../gui.h"
#include "../config.h"
#include "../clock.h"
#include "../hal.h"

// External sleep control (defined in main.cpp)
extern bool sleepEnabled;

// Helper to get readable name for special keys
static const char* getKeyName(char key) {
    switch(key) {
        case KEY_ESC:   return "ESC";
        case KEY_BKSP:  return "BKSP";
        case KEY_TAB:   return "TAB";
        case KEY_ENTER: return "ENTR";
        case KEY_SHIFT: return "SHFT";
        case KEY_ALT:   return "ALT";
        case KEY_UP:    return "UP";
        case KEY_DOWN:  return "DOWN";
        case KEY_LEFT:  return "LEFT";
        case KEY_RIGHT: return "RGHT";
        default:        return nullptr;
    }
}

SettingsApp::SettingsApp() {
    selectedIndex = 0;
    editMode = false;
    inKeyTester = false;
    tempBrightness = systemBrightness;
    tempSleepEnabled = SLEEP_ENABLED;
    lastPressedKey = ' ';
    for (int i = 0; i < HISTORY_SIZE; i++) keyHistory[i] = ' ';
    keyHistory[HISTORY_SIZE] = '\0';
}

void SettingsApp::start() {
    tempBrightness = systemBrightness;
    tempSleepEnabled = sleepEnabled;
    editMode = false;
    inKeyTester = false;
    selectedIndex = 0;
}

void SettingsApp::stop() {
    systemBrightness = tempBrightness;
    sleepEnabled = tempSleepEnabled;
    inKeyTester = false;
}

bool SettingsApp::isInSubmenu() {
    return inKeyTester;
}

void SettingsApp::update() {}

void SettingsApp::addToHistory(char c) {
    for (int i = 0; i < HISTORY_SIZE - 1; i++) keyHistory[i] = keyHistory[i + 1];
    keyHistory[HISTORY_SIZE - 1] = c;
}

void SettingsApp::handleInput(char key) {
    if (inKeyTester) {
        if (key == KEY_ESC) {
            inKeyTester = false;
        } else {
            lastPressedKey = key;
            addToHistory(key);
        }
        return;
    }

    if (!editMode) {
        if (key == KEY_UP) {
            selectedIndex--;
            if (selectedIndex < 0) selectedIndex = SETTING_COUNT - 1;
        }
        if (key == KEY_DOWN) {
            selectedIndex++;
            if (selectedIndex >= SETTING_COUNT) selectedIndex = 0;
        }
        if (key == KEY_ENTER) {
            if (selectedIndex == SETTING_KEY_TESTER) {
                inKeyTester = true;
                lastPressedKey = ' ';
                for (int i = 0; i < HISTORY_SIZE; i++) keyHistory[i] = ' ';
            } else {
                editMode = true;
            }
        }
    } else {
        if (key == KEY_ENTER) {
            editMode = false;
            if (selectedIndex == SETTING_BRIGHTNESS) {
                systemBrightness = tempBrightness;
                ledcWrite(0, systemBrightness);
            }
            if (selectedIndex == SETTING_SLEEP) {
                sleepEnabled = tempSleepEnabled;
            }
        }
        
        if (selectedIndex == SETTING_BRIGHTNESS) {
            if (key == KEY_RIGHT) {
                tempBrightness += 15;
                if (tempBrightness > 255) tempBrightness = 255;
                ledcWrite(0, tempBrightness);
            }
            if (key == KEY_LEFT) {
                tempBrightness -= 15;
                if (tempBrightness < 10) tempBrightness = 10;
                ledcWrite(0, tempBrightness);
            }
        }
        
        if (selectedIndex == SETTING_SLEEP) {
            if (key == KEY_LEFT || key == KEY_RIGHT) {
                tempSleepEnabled = !tempSleepEnabled;
            }
        }
    }
}

void SettingsApp::renderInfoHeader() {
    // Header: "Settings  vX.X.X  HH:MM"
    char timeBuf[8];
    SystemClock::getTimeString(timeBuf, sizeof(timeBuf));
    
    char headerBuf[40];
    snprintf(headerBuf, sizeof(headerBuf), "v%s %s", FIRMWARE_VERSION, timeBuf);
    
    GUI::drawHeader("Settings", headerBuf);
}

void SettingsApp::renderSettingsList() {
    renderInfoHeader();
    GUI::setFontSmall();
    
    int y = GUI::CONTENT_START_Y;
    
    // Brightness
    {
        bool isSelected = (selectedIndex == SETTING_BRIGHTNESS);
        if (isSelected && !editMode) {
            u8g2.drawBox(0, y - 8, GUI::SCREEN_WIDTH - 4, GUI::LINE_HEIGHT);
            u8g2.setDrawColor(0);
        }
        char buf[32];
        int pct = (tempBrightness * 100) / 255;
        if (editMode && isSelected) {
            snprintf(buf, sizeof(buf), "Backlight: <%d%%>", pct);
        } else {
            snprintf(buf, sizeof(buf), "Backlight: %d%%", pct);
        }
        u8g2.drawStr(2, y, buf);
        if (isSelected && !editMode) u8g2.setDrawColor(1);
    }
    y += GUI::LINE_HEIGHT;
    
    // Sleep
    {
        bool isSelected = (selectedIndex == SETTING_SLEEP);
        if (isSelected && !editMode) {
            u8g2.drawBox(0, y - 8, GUI::SCREEN_WIDTH - 4, GUI::LINE_HEIGHT);
            u8g2.setDrawColor(0);
        }
        char buf[32];
        if (editMode && isSelected) {
            snprintf(buf, sizeof(buf), "Sleep: <%s>", tempSleepEnabled ? "ON" : "OFF");
        } else {
            snprintf(buf, sizeof(buf), "Sleep: %s", tempSleepEnabled ? "ON" : "OFF");
        }
        u8g2.drawStr(2, y, buf);
        if (isSelected && !editMode) u8g2.setDrawColor(1);
    }
    y += GUI::LINE_HEIGHT;
    
    // Key Tester
    {
        bool isSelected = (selectedIndex == SETTING_KEY_TESTER);
        if (isSelected) {
            u8g2.drawBox(0, y - 8, GUI::SCREEN_WIDTH - 4, GUI::LINE_HEIGHT);
            u8g2.setDrawColor(0);
        }
        u8g2.drawStr(2, y, "Key Tester...");
        if (isSelected) u8g2.setDrawColor(1);
    }
    y += GUI::LINE_HEIGHT;
    
    // System info line (read-only, not selectable)
    {
        int freeRamK = ESP.getFreeHeap() / 1024;
        char buf[40];
        if (isSDMounted()) {
            uint64_t totalMB = sdTotalBytes() / (1024*1024);
            uint64_t usedMB = sdUsedBytes() / (1024*1024);
            snprintf(buf, sizeof(buf), "RAM %dk SD %llu/%lluM", freeRamK, (unsigned long long)usedMB, (unsigned long long)totalMB);
        } else {
            snprintf(buf, sizeof(buf), "RAM %dk SD:none", freeRamK);
        }
        u8g2.drawStr(2, y, buf);
    }
}

void SettingsApp::renderKeyTester() {
    GUI::drawHeader("KEY TESTER");
    GUI::setFontSmall();
    
    // Last Key Display
    u8g2.drawUTF8(0, 24, "LAST:");
    if (lastPressedKey != ' ') {
        const char* name = getKeyName(lastPressedKey);
        if (name) {
            u8g2.setFont(u8g2_font_ncenB14_tr);
            u8g2.drawStr(35, 38, name);
        } else {
            u8g2.setFont(u8g2_font_inr24_t_cyrillic);
            char keyStr[2] = {lastPressedKey, '\0'};
            u8g2.drawUTF8(50, 42, keyStr);
        }
    }
    
    // Currently Held Keys
    GUI::setFontSmall();
    u8g2.drawUTF8(0, 55, "HELD:");
    int xPos = 28;
    for (int i = 0; i < activeKeyCount; i++) {
        const char* name = getKeyName(activeKeys[i]);
        if (name) {
            u8g2.drawStr(xPos, 55, name);
            xPos += u8g2.getStrWidth(name) + 3;
        } else {
            char buf[4] = {'[', activeKeys[i], ']', '\0'};
            u8g2.drawStr(xPos, 55, buf);
            xPos += 15;
        }
    }
    
}

void SettingsApp::render() {
    if (inKeyTester) {
        renderKeyTester();
    } else {
        renderSettingsList();
    }
}
