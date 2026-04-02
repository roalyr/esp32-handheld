// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/apps/settings.cpp
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TRUTH_HARDWARE.md Sections 0, 3
// LOG_REF: 2026-04-02
// Description: Settings app — brightness, sleep toggle, key tester, T9 editor, SD card info.

#include "settings.h"
#include "../gui.h"
#include "../config.h"
#include "../clock.h"
#include "../hal.h"

// External sleep control (defined in main.cpp)
extern bool sleepEnabled;

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
        default:        return nullptr;
    }
}

// Multi-tap character maps (same layout as t9_engine.cpp)
static const char* multiTapMap[] = {
    " 0",        // 0
    ".,?!1",     // 1
    "abc2",      // 2
    "def3",      // 3
    "ghi4",      // 4
    "jkl5",      // 5
    "mno6",      // 6
    "pqrs7",     // 7
    "tuv8",      // 8
    "wxyz9"      // 9
};

SettingsApp::SettingsApp() {
    selectedIndex = 0;
    editMode = false;
    inKeyTester = false;
    inT9Editor = false;
    tempBrightness = systemBrightness;
    tempSleepEnabled = SLEEP_ENABLED;
    lastPressedKey = ' ';
    for (int i = 0; i < HISTORY_SIZE; i++) keyHistory[i] = ' ';
    keyHistory[HISTORY_SIZE] = '\0';
    t9EditorReset();
}

void SettingsApp::start() {
    tempBrightness = systemBrightness;
    tempSleepEnabled = sleepEnabled;
    editMode = false;
    inKeyTester = false;
    inT9Editor = false;
    selectedIndex = 0;
}

void SettingsApp::stop() {
    systemBrightness = tempBrightness;
    sleepEnabled = tempSleepEnabled;
    inKeyTester = false;
    inT9Editor = false;
}

bool SettingsApp::isInSubmenu() {
    return inKeyTester || inT9Editor;
}

void SettingsApp::update() {
    // Auto-commit multi-tap after timeout
    if (inT9Editor && t9InputMode == MODE_ABC && t9TapKey != '\0') {
        if (millis() - t9TapTime > MULTITAP_TIMEOUT) {
            t9CommitMultiTap();
        }
    }
    // In T9 mode, key 1 symbol cycling: clear tap state on timeout
    // (char is already inserted inline, just stop the cycle)
    if (inT9Editor && t9InputMode == MODE_T9 && t9TapKey == '1') {
        if (millis() - t9TapTime > MULTITAP_TIMEOUT) {
            t9TapKey = '\0';
            t9TapIndex = 0;
        }
    }
}

void SettingsApp::addToHistory(char c) {
    for (int i = 0; i < HISTORY_SIZE - 1; i++) keyHistory[i] = keyHistory[i + 1];
    keyHistory[HISTORY_SIZE - 1] = c;
}

void SettingsApp::handleInput(char key) {
    if (inKeyTester) {
        if (key == KEY_ESC) {
            inKeyTester = false;
        } else {
            lastPressedKey = key;
            addToHistory(key);
        }
        return;
    }

    if (inT9Editor) {
        t9HandleInput(key);
        return;
    }

    if (!editMode) {
        if (key == KEY_UP) {
            selectedIndex--;
            if (selectedIndex < 0) selectedIndex = SETTING_COUNT - 1;
        }
        if (key == KEY_DOWN) {
            selectedIndex++;
            if (selectedIndex >= SETTING_COUNT) selectedIndex = 0;
        }
        if (key == KEY_ENTER) {
            if (selectedIndex == SETTING_KEY_TESTER) {
                inKeyTester = true;
                lastPressedKey = ' ';
                for (int i = 0; i < HISTORY_SIZE; i++) keyHistory[i] = ' ';
            } else if (selectedIndex == SETTING_T9_EDITOR) {
                inT9Editor = true;
                t9EditorReset();
            } else {
                editMode = true;
            }
        }
    } else {
        if (key == KEY_ENTER) {
            editMode = false;
            if (selectedIndex == SETTING_BRIGHTNESS) {
                systemBrightness = tempBrightness;
                ledcWrite(0, systemBrightness);
            }
            if (selectedIndex == SETTING_SLEEP) {
                sleepEnabled = tempSleepEnabled;
            }
        }
        
        if (selectedIndex == SETTING_BRIGHTNESS) {
            if (key == KEY_RIGHT) {
                tempBrightness += 15;
                if (tempBrightness > 255) tempBrightness = 255;
                ledcWrite(0, tempBrightness);
            }
            if (key == KEY_LEFT) {
                tempBrightness -= 15;
                if (tempBrightness < 10) tempBrightness = 10;
                ledcWrite(0, tempBrightness);
            }
        }
        
        if (selectedIndex == SETTING_SLEEP) {
            if (key == KEY_LEFT || key == KEY_RIGHT) {
                tempSleepEnabled = !tempSleepEnabled;
            }
        }
    }
}

