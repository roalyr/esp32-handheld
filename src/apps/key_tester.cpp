// [Revision: v3.0] [Path: src/apps/key_tester.cpp] [Date: 2025-12-11]
// Description: Key Matrix Tester - refactored to use unified GUI module.

#include "key_tester.h"
#include "../gui.h"

KeyTesterApp::KeyTesterApp() {
    lastPressedKey = ' ';
    for(int i=0; i<HISTORY_SIZE; i++) keyHistory[i] = ' ';
    keyHistory[HISTORY_SIZE] = '\0';
}

void KeyTesterApp::start() {
    u8g2.setContrast(systemContrast);
    lastPressedKey = ' ';
    for(int i=0; i<HISTORY_SIZE; i++) keyHistory[i] = ' ';
}

void KeyTesterApp::stop() {}

void KeyTesterApp::update() {}

void KeyTesterApp::addToHistory(char c) {
    for(int i=0; i < HISTORY_SIZE-1; i++) keyHistory[i] = keyHistory[i+1];
    keyHistory[HISTORY_SIZE-1] = c;
}

void KeyTesterApp::handleInput(char key) {
    lastPressedKey = key;
    addToHistory(key);
}

void KeyTesterApp::render() {
    // Header with history
    GUI::setFontSmall();
    u8g2.drawFrame(0, 0, GUI::SCREEN_WIDTH, 15);
    u8g2.drawUTF8(2, 10, keyHistory);

    // Last Key Display
    u8g2.drawUTF8(0, 28, "LAST PRESSED:");
    if (lastPressedKey != ' ') {
        u8g2.setFont(u8g2_font_inr24_t_cyrillic);
        char keyStr[2] = {lastPressedKey, '\0'}; 
        u8g2.drawUTF8(50, 58, keyStr);
    }
    
    // Currently Held Keys
    GUI::setFontSmall();
    u8g2.drawUTF8(0, 63, "HELD:");
    int xPos = 25;
    
    for(int i=0; i<activeKeyCount; i++) {
       char buf[4] = {'[', activeKeys[i], ']', '\0'};
       u8g2.drawStr(xPos, 63, buf);
       xPos += 15;
    }
}