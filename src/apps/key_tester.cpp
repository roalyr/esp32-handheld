// [Revision: v3.0] [Path: src/apps/key_tester.cpp] [Date: 2025-12-11]
// Description: Key Matrix Tester - refactored to use unified GUI module.

#include "key_tester.h"
#include "../gui.h"

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
        default:        return nullptr;  // Regular key
    }
}

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
    GUI::drawHeader("KEY TESTER");
    GUI::setFontSystem();
    const GUI::FontMetrics& metrics = GUI::getSystemFontMetrics();

    // Last Key Display
    u8g2.drawUTF8(0, GUI::getContentBaselineStart(), "LAST:");
    if (lastPressedKey != ' ') {
        const char* name = getKeyName(lastPressedKey);
        if (name) {
            u8g2.drawStr(35, GUI::getContentBaselineStart() + metrics.lineHeight, name);
        } else {
            char keyStr[2] = {lastPressedKey, '\0'}; 
            u8g2.drawUTF8(50, GUI::getContentBaselineStart() + metrics.lineHeight, keyStr);
        }
    }
    
    u8g2.drawUTF8(0, GUI::getFooterBaselineY(), "HELD:");
    int xPos = 28;
    
    for(int i=0; i<activeKeyCount; i++) {
        const char* name = getKeyName(activeKeys[i]);
        if (name) {
            u8g2.drawStr(xPos, GUI::getFooterBaselineY(), name);
            xPos += u8g2.getStrWidth(name) + 3;
        } else {
            char buf[4] = {'[', activeKeys[i], ']', '\0'};
            u8g2.drawStr(xPos, GUI::getFooterBaselineY(), buf);
            xPos += 15;
        }
    }
}