// [Revision: v2.0] [Path: src/apps/t9_editor.cpp] [Date: 2025-12-10]
// Description: Implementation of T9 Editor Application class.

#include "t9_editor.h"

void T9EditorApp::start() {
    u8g2.setContrast(systemContrast); // Ensure standard contrast
    // Optional: engine.reset() if you want a fresh buffer every time
}

void T9EditorApp::stop() {
    // Cleanup if necessary
}

void T9EditorApp::update() {
    engine.update();
}

void T9EditorApp::handleInput(char key) {
    engine.handleInput(key);
}

void T9EditorApp::render() {
  // 1. Draw Header
  u8g2.drawHLine(0, 10, 128);
  u8g2.setFont(FONT_SMALL); 
  u8g2.drawUTF8(2, 8, "T9 EDITOR v2.0");
  
  // 2. Setup Text Position
  u8g2.setFont(u8g2_font_6x13_t_cyrillic); 
  int x = 2; 
  int y = 25;
  
  // 3. Draw Committed Text
  u8g2.drawUTF8(x, y, engine.textBuffer.c_str());
  
  // 4. Draw Pending Character (Candidate)
  if (engine.pendingCommit) {
    String pChar = engine.getCurrentChar();
    int width = u8g2.getUTF8Width(engine.textBuffer.c_str());
    
    // Draw candidate & underline
    u8g2.drawUTF8(x + width, y, pChar.c_str());
    u8g2.drawHLine(x + width, y+2, u8g2.getUTF8Width(pChar.c_str()));
    
    // Draw Timeout Progress Bar
    long timeLeft = MULTITAP_TIMEOUT - (millis() - engine.getLastPressTime());
    int barWidth = map(timeLeft, 0, MULTITAP_TIMEOUT, 0, 10);
    if (barWidth < 0) barWidth = 0; 
    u8g2.drawHLine(x + width, y+4, barWidth);
  }
  
  // 5. Draw Blinking Cursor
  if (!engine.pendingCommit && (millis() / CURSOR_BLINK_RATE) % 2) {
    int width = u8g2.getUTF8Width(engine.textBuffer.c_str());
    u8g2.drawVLine(x + width + 1, y - 10, 12);
  }
}