void SettingsApp::renderInfoHeader() {
    // Header: "Settings  vX.X.X  HH:MM"
    char timeBuf[8];
    SystemClock::getTimeString(timeBuf, sizeof(timeBuf));
    
    char headerBuf[40];
    snprintf(headerBuf, sizeof(headerBuf), "v%s %s", FIRMWARE_VERSION, timeBuf);
    
    GUI::drawHeader("Settings", headerBuf);
}

void SettingsApp::renderSettingsList() {
    renderInfoHeader();
    GUI::setFontSmall();
    
    int y = GUI::CONTENT_START_Y;
    
    // Brightness
    {
        bool isSelected = (selectedIndex == SETTING_BRIGHTNESS);
        if (isSelected && !editMode) {
            u8g2.drawBox(0, y - 8, GUI::SCREEN_WIDTH - 4, GUI::LINE_HEIGHT);
            u8g2.setDrawColor(0);
        }
        char buf[32];
        int pct = (tempBrightness * 100) / 255;
        if (editMode && isSelected) {
            snprintf(buf, sizeof(buf), "Backlight: <%d%%>", pct);
        } else {
            snprintf(buf, sizeof(buf), "Backlight: %d%%", pct);
        }
        u8g2.drawStr(2, y, buf);
        if (isSelected && !editMode) u8g2.setDrawColor(1);
    }
    y += GUI::LINE_HEIGHT;
    
    // Sleep
    {
        bool isSelected = (selectedIndex == SETTING_SLEEP);
        if (isSelected && !editMode) {
            u8g2.drawBox(0, y - 8, GUI::SCREEN_WIDTH - 4, GUI::LINE_HEIGHT);
            u8g2.setDrawColor(0);
        }
        char buf[32];
        if (editMode && isSelected) {
            snprintf(buf, sizeof(buf), "Sleep: <%s>", tempSleepEnabled ? "ON" : "OFF");
        } else {
            snprintf(buf, sizeof(buf), "Sleep: %s", tempSleepEnabled ? "ON" : "OFF");
        }
        u8g2.drawStr(2, y, buf);
        if (isSelected && !editMode) u8g2.setDrawColor(1);
    }
    y += GUI::LINE_HEIGHT;
    
    // Key Tester
    {
        bool isSelected = (selectedIndex == SETTING_KEY_TESTER);
        if (isSelected) {
            u8g2.drawBox(0, y - 8, GUI::SCREEN_WIDTH - 4, GUI::LINE_HEIGHT);
            u8g2.setDrawColor(0);
        }
        u8g2.drawStr(2, y, "Key Tester...");
        if (isSelected) u8g2.setDrawColor(1);
    }
    y += GUI::LINE_HEIGHT;

    // T9 Editor
    {
        bool isSelected = (selectedIndex == SETTING_T9_EDITOR);
        if (isSelected) {
            u8g2.drawBox(0, y - 8, GUI::SCREEN_WIDTH - 4, GUI::LINE_HEIGHT);
            u8g2.setDrawColor(0);
        }
        u8g2.drawStr(2, y, "T9 Editor...");
        if (isSelected) u8g2.setDrawColor(1);
    }
    y += GUI::LINE_HEIGHT;
    
    // System info line (read-only, not selectable)
    {
        int freeRamK = ESP.getFreeHeap() / 1024;
        char buf[40];
        if (isSDMounted()) {
            uint64_t totalMB = sdTotalBytes() / (1024*1024);
            uint64_t usedMB = sdUsedBytes() / (1024*1024);
            snprintf(buf, sizeof(buf), "RAM %dk SD %llu/%lluM", freeRamK, (unsigned long long)usedMB, (unsigned long long)totalMB);
        } else {
            snprintf(buf, sizeof(buf), "RAM %dk SD:none", freeRamK);
        }
        u8g2.drawStr(2, y, buf);
    }
}

