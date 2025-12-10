// [Revision: v2.0] [Path: src/apps/menu.cpp] [Date: 2025-12-10]
// Description: Menu logic. No changes to logic, just recompiling with new ITEM_COUNT from header.

#include "menu.h"

MenuApp::MenuApp() {
    selectedIndex = 0;
    pendingSwitchIndex = -1;
}

void MenuApp::start() {
    u8g2.setContrast(systemContrast);
    pendingSwitchIndex = -1;
}

void MenuApp::stop() {}

void MenuApp::update() {}

int MenuApp::getPendingSwitch() {
    int temp = pendingSwitchIndex;
    pendingSwitchIndex = -1; 
    return temp;
}

void MenuApp::handleInput(char key) {
    if (key == '2') {
        selectedIndex--;
        if (selectedIndex < 0) selectedIndex = ITEM_COUNT - 1;
    }
    if (key == '8') {
        selectedIndex++;
        if (selectedIndex >= ITEM_COUNT) selectedIndex = 0;
    }
    if (key == '5') {
        pendingSwitchIndex = selectedIndex;
    }
}

void MenuApp::render() {
    u8g2.setFont(FONT_SMALL);
    u8g2.drawBox(0, 0, 128, 10);
    u8g2.setDrawColor(0); 
    u8g2.drawStr(2, 8, "MAIN MENU");
    u8g2.setDrawColor(1); 

    int startY = 22;
    int lineHeight = 9; // Slightly tighter spacing to fit 5 items

    for(int i=0; i<ITEM_COUNT; i++) {
        int y = startY + (i * lineHeight);
        if (i == selectedIndex) {
            u8g2.drawStr(0, y, ">");
            u8g2.drawBox(8, y - 8, 120, 10); 
            u8g2.setDrawColor(0); 
        } else {
            u8g2.setDrawColor(1);
        }
        u8g2.drawUTF8(10, y, menuItems[i]);
    }
    u8g2.setDrawColor(1);
}