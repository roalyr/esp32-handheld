// [Revision: v1.0] [Path: src/apps/settings.cpp] [Date: 2025-12-11]
// Description: Settings app for contrast adjustment and system info.
// Controls: [2/8] Navigate, [4/6] Adjust value, [5] Toggle edit

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
    tempSleepEnabled = SLEEP_ENABLED;
}

void SettingsApp::start() {
    tempContrast = systemContrast;
    tempSleepEnabled = sleepEnabled;
    editMode = false;
    selectedIndex = 0;
}

void SettingsApp::stop() {
    // Apply contrast on exit
    systemContrast = tempContrast;
    sleepEnabled = tempSleepEnabled;
}

void SettingsApp::update() {}

void SettingsApp::handleInput(char key) {
    if (!editMode) {
        // Navigation mode
        if (key == '2') {
            selectedIndex--;
            if (selectedIndex < 0) selectedIndex = SETTING_COUNT - 1;
        }
        if (key == '8') {
            selectedIndex++;
            if (selectedIndex >= SETTING_COUNT) selectedIndex = 0;
        }
        if (key == '5' && selectedIndex != SETTING_INFO) {
            editMode = true;
        }
    } else {
        // Edit mode
        if (key == '5') {
            editMode = false;
            // Apply changes immediately for contrast preview
            if (selectedIndex == SETTING_CONTRAST) {
                systemContrast = tempContrast;
                u8g2.setContrast(systemContrast);
            }
            if (selectedIndex == SETTING_SLEEP) {
                sleepEnabled = tempSleepEnabled;
            }
        }
        
        // Adjust values
        if (selectedIndex == SETTING_CONTRAST) {
            if (key == '6') {
                tempContrast += 5;
                if (tempContrast > 255) tempContrast = 255;
                u8g2.setContrast(tempContrast);  // Live preview
            }
            if (key == '4') {
                tempContrast -= 5;
                if (tempContrast < 0) tempContrast = 0;
                u8g2.setContrast(tempContrast);  // Live preview
            }
        }
        
        if (selectedIndex == SETTING_SLEEP) {
            if (key == '4' || key == '6') {
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
        GUI::drawFooterHints("4/6:Adjust", "5:Done");
    } else {
        GUI::drawFooterHints("5:Edit", "D:Back");
    }
}