void SettingsApp::renderKeyTester() {
    GUI::drawHeader("KEY TESTER");
    GUI::setFontSmall();
    
    // Last Key Display
    u8g2.drawUTF8(0, 24, "LAST:");
    if (lastPressedKey != ' ') {
        const char* name = getKeyName(lastPressedKey);
        if (name) {
            u8g2.setFont(u8g2_font_ncenB14_tr);
            u8g2.drawStr(35, 38, name);
        } else {
            u8g2.setFont(u8g2_font_inr24_t_cyrillic);
            char keyStr[2] = {lastPressedKey, '\0'};
            u8g2.drawUTF8(50, 42, keyStr);
        }
    }
    
    // Currently Held Keys
    GUI::setFontSmall();
    u8g2.drawUTF8(0, 55, "HELD:");
    int xPos = 28;
    for (int i = 0; i < activeKeyCount; i++) {
        const char* name = getKeyName(activeKeys[i]);
        if (name) {
            u8g2.drawStr(xPos, 55, name);
            xPos += u8g2.getStrWidth(name) + 3;
        } else {
            char buf[4] = {'[', activeKeys[i], ']', '\0'};
            u8g2.drawStr(xPos, 55, buf);
            xPos += 15;
        }
    }
    
}

void SettingsApp::render() {
    if (inKeyTester) {
        renderKeyTester();
    } else if (inT9Editor) {
        renderT9Editor();
    } else {
        renderSettingsList();
    }
}

// --------------------------------------------------------------------------
// T9 EDITOR SUBMENU
// --------------------------------------------------------------------------

void SettingsApp::t9EditorReset() {
    t9predict.reset();
    t9Text = "";
    t9Cursor = 0;
    t9ShiftMode = 0;        // 0=lower, 1=firstCap, 2=allCaps
    t9InputMode = MODE_T9;
    t9TapKey = '\0';
    t9TapIndex = 0;
    t9TapTime = 0;
    t9ScrollOffset = 0;
    t9CursorMoveTime = 0;
}

String SettingsApp::t9GetMultiTapChar() const {
    if (t9TapKey < '0' || t9TapKey > '9') return "";
    const char* map = multiTapMap[t9TapKey - '0'];
    int len = strlen(map);
    if (t9TapIndex >= len) return "";
    char c = map[t9TapIndex];
    if (t9ShiftMode >= 1 && c >= 'a' && c <= 'z') c -= 32;
    return String(c);
}

void SettingsApp::t9CommitMultiTap() {
    if (t9TapKey == '\0') return;
    String c = t9GetMultiTapChar();
    t9Text = t9Text.substring(0, t9Cursor) + c + t9Text.substring(t9Cursor);
    t9Cursor += c.length();
    t9TapKey = '\0';
    t9TapIndex = 0;
}

void SettingsApp::t9CommitPrediction() {
    // Use prefix candidate: show the best word truncated to digitCount chars
    int dc = t9predict.getDigitCount();
    const char* word = t9predict.getSelectedPrefixWord();
    if (!word) {
        // No prefix match — commit raw digits as literal text
        const char* digs = t9predict.getDigits();
        if (digs && dc > 0) {
            String d = digs;
            t9Text = t9Text.substring(0, t9Cursor) + d + t9Text.substring(t9Cursor);
            t9Cursor += d.length();
        }
        t9predict.reset();
        return;
    }
    String w = word;
    // Truncate to digitCount characters (letters priority preview)
    if ((int)w.length() > dc) w = w.substring(0, dc);
    // Apply shift
    if (t9ShiftMode == 1 && w.length() > 0) {
        w[0] = toupper(w[0]);
    } else if (t9ShiftMode == 2) {
        for (int i = 0; i < (int)w.length(); i++) {
            if (w[i] >= 'a' && w[i] <= 'z') w[i] -= 32;
        }
    }
    t9Text = t9Text.substring(0, t9Cursor) + w + t9Text.substring(t9Cursor);
    t9Cursor += w.length();
    t9predict.reset();
    // Auto-reset shift to lowercase after firstCap commit
    if (t9ShiftMode == 1) t9ShiftMode = 0;
}

