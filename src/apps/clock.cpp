// [Revision: v1.0] [Path: src/apps/clock.cpp] [Date: 2025-12-11]
// Description: Clock app for viewing and setting system time.
// Controls: [ENTER] Enter/Exit edit mode, [UP/DOWN] Adjust value, [LEFT/RIGHT] Move field

#include "clock.h"
#include "../gui.h"
#include "../clock.h"

ClockApp::ClockApp() {
    editMode = false;
    editField = 0;
    editHours = 0;
    editMinutes = 0;
    editSeconds = 0;
    lastBlinkTime = 0;
    blinkOn = true;
}

void ClockApp::start() {
    u8g2.setContrast(systemContrast);
    editMode = false;
    editField = 0;
    // Load current time into edit values
    editHours = SystemClock::getHours();
    editMinutes = SystemClock::getMinutes();
    editSeconds = SystemClock::getSeconds();
    lastBlinkTime = millis();
    blinkOn = true;
}

void ClockApp::stop() {
    // Nothing to clean up
}

void ClockApp::update() {
    // Cursor blink in edit mode
    if (editMode) {
        if (millis() - lastBlinkTime >= 400) {
            lastBlinkTime = millis();
            blinkOn = !blinkOn;
        }
    }
}

void ClockApp::handleInput(char key) {
    if (!editMode) {
        // View mode
        if (key == KEY_ENTER) {
            // Enter edit mode
            editMode = true;
            editField = 0;
            editHours = SystemClock::getHours();
            editMinutes = SystemClock::getMinutes();
            editSeconds = SystemClock::getSeconds();
        }
    } else {
        // Edit mode
        if (key == KEY_ENTER) {
            // Save and exit edit mode
            SystemClock::setTime(editHours, editMinutes, editSeconds);
            editMode = false;
        }
        
        // Move between fields
        if (key == KEY_LEFT) {
            editField--;
            if (editField < 0) editField = 2;
        }
        if (key == KEY_RIGHT) {
            editField++;
            if (editField > 2) editField = 0;
        }
        
        // Adjust value
        int* currentValue = nullptr;
        int maxValue = 0;
        
        switch (editField) {
            case 0: currentValue = &editHours; maxValue = 23; break;
            case 1: currentValue = &editMinutes; maxValue = 59; break;
            case 2: currentValue = &editSeconds; maxValue = 59; break;
        }
        
        if (currentValue) {
            if (key == KEY_UP) {
                (*currentValue)++;
                if (*currentValue > maxValue) *currentValue = 0;
            }
            if (key == KEY_DOWN) {
                (*currentValue)--;
                if (*currentValue < 0) *currentValue = maxValue;
            }
        }
        
        // Cancel with ESC
        if (key == KEY_ESC) {
            editMode = false;
        }
    }
}

void ClockApp::render() {
    if (editMode) {
        GUI::drawHeader("SET TIME");
    } else {
        GUI::drawHeader("CLOCK");
    }
    
    // Get display values
    int h, m, s;
    if (editMode) {
        h = editHours;
        m = editMinutes;
        s = editSeconds;
    } else {
        h = SystemClock::getHours();
        m = SystemClock::getMinutes();
        s = SystemClock::getSeconds();
    }
    
    // Draw large time display
    u8g2.setFont(u8g2_font_ncenB14_tr);
    
    char timeBuf[12];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", h, m, s);
    
    int width = u8g2.getStrWidth(timeBuf);
    int x = (GUI::SCREEN_WIDTH - width) / 2;
    int y = 38;
    
    u8g2.drawStr(x, y, timeBuf);
    
    // In edit mode, draw underline for selected field
    if (editMode && blinkOn) {
        // Calculate position of each field
        // Format: "HH:MM:SS" - each digit pair is ~18px wide at this font
        int charWidth = u8g2.getStrWidth("00");
        int colonWidth = u8g2.getStrWidth(":");
        
        int fieldX = x;
        if (editField == 1) fieldX = x + charWidth + colonWidth;
        if (editField == 2) fieldX = x + 2 * (charWidth + colonWidth);
        
        // Draw underline
        u8g2.drawHLine(fieldX, y + 2, charWidth);
    }
    
    // Footer
    if (editMode) {
        GUI::drawFooterHints("^v:Val </>:Fld", "Enter");
    } else {
        GUI::drawFooterHints("Enter:Edit", "Esc:Back");
    }
}
