// [Revision: v2.3] [Path: src/apps/menu.cpp] [Date: 2025-12-10]
// Description: Added File Browser menu item to app list.
// Visuals: Shows 4 items at a time.

#include "menu.h"

// Configuration
const int VISIBLE_ITEMS = 4;
const int HEADER_HEIGHT = 12;
const int LINE_HEIGHT = 10;
const int START_Y = 24; // Y position of the first item line

MenuApp::MenuApp() {
    selectedIndex = 0;
    scrollOffset = 0;
    pendingSwitchIndex = -1;
}

void MenuApp::start() {
    u8g2.setContrast(systemContrast);
    pendingSwitchIndex = -1;
    // Keep previous selection/scroll state or reset if desired:
    // selectedIndex = 0; 
    // scrollOffset = 0;
}

void MenuApp::stop() {}
void MenuApp::update() {}

int MenuApp::getPendingSwitch() {
    int temp = pendingSwitchIndex;
    pendingSwitchIndex = -1; 
    return temp;
}

void MenuApp::handleInput(char key) {
    // UP (2)
    if (key == '2') {
        selectedIndex--;
        if (selectedIndex < 0) {
            // Wrap to bottom
            selectedIndex = ITEM_COUNT - 1;
            scrollOffset = ITEM_COUNT - VISIBLE_ITEMS;
            if (scrollOffset < 0) scrollOffset = 0;
        } else if (selectedIndex < scrollOffset) {
            // Scroll Up
            scrollOffset--;
        }
    }
    
    // DOWN (8)
    if (key == '8') {
        selectedIndex++;
        if (selectedIndex >= ITEM_COUNT) {
            // Wrap to top
            selectedIndex = 0;
            scrollOffset = 0;
        } else if (selectedIndex >= scrollOffset + VISIBLE_ITEMS) {
            // Scroll Down
            scrollOffset++;
        }
    }
    
    // ENTER (5)
    if (key == '5') {
        pendingSwitchIndex = selectedIndex;
    }
}

void MenuApp::render() {
    u8g2.setFont(FONT_SMALL);
    
    // 1. Draw Header
    u8g2.drawBox(0, 0, 128, HEADER_HEIGHT);
    u8g2.setDrawColor(0); 
    u8g2.drawStr(2, 9, "MAIN MENU");
    u8g2.setDrawColor(1); 
    
    // 2. Draw Scrollbar (Simple indicator)
    // Draw a line on the right edge representing the full list size
    // and a box representing the current view
    int scrollBarH = 64 - HEADER_HEIGHT;
    int knobH = (scrollBarH * VISIBLE_ITEMS) / ITEM_COUNT;
    int knobY = HEADER_HEIGHT + (scrollBarH * scrollOffset) / ITEM_COUNT;
    
    u8g2.drawVLine(126, HEADER_HEIGHT, scrollBarH); // Track
    u8g2.drawBox(125, knobY, 3, knobH);             // Knob

    // 3. Draw Visible Items
    for(int i = 0; i < VISIBLE_ITEMS; i++) {
        int itemIndex = scrollOffset + i;
        if (itemIndex >= ITEM_COUNT) break;

        int y = START_Y + (i * LINE_HEIGHT);
        
        // Highlight Selection
        if (itemIndex == selectedIndex) {
            u8g2.drawStr(0, y, ">");
            u8g2.drawBox(8, y - 8, 115, 10); 
            u8g2.setDrawColor(0); 
        } else {
            u8g2.setDrawColor(1);
        }
        
        u8g2.drawUTF8(10, y, menuItems[itemIndex]);
    }
    
    // Safety reset
    u8g2.setDrawColor(1);
}