void SettingsApp::t9MoveCursorVertically(int dir) {
    // Find current line start and column
    int lineStart = 0;
    for (int i = t9Cursor - 1; i >= 0; i--) {
        if (t9Text[i] == '\n') { lineStart = i + 1; break; }
    }
    int col = t9Cursor - lineStart;

    if (dir < 0) {
        // UP: move to previous line
        if (lineStart == 0) return; // already on first line
        int prevLineEnd = lineStart - 1; // points to \n
        int prevLineStart = 0;
        for (int i = prevLineEnd - 1; i >= 0; i--) {
            if (t9Text[i] == '\n') { prevLineStart = i + 1; break; }
        }
        int prevLineLen = prevLineEnd - prevLineStart;
        t9Cursor = prevLineStart + min(col, prevLineLen);
    } else {
        // DOWN: move to next line
        int nextLineStart = -1;
        for (int i = t9Cursor; i < (int)t9Text.length(); i++) {
            if (t9Text[i] == '\n') { nextLineStart = i + 1; break; }
        }
        if (nextLineStart < 0) return; // already on last line
        int nextLineEnd = t9Text.length();
        for (int i = nextLineStart; i < (int)t9Text.length(); i++) {
            if (t9Text[i] == '\n') { nextLineEnd = i; break; }
        }
        int nextLineLen = nextLineEnd - nextLineStart;
        t9Cursor = nextLineStart + min(col, nextLineLen);
    }
}

