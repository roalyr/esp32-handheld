// [Revision: v2.0] [Path: src/apps/key_tester.cpp] [Date: 2025-12-10]
// Description: Implementation of the Key Matrix Tester Application.

#include "key_tester.h"

KeyTesterApp::KeyTesterApp() {
    lastPressedKey = ' ';
    // Initialize history buffer
    for(int i=0; i<HISTORY_SIZE; i++) keyHistory[i] = ' ';
    keyHistory[HISTORY_SIZE] = '\0';
}

void KeyTesterApp::start() {
    u8g2.setContrast(systemContrast);
    // Clear state on entry
    lastPressedKey = ' ';
    for(int i=0; i<HISTORY_SIZE; i++) keyHistory[i] = ' ';
}

void KeyTesterApp::stop() {
    // No cleanup required
}

void KeyTesterApp::update() {
    // No continuous logic needed for this app
}

void KeyTesterApp::addToHistory(char c) {
    // Shift history left
    for(int i=0; i < HISTORY_SIZE-1; i++) keyHistory[i] = keyHistory[i+1];
    // Add new character at the end
    keyHistory[HISTORY_SIZE-1] = c;
}

void KeyTesterApp::handleInput(char key) {
    lastPressedKey = key;
    addToHistory(key);
}

void KeyTesterApp::render() {
    // 1. Draw Header Frame
    u8g2.drawFrame(0, 0, 128, 15);
    u8g2.setFont(FONT_SMALL);
    
    // 2. Draw History
    u8g2.drawUTF8(2, 10, keyHistory);

    // 3. Last Key Display
    u8g2.drawUTF8(0, 28, "LAST PRESSED:");
    if (lastPressedKey != ' ') {
        u8g2.setFont(u8g2_font_inr24_t_cyrillic);
        char keyStr[2] = {lastPressedKey, '\0'}; 
        u8g2.drawUTF8(50, 58, keyStr);
    }
    
    // 4. Currently Held Keys
    // Accessing global activeKeys from hal.h
    u8g2.setFont(FONT_SMALL);
    u8g2.drawUTF8(0, 63, "HELD:");
    int xPos = 25;
    
    for(int i=0; i<activeKeyCount; i++) {
       char buf[4];
       buf[0] = '[';
       buf[1] = activeKeys[i];
       buf[2] = ']';
       buf[3] = '\0';
       u8g2.drawStr(xPos, 63, buf);
       xPos += 15;
    }
}