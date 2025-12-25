// [Revision: v1.0] [Path: src/apps/settings.cpp] [Date: 2025-12-11]
// Description: Settings app for contrast, brightness adjustment and system info.
// Controls: [UP/DOWN] Navigate, [LEFT/RIGHT] Adjust value, [ENTER] Toggle edit

#include "settings.h"
#include "../gui.h"
#include "../config.h"
#include <SPIFFS.h>

// External sleep control (defined in main.cpp)
extern bool sleepEnabled;

SettingsApp::SettingsApp() {
    selectedIndex = 0;
    editMode = false;
    tempContrast = systemContrast;
    tempBrightness = systemBrightness;
    tempSleepEnabled = SLEEP_ENABLED;
}

void SettingsApp::start() {
    tempContrast = systemContrast;
    tempBrightness = systemBrightness;
    tempSleepEnabled = sleepEnabled;
    editMode = false;
    selectedIndex = 0;
}

void SettingsApp::stop() {
    // Apply settings on exit
    systemContrast = tempContrast;
    systemBrightness = tempBrightness;
    sleepEnabled = tempSleepEnabled;
}

void SettingsApp::update() {}

void SettingsApp::handleInput(char key) {
    if (!editMode) {
        // Navigation mode
        if (key == KEY_UP) {
            selectedIndex--;
            if (selectedIndex < 0) selectedIndex = SETTING_COUNT - 1;
        }
        if (key == KEY_DOWN) {
            selectedIndex++;
            if (selectedIndex >= SETTING_COUNT) selectedIndex = 0;
        }
        if (key == KEY_ENTER && selectedIndex != SETTING_INFO) {
            editMode = true;
        }
    } else {
        // Edit mode
        if (key == KEY_ENTER) {
            editMode = false;
            // Apply changes
            if (selectedIndex == SETTING_CONTRAST) {
                systemContrast = tempContrast;
                u8g2.setContrast(systemContrast);
            }
            if (selectedIndex == SETTING_BRIGHTNESS) {
                systemBrightness = tempBrightness;
                ledcWrite(0, systemBrightness);  // Channel 0
            }
            if (selectedIndex == SETTING_SLEEP) {
                sleepEnabled = tempSleepEnabled;
            }
        }
        
        // Adjust values
        if (selectedIndex == SETTING_CONTRAST) {
            if (key == KEY_RIGHT) {
                tempContrast += 5;
                if (tempContrast > 255) tempContrast = 255;
                u8g2.setContrast(tempContrast);  // Live preview
            }
            if (key == KEY_LEFT) {
                tempContrast -= 5;
                if (tempContrast < 0) tempContrast = 0;
                u8g2.setContrast(tempContrast);  // Live preview
            }
        }
        
        if (selectedIndex == SETTING_BRIGHTNESS) {
            if (key == KEY_RIGHT) {
                tempBrightness += 15;
                if (tempBrightness > 255) tempBrightness = 255;
                ledcWrite(0, tempBrightness);  // Live preview
            }
            if (key == KEY_LEFT) {
                tempBrightness -= 15;
                if (tempBrightness < 10) tempBrightness = 10;  // Min 10 so screen visible
                ledcWrite(0, tempBrightness);  // Live preview
            }
        }
        
        if (selectedIndex == SETTING_SLEEP) {
            if (key == KEY_LEFT || key == KEY_RIGHT) {
                tempSleepEnabled = !tempSleepEnabled;
            }
        }
    }
}

void SettingsApp::render() {
    GUI::drawHeader("SETTINGS");
    GUI::setFontSmall();
    
    int y = GUI::CONTENT_START_Y;
    
    // Contrast setting
    {
        bool isSelected = (selectedIndex == SETTING_CONTRAST);
        if (isSelected && !editMode) {
            u8g2.drawBox(0, y - 8, GUI::SCREEN_WIDTH - 4, GUI::LINE_HEIGHT);
            u8g2.setDrawColor(0);
        }
        
        char buf[32];
        if (editMode && isSelected) {
            snprintf(buf, sizeof(buf), "Contrast: <%d>", tempContrast);
        } else {
            snprintf(buf, sizeof(buf), "Contrast: %d", tempContrast);
        }
        u8g2.drawStr(2, y, buf);
        
        if (isSelected && !editMode) u8g2.setDrawColor(1);
    }
    y += GUI::LINE_HEIGHT;
    
    // Brightness setting
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
    
    // Sleep setting
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
    
    // System info (read-only)
    {
        bool isSelected = (selectedIndex == SETTING_INFO);
        if (isSelected) {
            u8g2.drawBox(0, y - 8, GUI::SCREEN_WIDTH - 4, GUI::LINE_HEIGHT);
            u8g2.setDrawColor(0);
        }
        
        char buf[32];
        unsigned long freeKB = (SPIFFS.totalBytes() - SPIFFS.usedBytes()) / 1024;
        snprintf(buf, sizeof(buf), "Free: %luKB  v%s", freeKB, FIRMWARE_VERSION);
        u8g2.drawStr(2, y, buf);
        
        if (isSelected) u8g2.setDrawColor(1);
    }
    
    // Footer
    if (editMode) {
        GUI::drawFooterHints("</>:Adj", "Enter:Done");
    } else {
        GUI::drawFooterHints("Enter:Edit", "Esc:Back");
    }
}