void SettingsApp::t9HandleInput(char key) {
    if (key == KEY_ESC) {
        inT9Editor = false;
        return;
    }

    // ALT: cycle T9 → ABC → 123
    if (key == KEY_ALT) {
        // Commit any pending input before switching
        if (t9InputMode == MODE_ABC && t9TapKey != '\0') t9CommitMultiTap();
        if (t9InputMode == MODE_T9 && t9predict.hasInput()) t9CommitPrediction();
        // Cycle mode
        if (t9InputMode == MODE_T9)       t9InputMode = MODE_ABC;
        else if (t9InputMode == MODE_ABC)  t9InputMode = MODE_123;
        else                               t9InputMode = MODE_T9;
        t9predict.reset();
        t9TapKey = '\0';
        return;
    }

    // SHIFT: cycle lower → firstCap → allCaps
    if (key == KEY_SHIFT) {
        t9ShiftMode = (t9ShiftMode + 1) % 3;
        return;
    }

    // BACKSPACE
    if (key == KEY_BKSP) {
        t9CursorMoveTime = millis();
        if (t9InputMode == MODE_ABC) {
            if (t9TapKey != '\0') {
                t9TapKey = '\0';
                t9TapIndex = 0;
            } else if (t9Cursor > 0) {
                t9Text.remove(t9Cursor - 1, 1);
                t9Cursor--;
            }
        } else if (t9InputMode == MODE_T9) {
            if (t9predict.hasInput()) {
                t9predict.popDigit();
            } else if (t9Cursor > 0) {
                t9Text.remove(t9Cursor - 1, 1);
                t9Cursor--;
            }
        } else {
            // MODE_123
            if (t9Cursor > 0) {
                t9Text.remove(t9Cursor - 1, 1);
                t9Cursor--;
            }
        }
        return;
    }

    // ENTER: commit current input; if nothing pending, insert newline
    if (key == KEY_ENTER) {
        t9CursorMoveTime = millis();
        if (t9InputMode == MODE_ABC && t9TapKey != '\0') {
            t9CommitMultiTap();
        } else if (t9InputMode == MODE_T9 && t9predict.hasInput()) {
            t9CommitPrediction();
        } else {
            // No pending input — insert newline
            t9Text = t9Text.substring(0, t9Cursor) + "\n" + t9Text.substring(t9Cursor);
            t9Cursor++;
        }
        return;
    }

    // TAB: always insert newline (commit pending first)
    if (key == KEY_TAB) {
        t9CursorMoveTime = millis();
        if (t9InputMode == MODE_ABC && t9TapKey != '\0') t9CommitMultiTap();
        if (t9InputMode == MODE_T9 && t9predict.hasInput()) t9CommitPrediction();
        t9Text = t9Text.substring(0, t9Cursor) + "\n" + t9Text.substring(t9Cursor);
        t9Cursor++;
        return;
    }

    // UP/DOWN: cycle candidates when T9 has input, otherwise move cursor vertically
    if (key == KEY_UP) {
        t9CursorMoveTime = millis();
        if (t9InputMode == MODE_T9 && t9predict.hasInput()) {
            t9predict.prevPrefixCandidate();
        } else {
            // Commit pending, then move cursor up
            if (t9InputMode == MODE_ABC && t9TapKey != '\0') t9CommitMultiTap();
            if (t9InputMode == MODE_T9 && t9predict.hasInput()) t9CommitPrediction();
            t9MoveCursorVertically(-1);
        }
        return;
    }
    if (key == KEY_DOWN) {
        t9CursorMoveTime = millis();
        if (t9InputMode == MODE_T9 && t9predict.hasInput()) {
            t9predict.nextPrefixCandidate();
        } else {
            if (t9InputMode == MODE_ABC && t9TapKey != '\0') t9CommitMultiTap();
            if (t9InputMode == MODE_T9 && t9predict.hasInput()) t9CommitPrediction();
            t9MoveCursorVertically(1);
        }
        return;
    }

    // LEFT/RIGHT: commit pending + move cursor horizontally (TASK_4)
    if (key == KEY_LEFT) {
        t9CursorMoveTime = millis();
        if (t9InputMode == MODE_ABC && t9TapKey != '\0') t9CommitMultiTap();
        if (t9InputMode == MODE_T9 && t9predict.hasInput()) t9CommitPrediction();
        if (t9Cursor > 0) t9Cursor--;
        return;
    }
    if (key == KEY_RIGHT) {
        t9CursorMoveTime = millis();
        if (t9InputMode == MODE_ABC && t9TapKey != '\0') t9CommitMultiTap();
        if (t9InputMode == MODE_T9 && t9predict.hasInput()) t9CommitPrediction();
        if (t9Cursor < (int)t9Text.length()) t9Cursor++;
        return;
    }

    // DIGIT KEYS (0-9)
    if (key >= '0' && key <= '9') {
        // Long-press in T9/ABC mode: replace the pending letter with the digit
        if ((t9InputMode == MODE_T9 || t9InputMode == MODE_ABC) && isKeyHeld(key)) {
            if (t9InputMode == MODE_ABC && t9TapKey != '\0') {
                // Cancel the pending multi-tap character (don't commit the letter)
                t9TapKey = '\0';
                t9TapIndex = 0;
            }
            if (t9InputMode == MODE_T9) {
                // For key 1 symbol cycling, undo the last inserted symbol
                if (t9TapKey == '1' && t9Cursor > 0) {
                    t9Text.remove(t9Cursor - 1, 1);
                    t9Cursor--;
                    t9TapKey = '\0';
                    t9TapIndex = 0;
                }
                // Pop the digit that the initial short press added to prediction,
                // so the digit replaces the letter it contributed.
                if (t9predict.hasInput()) {
                    t9predict.popDigit();
                    if (t9predict.hasInput()) t9CommitPrediction();
                }
            }
            t9Text = t9Text.substring(0, t9Cursor) + String(key) + t9Text.substring(t9Cursor);
            t9Cursor++;
            t9CursorMoveTime = millis();
            return;
        }

        // In 123 mode, long-press just means the key is held — digit already
        // inserted on short press, so ignore the long-press event.
        if (t9InputMode == MODE_123 && isKeyHeld(key)) {
            return;
        }

        if (t9InputMode == MODE_123) {
            // Direct digit insertion
            t9Text = t9Text.substring(0, t9Cursor) + String(key) + t9Text.substring(t9Cursor);
            t9Cursor++;
        } else if (t9InputMode == MODE_ABC) {
            // Multi-tap mode
            unsigned long now = millis();
            if (key == t9TapKey && (now - t9TapTime < MULTITAP_TIMEOUT)) {
                const char* map = multiTapMap[key - '0'];
                t9TapIndex = (t9TapIndex + 1) % strlen(map);
            } else {
                if (t9TapKey != '\0') t9CommitMultiTap();
                t9TapKey = key;
                t9TapIndex = 0;
            }
            t9TapTime = now;
        } else {
            // MODE_T9: predictive mode
            if (key == '0') {
                // 0 = space (commit current word + space)
                if (t9predict.hasInput()) t9CommitPrediction();
                t9Text = t9Text.substring(0, t9Cursor) + " " + t9Text.substring(t9Cursor);
                t9Cursor++;
            } else if (key == '1') {
                // 1 = symbol cycling (same as ABC mode multi-tap on key 1)
                if (t9predict.hasInput()) t9CommitPrediction();
                unsigned long now = millis();
                if (t9TapKey == '1' && (now - t9TapTime < MULTITAP_TIMEOUT)) {
                    // Cycle through symbols
                    // Remove previously inserted multi-tap char
                    if (t9Cursor > 0) {
                        t9Text.remove(t9Cursor - 1, 1);
                        t9Cursor--;
                    }
                    const char* map = multiTapMap[1]; // ".,?!1"
                    t9TapIndex = (t9TapIndex + 1) % strlen(map);
                } else {
                    t9TapKey = '1';
                    t9TapIndex = 0;
                }
                t9TapTime = now;
                // Insert current symbol
                char c = multiTapMap[1][t9TapIndex];
                t9Text = t9Text.substring(0, t9Cursor) + String(c) + t9Text.substring(t9Cursor);
                t9Cursor++;
            } else {
                // 2-9: predictive digit
                t9predict.pushDigit(key);
            }
        }
    }
}

