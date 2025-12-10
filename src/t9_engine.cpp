// [Revision: v1.0] [Path: src/t9_engine.cpp] [Date: 2025-12-09]
// Description: Implementation of T9 multi-tap logic with UTF-8 support.

#include "t9_engine.h"

// --------------------------------------------------------------------------
// CHARACTER MAPS
// --------------------------------------------------------------------------

// Multi-tap mappings including Cyrillic characters
const char* t9Map[] = {
  " 0",                  // 0
  ".,?!1",               // 1
  "abc2абвгґ",           // 2
  "def3деєжз",           // 3
  "ghi4иіїйкл",          // 4
  "jkl5мноп",            // 5
  "mno6рсту",            // 6
  "pqrs7фхцч",           // 7
  "tuv8шщ",              // 8
  "wxyz9ьюя"             // 9
};

T9Engine engine;

// --------------------------------------------------------------------------
// UTF-8 HELPER METHODS
// --------------------------------------------------------------------------

// Calculate actual character count (not byte count) for UTF-8 string
int T9Engine::getUtf8Length(const char* str) {
  int len = 0;
  int i = 0;
  while(str[i] != 0) {
    // 0xC0 mask checks for start of multibyte sequence
    if ((str[i] & 0xC0) != 0x80) len++;
    i++;
  }
  return len;
}

// Extract specific character at visual index from UTF-8 string
String T9Engine::getUtf8CharAtIndex(const char* str, int index) {
  int count = 0;
  int i = 0;
  int start = 0;
  
  while(str[i] != 0) {
    if ((str[i] & 0xC0) != 0x80) { 
        if (count == index) start = i;
        if (count == index + 1) {
            String res = "";
            for(int k=start; k<i; k++) res += str[k];
            return res;
        }
        count++;
    }
    i++;
  }
  // Handle last character case
  if (count == index + 1) {
      String res = "";
      for(int k=start; k<i; k++) res += str[k];
      return res;
  }
  return "";
}

// --------------------------------------------------------------------------
// ENGINE LOGIC
// --------------------------------------------------------------------------

unsigned long T9Engine::getLastPressTime() { return lastPressTime; }

String T9Engine::getCurrentChar() {
  if (!pendingCommit) return "";
  int mapIndex = pendingKey - '0';
  if (mapIndex < 0 || mapIndex > 9) return String(pendingKey); 
  
  String c = getUtf8CharAtIndex(t9Map[mapIndex], cycleIndex);
  
  if (isShifted) {
      // Basic ASCII upper-casing
      if (c.length() == 1 && c[0] >= 'a' && c[0] <= 'z') {
        c = String((char)(c[0] - 32));
      }
      // Note: Cyrillic uppercase implementation omitted for brevity
  }
  return c;
}

void T9Engine::handleInput(char key) {
  unsigned long now = millis();

  // BACKSPACE (M)
  if (key == 'M') { 
    if (pendingCommit) {
        pendingCommit = false;
    } else if (textBuffer.length() > 0) {
        // Remove last UTF-8 character
        while(textBuffer.length() > 0) {
          char lastByte = textBuffer[textBuffer.length()-1];
          textBuffer.remove(textBuffer.length() - 1);
          if ((lastByte & 0xC0) != 0x80) break; 
        }
    }
    return;
  }
  
  // NEWLINE (Z)
  if (key == 'Z') {
    if (pendingCommit) commit();
    textBuffer += '\n';
    return;
  }
  
  // SPACE (#)
  if (key == '#') {
      if (pendingCommit) commit();
      textBuffer += ' ';
      return;
  }
  
  // SHIFT (*)
  if (key == '*') { 
    isShifted = !isShifted;
    return;
  }

  // NUMERIC KEYS (0-9)
  if (key >= '0' && key <= '9') {
    // If same key pressed within timeout -> cycle character
    if (pendingCommit && key == pendingKey && (now - lastPressTime < MULTITAP_TIMEOUT)) {
      cycleIndex++;
      int mapLen = getUtf8Length(t9Map[key - '0']);
      if (cycleIndex >= mapLen) cycleIndex = 0; 
      lastPressTime = now;
    } else {
      // New key pressed -> commit previous and start new cycle
      if (pendingCommit) commit();
      pendingKey = key;
      cycleIndex = 0;
      pendingCommit = true;
      lastPressTime = now;
    }
  } else {
      // Any other key commits current character
      if (pendingCommit) commit();
  }
}

void T9Engine::update() {
  // Auto-commit if timeout reached
  if (pendingCommit && (millis() - lastPressTime > MULTITAP_TIMEOUT)) {
    commit();
  }
}

void T9Engine::commit() {
  textBuffer += getCurrentChar();
  pendingCommit = false;
  cycleIndex = 0;
}

void T9Engine::reset() {
    textBuffer = "";
    pendingCommit = false;
}