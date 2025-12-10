// [Revision: v2.1] [Path: src/t9_engine.cpp] [Date: 2025-12-10]
// Description: Implemented cursor-aware insertion and deletion.

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
    if ((str[i] & 0xC0) != 0x80) len++;
    i++;
  }
  return len;
}

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
  if (count == index + 1) {
      String res = "";
      for(int k=start; k<i; k++) res += str[k];
      return res;
  }
  return "";
}

unsigned long T9Engine::getLastPressTime() { return lastPressTime; }

String T9Engine::getCurrentChar() {
  if (!pendingCommit) return "";
  int mapIndex = pendingKey - '0';
  if (mapIndex < 0 || mapIndex > 9) return String(pendingKey); 
  
  String c = getUtf8CharAtIndex(t9Map[mapIndex], cycleIndex);
  
  if (isShifted) {
      if (c.length() == 1 && c[0] >= 'a' && c[0] <= 'z') {
        c = String((char)(c[0] - 32));
      }
  }
  return c;
}

void T9Engine::moveCursor(int delta) {
    if (pendingCommit) commit();

    // Move Forward
    if (delta > 0) {
        for(int i=0; i<delta; i++) {
            if (cursorPos >= textBuffer.length()) break;
            // Advance one UTF-8 char
            cursorPos++;
            while (cursorPos < textBuffer.length() && (textBuffer[cursorPos] & 0xC0) == 0x80) {
                cursorPos++;
            }
        }
    }
    // Move Backward
    else if (delta < 0) {
        for(int i=0; i < -delta; i++) {
            if (cursorPos <= 0) break;
            // Rewind one UTF-8 char
            cursorPos--;
            while (cursorPos > 0 && (textBuffer[cursorPos] & 0xC0) == 0x80) {
                cursorPos--;
            }
        }
    }
}

void T9Engine::setCursor(int pos) {
    if (pendingCommit) commit();
    cursorPos = pos;
    if (cursorPos < 0) cursorPos = 0;
    if (cursorPos > textBuffer.length()) cursorPos = textBuffer.length();
}

void T9Engine::handleInput(char key) {
  unsigned long now = millis();

  // BACKSPACE (M)
  if (key == 'M') { 
    if (pendingCommit) {
        pendingCommit = false;
    } else if (cursorPos > 0) {
        // Find start of previous character
        int prevPos = cursorPos - 1;
        while (prevPos > 0 && (textBuffer[prevPos] & 0xC0) == 0x80) {
            prevPos--;
        }
        textBuffer.remove(prevPos, cursorPos - prevPos);
        cursorPos = prevPos;
    }
    return;
  }
  
  // NEWLINE (Z)
  if (key == 'Z') {
    if (pendingCommit) commit();
    textBuffer = textBuffer.substring(0, cursorPos) + "\n" + textBuffer.substring(cursorPos);
    cursorPos++;
    return;
  }
  
  // SPACE (#)
  if (key == '#') {
      if (pendingCommit) commit();
      textBuffer = textBuffer.substring(0, cursorPos) + " " + textBuffer.substring(cursorPos);
      cursorPos++;
      return;
  }
  
  // SHIFT (*)
  if (key == '*') { 
    isShifted = !isShifted;
    return;
  }

  // NUMERIC KEYS (0-9)
  if (key >= '0' && key <= '9') {
    if (pendingCommit && key == pendingKey && (now - lastPressTime < MULTITAP_TIMEOUT)) {
      cycleIndex++;
      int mapLen = getUtf8Length(t9Map[key - '0']);
      if (cycleIndex >= mapLen) cycleIndex = 0; 
      lastPressTime = now;
    } else {
      if (pendingCommit) commit();
      pendingKey = key;
      cycleIndex = 0;
      pendingCommit = true;
      lastPressTime = now;
    }
  } else {
      if (pendingCommit) commit();
  }
}

void T9Engine::update() {
  if (pendingCommit && (millis() - lastPressTime > MULTITAP_TIMEOUT)) {
    commit();
  }
}

void T9Engine::commit() {
  String c = getCurrentChar();
  textBuffer = textBuffer.substring(0, cursorPos) + c + textBuffer.substring(cursorPos);
  cursorPos += c.length();
  
  pendingCommit = false;
  cycleIndex = 0;
}

void T9Engine::reset() {
    textBuffer = "";
    cursorPos = 0;
    pendingCommit = false;
}