void SettingsApp::renderT9Editor() {
    // Header: mode indicator + shift state
    const char* modeStr = (t9InputMode == MODE_T9) ? "T9" :
                          (t9InputMode == MODE_ABC) ? "ABC" : "123";
    const char* shiftStr = (t9ShiftMode == 2) ? "^^" :
                           (t9ShiftMode == 1) ? "^" : "";
    char headerRight[16];
    snprintf(headerRight, sizeof(headerRight), "%s%s", shiftStr, modeStr);
    GUI::drawHeader("T9 EDITOR", headerRight);

    GUI::setFontSmall();

    // --- Text area (lines 13-44, ~4 lines of text) ---
    String displayText = t9Text;
    int previewInsertPos = t9Cursor;
    String preview = "";

    if (t9InputMode == MODE_ABC && t9TapKey != '\0') {
        preview = t9GetMultiTapChar();
    } else if (t9InputMode == MODE_T9 && t9predict.hasInput()) {
        int dc = t9predict.getDigitCount();
        const char* word = t9predict.getSelectedPrefixWord();
        if (word) {
            preview = word;
            if ((int)preview.length() > dc) preview = preview.substring(0, dc);
            // Apply shift to preview
            if (t9ShiftMode == 1 && preview.length() > 0) {
                preview[0] = toupper(preview[0]);
            } else if (t9ShiftMode == 2) {
                for (int i = 0; i < (int)preview.length(); i++) {
                    if (preview[i] >= 'a' && preview[i] <= 'z') preview[i] -= 32;
                }
            }
        } else {
            preview = t9predict.getDigits();
        }
    }

    // Insert preview into display text
    if (preview.length() > 0) {
        displayText = displayText.substring(0, previewInsertPos) + preview + displayText.substring(previewInsertPos);
    }

    // Multi-line rendering with vertical scroll
    int maxVisibleLines = 4;
    int lineHeight = 9;
    int maxCharsPerLine = 21;
    int textLen = displayText.length();

    // Break text into lines
    int lineStarts[64];
    int lineEnds[64];
    int totalLines = 0;
    int pos = 0;
    while (pos <= textLen && totalLines < 64) {
        lineStarts[totalLines] = pos;
        int lineEnd = pos;
        int charCount = 0;
        while (lineEnd < textLen && displayText[lineEnd] != '\n' && charCount < maxCharsPerLine) {
            lineEnd++;
            charCount++;
        }
        lineEnds[totalLines] = lineEnd;
        totalLines++;
        pos = lineEnd;
        if (pos < textLen && displayText[pos] == '\n') pos++;
        if (pos == textLen && lineEnd == textLen && displayText[textLen - 1] != '\n') break;
    }
    if (totalLines == 0) totalLines = 1; // at least one empty line

    // Find which line the cursor is on
    int cursorLine = 0;
    for (int i = 0; i < totalLines; i++) {
        if (t9Cursor >= lineStarts[i] && t9Cursor <= lineEnds[i]) {
            cursorLine = i;
            break;
        }
    }

    // Auto-scroll to keep cursor visible
    if (cursorLine < t9ScrollOffset) t9ScrollOffset = cursorLine;
    if (cursorLine >= t9ScrollOffset + maxVisibleLines) t9ScrollOffset = cursorLine - maxVisibleLines + 1;

    // Render visible lines
    int y = 22;
    int cursorScreenX = -1, cursorScreenY = -1;
    for (int i = 0; i < maxVisibleLines; i++) {
        int li = t9ScrollOffset + i;
        if (li >= totalLines) break;
        String line = displayText.substring(lineStarts[li], lineEnds[li]);
        u8g2.drawStr(1, y, line.c_str());

        // Track cursor position
        if (li == cursorLine && cursorScreenX < 0) {
            int localIdx = t9Cursor - lineStarts[li];
            String before = displayText.substring(lineStarts[li], lineStarts[li] + localIdx);
            cursorScreenX = 1 + u8g2.getStrWidth(before.c_str());
            cursorScreenY = y;
        }
        y += lineHeight;
    }

    // Draw cursor
    if (cursorScreenX >= 0) {
        if (preview.length() > 0) {
            int pw = u8g2.getStrWidth(preview.c_str());
            u8g2.drawHLine(cursorScreenX, cursorScreenY + 2, pw);
        } else {
            // Show solid cursor briefly after movement, then blink
            bool recentMove = (millis() - t9CursorMoveTime < CURSOR_BLINK_RATE);
            if (recentMove || (millis() / CURSOR_BLINK_RATE) % 2) {
                u8g2.drawVLine(cursorScreenX, cursorScreenY - 7, 8);
            }
        }
    }

    // --- Separator line ---
    u8g2.drawHLine(0, 54, 128);

    // --- Candidate bar (line 56-63) ---
    GUI::setFontSmall();
    if (t9InputMode == MODE_T9 && t9predict.hasInput()) {
        const char* word = t9predict.getSelectedPrefixWord();
        char bar[40];
        if (word) {
            snprintf(bar, sizeof(bar), "%s %d/%d",
                     word, t9predict.getSelectedPrefixIndex() + 1,
                     t9predict.getPrefixCandidateCount());
        } else {
            snprintf(bar, sizeof(bar), "? [%s]", t9predict.getDigits());
        }
        u8g2.drawStr(1, 62, bar);
    } else if (t9InputMode == MODE_T9 && t9TapKey == '1') {
        // T9 mode key 1 symbol cycling
        const char* map = multiTapMap[1];
        char bar[32];
        snprintf(bar, sizeof(bar), "[%s] %d/%d", map, t9TapIndex + 1, (int)strlen(map));
        u8g2.drawStr(1, 62, bar);
    } else if (t9InputMode == MODE_ABC && t9TapKey != '\0') {
        const char* map = multiTapMap[t9TapKey - '0'];
        char bar[32];
        snprintf(bar, sizeof(bar), "[%s] %d/%d", map, t9TapIndex + 1, (int)strlen(map));
        u8g2.drawStr(1, 62, bar);
    } else {
        // Mode hint
        const char* hint = (t9InputMode == MODE_T9)  ? "ALT:ABC 2-9:T9 0:sp" :
                           (t9InputMode == MODE_ABC) ? "ALT:123 0-9:tap" :
                                                       "ALT:T9 0-9:digits";
        u8g2.drawStr(1, 62, hint);
    }
}
