// [Revision: v2.1] [Path: src/t9_engine.h] [Date: 2025-12-10]
// Description: Added Cursor Position support for navigation and insertion.

#ifndef T9_ENGINE_H
#define T9_ENGINE_H

#include <Arduino.h>
#include "config.h"

class T9Engine {
  private:
    // Input State
    char pendingKey = '\0';          
    int cycleIndex = 0;              
    unsigned long lastPressTime = 0; 
    bool isShifted = false;          

    // UTF-8 Helper Methods
    int getUtf8Length(const char* str);
    String getUtf8CharAtIndex(const char* str, int index);

  public:
    // Editor State
    String textBuffer = "";
    int cursorPos = 0; // Current insertion point (byte index)
    bool pendingCommit = false; 

    // Getters
    unsigned long getLastPressTime();
    String getCurrentChar(); 

    // Core Logic
    void handleInput(char key); 
    void update();              
    void commit();              
    void reset();               
    
    // Navigation Helpers
    void moveCursor(int delta); // Move by UTF-8 characters
    void setCursor(int pos);    // Set raw byte index
};

extern T9Engine engine;

#endif