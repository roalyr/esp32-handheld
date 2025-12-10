// [Revision: v1.0] [Path: src/t9_engine.h] [Date: 2025-12-09]
// Description: T9 Predictive Text / Multi-tap Engine class declaration.

#ifndef T9_ENGINE_H
#define T9_ENGINE_H

#include <Arduino.h>
#include "config.h"

class T9Engine {
  private:
    // Input State
    char pendingKey = '\0';          // The number key currently being cycled
    int cycleIndex = 0;              // Current index in the character map for the key
    unsigned long lastPressTime = 0; // Timestamp of last press for timeout logic
    bool isShifted = false;          // Shift/Caps Lock state

    // UTF-8 Helper Methods
    int getUtf8Length(const char* str);
    String getUtf8CharAtIndex(const char* str, int index);

  public:
    // Editor State (Public for Renderer access)
    String textBuffer = "";
    bool pendingCommit = false; // True if user is currently cycling a character

    // Getters
    unsigned long getLastPressTime();
    String getCurrentChar(); // Returns the character currently being cycled

    // Core Logic
    void handleInput(char key); // Process raw key input
    void update();              // Call in loop to handle timeout commits
    void commit();              // Force commit of pending character
    void reset();               // Clear buffer and state
};

extern T9Engine engine;

#endif