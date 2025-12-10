// [Revision: v1.0] [Path: src/apps/key_tester.cpp] [Date: 2025-12-09]
// Description: Implementation of the Key Matrix Tester application.

#include "key_tester.h"

// --------------------------------------------------------------------------
// LOCAL STATE
// --------------------------------------------------------------------------

const int HISTORY_SIZE = 14;
char keyHistory[HISTORY_SIZE + 1];

// --------------------------------------------------------------------------
// LOGIC
// --------------------------------------------------------------------------

void setupTester() {
  // Clear history buffer with spaces
  for(int i=0; i<HISTORY_SIZE; i++) keyHistory[i] = ' ';
  keyHistory[HISTORY_SIZE] = '\0';
}

void addToTesterHistory(char c) {
    // Shift history left
    for(int i=0; i < HISTORY_SIZE-1; i++) keyHistory[i] = keyHistory[i+1];
    // Add new character at the end
    keyHistory[HISTORY_SIZE-1] = c;
}

// --------------------------------------------------------------------------
// RENDER
// --------------------------------------------------------------------------

void renderKeyTester(char lastKey, char* heldKeys, int count) {
    // 1. Draw Header Frame
    u8g2.drawFrame(0, 0, 128, 15);
    u8g2.setFont(FONT_SMALL);
    
    // 2. Draw History (inside the frame)
    u8g2.drawUTF8(2, 10, keyHistory);

    // 3. Last Key Display (Large Font)
    u8g2.drawUTF8(0, 28, "LAST PRESSED:");
    if (lastKey != ' ') {
        u8g2.setFont(u8g2_font_inr24_t_cyrillic);
        char keyStr[2] = {lastKey, '\0'}; 
        u8g2.drawUTF8(50, 58, keyStr);
    }
    
    // 4. Currently Held Keys List
    u8g2.setFont(FONT_SMALL);
    u8g2.drawUTF8(0, 63, "HELD:");
    int xPos = 25;
    
    for(int i=0; i<count; i++) {
       char buf[4];
       buf[0] = '[';
       buf[1] = heldKeys[i];
       buf[2] = ']';
       buf[3] = '\0';
       u8g2.drawStr(xPos, 63, buf);
       xPos += 15;
    }
}