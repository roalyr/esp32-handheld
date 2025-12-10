// [Revision: v1.0] [Path: src/apps/menu.cpp] [Date: 2025-12-10]
// Description: Logic for the Main Menu (Navigation and Selection).

#include "menu.h"

MenuApp::MenuApp() {
    selectedIndex = 0;
    pendingSwitchIndex = -1;
}

void MenuApp::start() {
    u8g2.setContrast(systemContrast);
    // Do not reset selectedIndex here so we remember cursor position
    pendingSwitchIndex = -1;
}

void MenuApp::stop() {
    // No cleanup needed
}

void MenuApp::update() {
    // No continuous animation needed
}

int MenuApp::getPendingSwitch() {
    int temp = pendingSwitchIndex;
    pendingSwitchIndex = -1; // Reset after reading
    return temp;
}

void MenuApp::handleInput(char key) {
    // UP (2)
    if (key == '2') {
        selectedIndex--;
        if (selectedIndex < 0) selectedIndex = ITEM_COUNT - 1; // Wrap around
    }
    
    // DOWN (8)
    if (key == '8') {
        selectedIndex++;
        if (selectedIndex >= ITEM_COUNT) selectedIndex = 0; // Wrap around
    }

    // ENTER (5) or OK (A/B/C/D context dependent, using 5 for now)
    if (key == '5') {
        pendingSwitchIndex = selectedIndex;
    }
}

void MenuApp::render() {
    u8g2.setFont(FONT_SMALL);
    
    // Draw Header
    u8g2.drawBox(0, 0, 128, 10);
    u8g2.setDrawColor(0); // Invert text
    u8g2.drawStr(2, 8, "MAIN MENU");
    u8g2.setDrawColor(1); // Normal text

    // Draw Items
    int startY = 22;
    int lineHeight = 10;

    for(int i=0; i<ITEM_COUNT; i++) {
        int y = startY + (i * lineHeight);
        
        // Draw Selection Cursor
        if (i == selectedIndex) {
            u8g2.drawStr(0, y, ">");
            u8g2.drawBox(8, y - 8, 120, 10); // Highlight bar
            u8g2.setDrawColor(0); // Invert text for selected item
        } else {
            u8g2.setDrawColor(1);
        }

        u8g2.drawUTF8(10, y, menuItems[i]);
    }
    
    // Reset draw color for safety
    u8g2.setDrawColor(1);
}