// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/apps/settings.cpp
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TRUTH_HARDWARE.md Sections 0, 3
// LOG_REF: 2026-03-30
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
    if (inT9Editor && t9MultiTap && t9TapKey != '\0') {
        if (millis() - t9TapTime > MULTITAP_TIMEOUT) {
            t9CommitMultiTap();
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
    t9Shifted = false;
    t9MultiTap = false;
    t9TapKey = '\0';
    t9TapIndex = 0;
    t9TapTime = 0;
}

String SettingsApp::t9GetMultiTapChar() const {
    if (t9TapKey < '0' || t9TapKey > '9') return "";
    const char* map = multiTapMap[t9TapKey - '0'];
    int len = strlen(map);
    if (t9TapIndex >= len) return "";
    char c = map[t9TapIndex];
    if (t9Shifted && c >= 'a' && c <= 'z') c -= 32;
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
    const char* word = t9predict.getSelectedWord();
    if (!word) return;
    String w = word;
    if (t9Shifted && w.length() > 0) {
        w[0] = toupper(w[0]);
    }
    t9Text = t9Text.substring(0, t9Cursor) + w + t9Text.substring(t9Cursor);
    t9Cursor += w.length();
    t9predict.reset();
}

void SettingsApp::t9HandleInput(char key) {
    if (key == KEY_ESC) {
        inT9Editor = false;
        return;
    }

    // ALT: toggle predictive / multi-tap mode
    if (key == KEY_ALT) {
        // Commit any pending input before switching
        if (t9MultiTap && t9TapKey != '\0') t9CommitMultiTap();
        if (!t9MultiTap && t9predict.hasInput()) t9CommitPrediction();
        t9MultiTap = !t9MultiTap;
        t9predict.reset();
        t9TapKey = '\0';
        return;
    }

    // SHIFT: toggle case
    if (key == KEY_SHIFT) {
        t9Shifted = !t9Shifted;
        return;
    }

    // BACKSPACE
    if (key == KEY_BKSP) {
        if (t9MultiTap) {
            if (t9TapKey != '\0') {
                // Cancel pending multi-tap
                t9TapKey = '\0';
                t9TapIndex = 0;
            } else if (t9Cursor > 0) {
                // Delete char before cursor
                t9Text.remove(t9Cursor - 1, 1);
                t9Cursor--;
            }
        } else {
            if (t9predict.hasInput()) {
                t9predict.popDigit();
            } else if (t9Cursor > 0) {
                t9Text.remove(t9Cursor - 1, 1);
                t9Cursor--;
            }
        }
        return;
    }

    // ENTER: confirm current input (no space)
    if (key == KEY_ENTER) {
        if (t9MultiTap && t9TapKey != '\0') {
            t9CommitMultiTap();
        } else if (!t9MultiTap && t9predict.hasInput()) {
            t9CommitPrediction();
        }
        return;
    }

    // TAB: newline
    if (key == KEY_TAB) {
        if (t9MultiTap && t9TapKey != '\0') t9CommitMultiTap();
        if (!t9MultiTap && t9predict.hasInput()) t9CommitPrediction();
        t9Text = t9Text.substring(0, t9Cursor) + "\n" + t9Text.substring(t9Cursor);
        t9Cursor++;
        return;
    }

    // UP/DOWN: cycle candidates (predictive) or do nothing (multi-tap)
    if (key == KEY_UP) {
        if (!t9MultiTap && t9predict.hasInput()) t9predict.prevCandidate();
        return;
    }
    if (key == KEY_DOWN) {
        if (!t9MultiTap && t9predict.hasInput()) t9predict.nextCandidate();
        return;
    }

    // LEFT/RIGHT: confirm + move cursor
    if (key == KEY_LEFT) {
        if (t9MultiTap && t9TapKey != '\0') t9CommitMultiTap();
        if (!t9MultiTap && t9predict.hasInput()) t9CommitPrediction();
        if (t9Cursor > 0) t9Cursor--;
        return;
    }
    if (key == KEY_RIGHT) {
        if (t9MultiTap && t9TapKey != '\0') t9CommitMultiTap();
        if (!t9MultiTap && t9predict.hasInput()) t9CommitPrediction();
        if (t9Cursor < (int)t9Text.length()) t9Cursor++;
        return;
    }

    // DIGIT KEYS (0-9)
    if (key >= '0' && key <= '9') {
        if (t9MultiTap) {
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
            // Predictive mode
            if (key == '0') {
                // 0 = space (commit current word + space)
                if (t9predict.hasInput()) t9CommitPrediction();
                t9Text = t9Text.substring(0, t9Cursor) + " " + t9Text.substring(t9Cursor);
                t9Cursor++;
            } else if (key == '1') {
                // 1 = punctuation (commit word, then multi-tap punctuation)
                if (t9predict.hasInput()) t9CommitPrediction();
                // Quick-insert period (simplest for now)
                t9Text = t9Text.substring(0, t9Cursor) + "." + t9Text.substring(t9Cursor);
                t9Cursor++;
            } else {
                // 2-9: predictive digit
                t9predict.pushDigit(key);
            }
        }
    }
}

void SettingsApp::renderT9Editor() {
    // Header: mode indicator
    const char* modeStr = t9MultiTap ? "ABC" : "T9";
    char headerRight[16];
    snprintf(headerRight, sizeof(headerRight), "%s%s", t9Shifted ? "^" : "", modeStr);
    GUI::drawHeader("T9 EDITOR", headerRight);

    GUI::setFontSmall();

    // --- Text area (lines 13-44, ~4 lines of text) ---
    // Build display text: committed text + pending preview
    String displayText = t9Text;
    int previewInsertPos = t9Cursor;
    String preview = "";

    if (t9MultiTap && t9TapKey != '\0') {
        preview = t9GetMultiTapChar();
    } else if (!t9MultiTap && t9predict.hasInput()) {
        const char* word = t9predict.getSelectedWord();
        if (word) {
            preview = word;
            if (t9Shifted && preview.length() > 0) {
                preview[0] = toupper(preview[0]);
            }
        } else {
            // No match — show digits as-is
            preview = t9predict.getDigits();
        }
    }

    // Insert preview into display text
    if (preview.length() > 0) {
        displayText = displayText.substring(0, previewInsertPos) + preview + displayText.substring(previewInsertPos);
    }

    // Simple multi-line rendering
    int y = 22;
    int maxCharsPerLine = 21; // ~128px / 6px per char
    int textLen = displayText.length();
    int pos = 0;
    int lineCount = 0;
    int cursorScreenX = -1, cursorScreenY = -1;

    while (pos <= textLen && lineCount < 4) {
        // Find line break
        int lineEnd = pos;
        int charCount = 0;
        while (lineEnd < textLen && displayText[lineEnd] != '\n' && charCount < maxCharsPerLine) {
            lineEnd++;
            charCount++;
        }

        // Draw this line
        String line = displayText.substring(pos, lineEnd);
        u8g2.drawStr(1, y, line.c_str());

        // Track cursor position (in the display text, cursor is at previewInsertPos)
        if (t9Cursor >= pos && t9Cursor <= lineEnd && cursorScreenX < 0) {
            int localIdx = t9Cursor - pos;
            String before = displayText.substring(pos, pos + localIdx);
            cursorScreenX = 1 + u8g2.getStrWidth(before.c_str());
            cursorScreenY = y;
        }

        pos = lineEnd;
        if (pos < textLen && displayText[pos] == '\n') pos++; // skip newline
        y += 9;
        lineCount++;
    }

    // Draw cursor
    if (cursorScreenX >= 0) {
        if (preview.length() > 0) {
            // Underline the preview word
            String previewStr = preview;
            int pw = u8g2.getStrWidth(previewStr.c_str());
            u8g2.drawHLine(cursorScreenX, cursorScreenY + 2, pw);
        } else {
            // Blinking bar cursor
            if ((millis() / CURSOR_BLINK_RATE) % 2) {
                u8g2.drawVLine(cursorScreenX, cursorScreenY - 7, 8);
            }
        }
    }

    // --- Separator line ---
    u8g2.drawHLine(0, 54, 128);

    // --- Candidate bar (line 56-63) ---
    GUI::setFontSmall();
    if (!t9MultiTap && t9predict.hasInput()) {
        // Show: "word 1/N  [digits]"
        const char* word = t9predict.getSelectedWord();
        char bar[40];
        if (word) {
            snprintf(bar, sizeof(bar), "%s %d/%d",
                     word, t9predict.getSelectedIndex() + 1,
                     t9predict.getCandidateCount());
        } else {
            snprintf(bar, sizeof(bar), "? [%s]", t9predict.getDigits());
        }
        u8g2.drawStr(1, 62, bar);

        // Show digit sequence on the right
        const char* digs = t9predict.getDigits();
        int dw = u8g2.getStrWidth(digs);
        u8g2.drawStr(128 - dw - 1, 62, digs);
    } else if (t9MultiTap && t9TapKey != '\0') {
        // Show current multi-tap cycle
        const char* map = multiTapMap[t9TapKey - '0'];
        char bar[32];
        snprintf(bar, sizeof(bar), "[%s] %d/%d", map, t9TapIndex + 1, (int)strlen(map));
        u8g2.drawStr(1, 62, bar);
    } else {
        // Show mode hint
        u8g2.drawStr(1, 62, t9MultiTap ? "ALT:T9 0-9:type" : "ALT:ABC 2-9:type 0:sp");
    }
}
