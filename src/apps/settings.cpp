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
    inLcdTest = false;
    lcdTestStep = 0;
    inSdTest = false;
    sdTestRan = false;
    sdTestLineCount = 0;
    t9EditorReset();
}

void SettingsApp::start() {
    tempBrightness = systemBrightness;
    tempSleepEnabled = sleepEnabled;
    editMode = false;
    inKeyTester = false;
    inT9Editor = false;
    inLcdTest = false;
    lcdTestStep = 0;
    inSdTest = false;
    sdTestRan = false;
    selectedIndex = 0;
}

void SettingsApp::stop() {
    systemBrightness = tempBrightness;
    sleepEnabled = tempSleepEnabled;
    inKeyTester = false;
    inT9Editor = false;
    inLcdTest = false;
    inSdTest = false;
}

bool SettingsApp::isInSubmenu() {
    return inKeyTester || inT9Editor || inLcdTest || inSdTest;
}

void SettingsApp::update() {
    // Auto-commit multi-tap after timeout (ABC mode or T9 fallback)
    if (inT9Editor && (t9InputMode == MODE_ABC || t9Fallback) && t9TapKey != '\0') {
        if (millis() - t9TapTime > MULTITAP_TIMEOUT) {
            // Check if the character about to be committed is a separator
            bool isSeparator = false;
            if (t9Fallback) {
                String c = t9GetMultiTapChar();
                if (c == "." || c == "?" || c == "!" || c == ",") isSeparator = true;
            }
            t9CommitMultiTap();
            if (isSeparator) t9Fallback = false;
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

    if (inLcdTest) {
        if (key == KEY_ESC) {
            inLcdTest = false;
        } else if (key == KEY_ENTER || key == KEY_RIGHT || key == KEY_DOWN) {
            lcdTestStep++;
            if (lcdTestStep >= LCD_TEST_COUNT) {
                inLcdTest = false;
            }
        } else if (key == KEY_LEFT || key == KEY_UP) {
            if (lcdTestStep > 0) lcdTestStep--;
        }
        return;
    }

    if (inSdTest) {
        if (key == KEY_ESC || key == KEY_ENTER) {
            inSdTest = false;
        } else if (key == KEY_RIGHT || key == KEY_DOWN) {
            // Re-run the test
            sdTestRan = false;
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
            } else if (selectedIndex == SETTING_LCD_TEST) {
                inLcdTest = true;
                lcdTestStep = 0;
            } else if (selectedIndex == SETTING_SD_TEST) {
                inSdTest = true;
                sdTestRan = false;
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
    
    // Total items: SETTING_COUNT selectable + 1 info line
    const int totalItems = SETTING_COUNT + 1;  // +1 for info line
    const int maxVisible = 4;  // Content area fits 4 lines
    
    // Calculate scroll offset to keep selection visible
    int scrollOff = 0;
    if (selectedIndex >= maxVisible) {
        scrollOff = selectedIndex - maxVisible + 1;
    }
    // Clamp so we can always see totalItems
    if (scrollOff > totalItems - maxVisible) {
        scrollOff = totalItems - maxVisible;
    }
    if (scrollOff < 0) scrollOff = 0;

    for (int vis = 0; vis < maxVisible && (scrollOff + vis) < totalItems; vis++) {
        int idx = scrollOff + vis;
        int y = GUI::CONTENT_START_Y + vis * GUI::LINE_HEIGHT;
        bool isSelected = (idx == selectedIndex);
        char buf[40];

        // Info line is at index SETTING_COUNT (not selectable)
        if (idx == SETTING_COUNT) {
            int freeRamK = ESP.getFreeHeap() / 1024;
            if (isSDMounted()) {
                uint64_t totalMB = sdTotalBytes() / (1024*1024);
                uint64_t usedMB = sdUsedBytes() / (1024*1024);
                snprintf(buf, sizeof(buf), "RAM %dk SD %llu/%lluM", freeRamK, (unsigned long long)usedMB, (unsigned long long)totalMB);
            } else {
                snprintf(buf, sizeof(buf), "RAM %dk SD:none", freeRamK);
            }
            u8g2.drawStr(2, y, buf);
            continue;
        }

        // Draw selection highlight for non-edit items
        bool highlightSel = isSelected && !editMode;
        // For edit-mode items, highlight only when not editing
        if (highlightSel) {
            u8g2.drawBox(0, y - 8, GUI::SCREEN_WIDTH - 4, GUI::LINE_HEIGHT);
            u8g2.setDrawColor(0);
        }

        switch ((SettingItem)idx) {
        case SETTING_BRIGHTNESS: {
            int pct = (tempBrightness * 100) / 255;
            if (editMode && isSelected)
                snprintf(buf, sizeof(buf), "Backlight: <%d%%>", pct);
            else
                snprintf(buf, sizeof(buf), "Backlight: %d%%", pct);
            break;
        }
        case SETTING_SLEEP:
            if (editMode && isSelected)
                snprintf(buf, sizeof(buf), "Sleep: <%s>", tempSleepEnabled ? "ON" : "OFF");
            else
                snprintf(buf, sizeof(buf), "Sleep: %s", tempSleepEnabled ? "ON" : "OFF");
            break;
        case SETTING_KEY_TESTER:
            snprintf(buf, sizeof(buf), "Key Tester...");
            break;
        case SETTING_T9_EDITOR:
            snprintf(buf, sizeof(buf), "T9 Editor...");
            break;
        case SETTING_LCD_TEST:
            snprintf(buf, sizeof(buf), "LCD Test...");
            break;
        case SETTING_SD_TEST:
            snprintf(buf, sizeof(buf), "SD Pin Test...");
            break;
        default:
            buf[0] = '\0';
        }

        u8g2.drawStr(2, y, buf);
        if (highlightSel) u8g2.setDrawColor(1);
    }
    
    // Scroll indicators
    if (scrollOff > 0) {
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.drawStr(120, GUI::CONTENT_START_Y - 2, "^");
    }
    if (scrollOff + maxVisible < totalItems) {
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.drawStr(120, GUI::CONTENT_START_Y + maxVisible * GUI::LINE_HEIGHT - 4, "v");
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
    } else if (inLcdTest) {
        renderLcdTest();
    } else if (inSdTest) {
        renderSdTest();
    } else {
        renderSettingsList();
    }
}

// --------------------------------------------------------------------------
// LCD TEST SUBMENU
// --------------------------------------------------------------------------

void SettingsApp::renderLcdTest() {
    char stepBuf[16];
    snprintf(stepBuf, sizeof(stepBuf), "%d/%d", lcdTestStep + 1, LCD_TEST_COUNT);

    switch (lcdTestStep) {

    case 0: {
        // All pixels ON (full fill)
        u8g2.drawBox(0, 0, 128, 64);
        u8g2.setDrawColor(0);
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(2, 8, "ALL PIXELS ON");
        u8g2.drawStr(108, 8, stepBuf);
        u8g2.drawStr(36, 63, "Enter:Next");
        u8g2.setDrawColor(1);
        break;
    }

    case 1: {
        // All horizontal lines
        for (int y = 0; y < 64; y++) {
            u8g2.drawHLine(0, y, 128);
        }
        u8g2.setDrawColor(0);
        u8g2.drawBox(0, 0, 128, 9);
        u8g2.drawBox(0, 56, 128, 8);
        u8g2.setDrawColor(1);
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawBox(0, 0, 128, 9);
        u8g2.setDrawColor(0);
        u8g2.drawStr(2, 8, "ALL HLINES");
        u8g2.drawStr(108, 8, stepBuf);
        u8g2.setDrawColor(1);
        u8g2.drawStr(36, 63, "Enter:Next");
        break;
    }

    case 2: {
        // Even rows only
        for (int y = 0; y < 64; y += 2) {
            u8g2.drawHLine(0, y, 128);
        }
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawBox(0, 0, 128, 9);
        u8g2.setDrawColor(0);
        u8g2.drawStr(2, 8, "EVEN ROWS");
        u8g2.drawStr(108, 8, stepBuf);
        u8g2.setDrawColor(1);
        u8g2.drawStr(36, 63, "Enter:Next");
        break;
    }

    case 3: {
        // Odd rows only
        for (int y = 1; y < 64; y += 2) {
            u8g2.drawHLine(0, y, 128);
        }
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawBox(0, 0, 128, 9);
        u8g2.setDrawColor(0);
        u8g2.drawStr(2, 8, "ODD ROWS");
        u8g2.drawStr(108, 8, stepBuf);
        u8g2.setDrawColor(1);
        u8g2.drawStr(36, 63, "Enter:Next");
        break;
    }

    case 4: {
        // 8-pixel bands with row numbers
        u8g2.setFont(u8g2_font_4x6_tf);
        for (int band = 0; band < 8; band++) {
            int y0 = band * 8;
            if (band % 2 == 0) {
                u8g2.drawBox(0, y0, 128, 8);
                u8g2.setDrawColor(0);
                char buf[8];
                snprintf(buf, sizeof(buf), "%d-%d", y0, y0 + 7);
                u8g2.drawStr(2, y0 + 7, buf);
                u8g2.setDrawColor(1);
            } else {
                char buf[8];
                snprintf(buf, sizeof(buf), "%d-%d", y0, y0 + 7);
                u8g2.drawStr(2, y0 + 7, buf);
            }
        }
        break;
    }

    case 5: {
        // 1px checkerboard
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 128; x++) {
                if ((x + y) % 2 == 0) u8g2.drawPixel(x, y);
            }
        }
        break;
    }

    case 6: {
        // 8px grid
        for (int y = 0; y < 64; y += 8) u8g2.drawHLine(0, y, 128);
        for (int x = 0; x < 128; x += 8) u8g2.drawVLine(x, 0, 64);
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawBox(0, 0, 128, 9);
        u8g2.setDrawColor(0);
        u8g2.drawStr(2, 8, "8px GRID");
        u8g2.drawStr(108, 8, stepBuf);
        u8g2.setDrawColor(1);
        u8g2.drawStr(28, 63, "Enter:Exit");
        break;
    }

    }  // switch
}

// --------------------------------------------------------------------------
// SD PIN TEST SUBMENU
// --------------------------------------------------------------------------

void SettingsApp::runSdPinDiagnostic() {
    sdTestLineCount = 0;
    auto addLine = [&](const char* fmt, ...) {
        if (sdTestLineCount >= SD_TEST_LINES) return;
        va_list args;
        va_start(args, fmt);
        vsnprintf(sdTestResults[sdTestLineCount], sizeof(sdTestResults[0]), fmt, args);
        va_end(args);
        Serial.printf("[SD_DIAG] %s\n", sdTestResults[sdTestLineCount]);
        sdTestLineCount++;
    };

    addLine("Pin test: MOSI=%d SCK=%d", PIN_SPI_MOSI, PIN_SPI_SCLK);
    addLine("  MISO=%d CS=%d", PIN_SPI_MISO, PIN_SD_CS);

    // --- Step 1: GPIO loopback test (MOSI/MISO/SCK/CS as plain GPIO) ---
    // First, release SPI bus and LCD pins
    u8g2.begin();  // Will be re-inited after test

    // Test CS pin — should be writable and readable
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);
    bool csHigh = digitalRead(PIN_SD_CS) == HIGH;
    digitalWrite(PIN_SD_CS, LOW);
    bool csLow = digitalRead(PIN_SD_CS) == LOW;
    addLine("CS(39): H=%d L=%d %s", csHigh, csLow,
            (csHigh && csLow) ? "OK" : "FAIL");

    // Test MISO pin — should be pullable HIGH/LOW when no card
    // With card inserted but no SPI, MISO should be floating/high-Z
    pinMode(PIN_SPI_MISO, INPUT_PULLUP);
    delay(1);
    bool misoPU = digitalRead(PIN_SPI_MISO);
    pinMode(PIN_SPI_MISO, INPUT_PULLDOWN);
    delay(1);
    bool misoPD = digitalRead(PIN_SPI_MISO);
    // If both follow pullup/down: no card or card not driving = expected
    // If stuck HIGH: card may be driving MISO (powered)
    // If stuck LOW: possible short to GND
    const char* misoStatus;
    if (misoPU && !misoPD) misoStatus = "Float OK";
    else if (misoPU && misoPD) misoStatus = "STUCK HI";
    else if (!misoPU && !misoPD) misoStatus = "STUCK LO";
    else misoStatus = "Weak pull";
    addLine("MISO(37): PU=%d PD=%d %s", misoPU, misoPD, misoStatus);

    // Test SCK as output
    pinMode(PIN_SPI_SCLK, OUTPUT);
    digitalWrite(PIN_SPI_SCLK, HIGH);
    bool sckH = digitalRead(PIN_SPI_SCLK);
    digitalWrite(PIN_SPI_SCLK, LOW);
    bool sckL = digitalRead(PIN_SPI_SCLK);
    addLine("SCK(36): H=%d L=%d %s", sckH, !sckL,
            (sckH && !sckL) ? "OK" : "FAIL");

    // Test MOSI as output
    pinMode(PIN_SPI_MOSI, OUTPUT);
    digitalWrite(PIN_SPI_MOSI, HIGH);
    bool mosiH = digitalRead(PIN_SPI_MOSI);
    digitalWrite(PIN_SPI_MOSI, LOW);
    bool mosiL = digitalRead(PIN_SPI_MOSI);
    addLine("MOSI(35): H=%d L=%d %s", mosiH, !mosiL,
            (mosiH && !mosiL) ? "OK" : "FAIL");

    // --- Step 2: Try SD CMD0 (GO_IDLE_STATE) via manual bit-bang ---
    // Send 74+ clock pulses with CS HIGH (SD card init requirement)
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);
    pinMode(PIN_SPI_MOSI, OUTPUT);
    digitalWrite(PIN_SPI_MOSI, HIGH);
    pinMode(PIN_SPI_SCLK, OUTPUT);

    for (int i = 0; i < 80; i++) {
        digitalWrite(PIN_SPI_SCLK, HIGH);
        delayMicroseconds(5);
        digitalWrite(PIN_SPI_SCLK, LOW);
        delayMicroseconds(5);
    }

    // Select card
    digitalWrite(PIN_SD_CS, LOW);
    delayMicroseconds(10);

    // Send CMD0: 0x40 0x00 0x00 0x00 0x00 0x95
    uint8_t cmd0[] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x95};
    pinMode(PIN_SPI_MISO, INPUT_PULLUP);

    for (int b = 0; b < 6; b++) {
        uint8_t byte = cmd0[b];
        for (int bit = 7; bit >= 0; bit--) {
            digitalWrite(PIN_SPI_MOSI, (byte >> bit) & 1);
            delayMicroseconds(3);
            digitalWrite(PIN_SPI_SCLK, HIGH);
            delayMicroseconds(3);
            digitalWrite(PIN_SPI_SCLK, LOW);
        }
    }

    // Read response — wait up to 16 bytes for non-0xFF
    uint8_t resp = 0xFF;
    for (int attempt = 0; attempt < 16; attempt++) {
        uint8_t rxByte = 0;
        for (int bit = 7; bit >= 0; bit--) {
            digitalWrite(PIN_SPI_SCLK, HIGH);
            delayMicroseconds(3);
            if (digitalRead(PIN_SPI_MISO)) rxByte |= (1 << bit);
            digitalWrite(PIN_SPI_SCLK, LOW);
            delayMicroseconds(3);
        }
        if (rxByte != 0xFF) {
            resp = rxByte;
            break;
        }
    }

    digitalWrite(PIN_SD_CS, HIGH);

    if (resp == 0x01) {
        addLine("CMD0: 0x%02X CARD OK", resp);
    } else if (resp == 0xFF) {
        addLine("CMD0: 0xFF NO CARD");
    } else {
        addLine("CMD0: 0x%02X (unexpected)", resp);
    }

    // --- Step 3: Try SdFat mount ---
    bool sdOk = mountSD();
    addLine("SdFat mount: %s", sdOk ? "OK" : "FAIL");

    // Restore LCD (mountSD already calls sdEndSession)
    // Re-init display after pin tests
    u8g2.begin();
    u8g2.setContrast(systemContrast);
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);

    sdTestRan = true;
}

void SettingsApp::renderSdTest() {
    if (!sdTestRan) {
        runSdPinDiagnostic();
    }

    GUI::drawHeader("SD PIN TEST");
    u8g2.setFont(u8g2_font_4x6_tf);

    int y = GUI::HEADER_HEIGHT + 8;
    for (int i = 0; i < sdTestLineCount && y < 58; i++) {
        u8g2.drawStr(1, y, sdTestResults[i]);
        y += 7;
    }

    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(1, 63, "Esc:Back  >:Retest");
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
    t9Fallback = false;
    t9FallbackStart = 0;
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
        if ((t9InputMode == MODE_ABC || t9Fallback) && t9TapKey != '\0') t9CommitMultiTap();
        if (t9InputMode == MODE_T9 && t9predict.hasInput()) t9CommitPrediction();
        t9Fallback = false;
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
        if (t9InputMode == MODE_ABC || (t9InputMode == MODE_T9 && t9Fallback)) {
            if (t9TapKey != '\0') {
                t9TapKey = '\0';
                t9TapIndex = 0;
            } else if (t9Cursor > 0) {
                t9Text.remove(t9Cursor - 1, 1);
                t9Cursor--;
                // If backspaced to fallback start, exit fallback
                if (t9Fallback && t9Cursor <= t9FallbackStart) {
                    t9Fallback = false;
                }
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
        if ((t9InputMode == MODE_ABC || t9Fallback) && t9TapKey != '\0') {
            t9CommitMultiTap();
        } else if (t9InputMode == MODE_T9 && t9predict.hasInput()) {
            t9CommitPrediction();
        } else {
            // No pending input — insert newline
            t9Text = t9Text.substring(0, t9Cursor) + "\n" + t9Text.substring(t9Cursor);
            t9Cursor++;
        }
        t9Fallback = false;
        return;
    }

    // TAB: always insert newline (commit pending first)
    if (key == KEY_TAB) {
        t9CursorMoveTime = millis();
        if ((t9InputMode == MODE_ABC || t9Fallback) && t9TapKey != '\0') t9CommitMultiTap();
        if (t9InputMode == MODE_T9 && t9predict.hasInput()) t9CommitPrediction();
        t9Fallback = false;
        t9Text = t9Text.substring(0, t9Cursor) + "\n" + t9Text.substring(t9Cursor);
        t9Cursor++;
        return;
    }

    // UP/DOWN: cycle candidates when T9 has input, otherwise move cursor vertically
    if (key == KEY_UP) {
        t9CursorMoveTime = millis();
        if (t9InputMode == MODE_T9 && t9predict.hasInput() && !t9Fallback) {
            t9predict.prevPrefixCandidate();
        } else {
            // Commit pending, then move cursor up
            if ((t9InputMode == MODE_ABC || t9Fallback) && t9TapKey != '\0') t9CommitMultiTap();
            if (t9InputMode == MODE_T9 && t9predict.hasInput()) t9CommitPrediction();
            t9Fallback = false;
            t9MoveCursorVertically(-1);
        }
        return;
    }
    if (key == KEY_DOWN) {
        t9CursorMoveTime = millis();
        if (t9InputMode == MODE_T9 && t9predict.hasInput() && !t9Fallback) {
            t9predict.nextPrefixCandidate();
        } else {
            if ((t9InputMode == MODE_ABC || t9Fallback) && t9TapKey != '\0') t9CommitMultiTap();
            if (t9InputMode == MODE_T9 && t9predict.hasInput()) t9CommitPrediction();
            t9Fallback = false;
            t9MoveCursorVertically(1);
        }
        return;
    }

    // LEFT/RIGHT: commit pending + move cursor horizontally (TASK_4)
    if (key == KEY_LEFT) {
        t9CursorMoveTime = millis();
        if ((t9InputMode == MODE_ABC || t9Fallback) && t9TapKey != '\0') t9CommitMultiTap();
        if (t9InputMode == MODE_T9 && t9predict.hasInput()) t9CommitPrediction();
        t9Fallback = false;
        if (t9Cursor > 0) t9Cursor--;
        return;
    }
    if (key == KEY_RIGHT) {
        t9CursorMoveTime = millis();
        if ((t9InputMode == MODE_ABC || t9Fallback) && t9TapKey != '\0') t9CommitMultiTap();
        if (t9InputMode == MODE_T9 && t9predict.hasInput()) t9CommitPrediction();
        t9Fallback = false;
        if (t9Cursor < (int)t9Text.length()) t9Cursor++;
        return;
    }

    // DIGIT KEYS (0-9)
    if (key >= '0' && key <= '9') {
        // Long-press in T9/ABC mode: replace the pending letter with the digit
        if ((t9InputMode == MODE_T9 || t9InputMode == MODE_ABC) && isKeyHeld(key)) {
            if ((t9InputMode == MODE_ABC || t9Fallback) && t9TapKey != '\0') {
                // Cancel the pending multi-tap character (don't commit the letter)
                t9TapKey = '\0';
                t9TapIndex = 0;
            }
            if (t9InputMode == MODE_T9 && !t9Fallback) {
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
        } else if (t9InputMode == MODE_ABC || (t9InputMode == MODE_T9 && t9Fallback)) {
            // Multi-tap mode (also used during T9 fallback)
            if (t9Fallback && key == '0') {
                // Space exits fallback, return to normal T9
                if (t9TapKey != '\0') t9CommitMultiTap();
                t9Fallback = false;
                t9Text = t9Text.substring(0, t9Cursor) + " " + t9Text.substring(t9Cursor);
                t9Cursor++;
                return;
            }
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
                // If no prefix match after this digit, fall back to ABC
                if (t9predict.getPrefixCandidateCount() == 0) {
                    // Pop the failed digit
                    t9predict.popDigit();
                    // Commit whatever was successfully predicted so far
                    if (t9predict.hasInput()) t9CommitPrediction();
                    // Enter fallback-ABC mode
                    t9Fallback = true;
                    t9FallbackStart = t9Cursor;
                    // Process the failed digit as ABC multi-tap
                    t9TapKey = key;
                    t9TapIndex = 0;
                    t9TapTime = millis();
                }
            }
        }
    }
}

void SettingsApp::renderT9Editor() {
    // Header: mode indicator + shift state
    const char* modeStr = t9Fallback ? "?ABC" :
                          (t9InputMode == MODE_T9) ? "T9" :
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

    if ((t9InputMode == MODE_ABC || t9Fallback) && t9TapKey != '\0') {
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

    // Compute fallback region in displayText coordinates (inverted rendering)
    int fbDispStart = -1, fbDispEnd = -1;
    if (t9Fallback) {
        fbDispStart = t9FallbackStart;
        fbDispEnd = t9Cursor + (int)preview.length(); // includes pending multi-tap preview
    }

    // Render visible lines
    int y = 22;
    int cursorScreenX = -1, cursorScreenY = -1;
    for (int i = 0; i < maxVisibleLines; i++) {
        int li = t9ScrollOffset + i;
        if (li >= totalLines) break;
        int lStart = lineStarts[li];
        int lEnd = lineEnds[li];
        String line = displayText.substring(lStart, lEnd);

        // Check if fallback region overlaps this line
        if (fbDispStart >= 0 && fbDispStart < lEnd && fbDispEnd > lStart) {
            int regionS = max(fbDispStart, lStart) - lStart; // local char index
            int regionE = min(fbDispEnd, lEnd) - lStart;

            // Draw normal part before fallback
            if (regionS > 0) {
                String before = line.substring(0, regionS);
                u8g2.drawStr(1, y, before.c_str());
            }

            // Draw inverted fallback region
            String fbPart = line.substring(regionS, regionE);
            int xFb = 1 + u8g2.getStrWidth(line.substring(0, regionS).c_str());
            int wFb = u8g2.getStrWidth(fbPart.c_str());
            u8g2.drawBox(xFb, y - 7, wFb, 9);
            u8g2.setDrawColor(0);
            u8g2.drawStr(xFb, y, fbPart.c_str());
            u8g2.setDrawColor(1);

            // Draw normal part after fallback
            if (regionE < (int)line.length()) {
                String after = line.substring(regionE);
                int xAfter = xFb + wFb;
                u8g2.drawStr(xAfter, y, after.c_str());
            }
        } else {
            u8g2.drawStr(1, y, line.c_str());
        }

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
    if (t9Fallback && t9TapKey != '\0') {
        // Fallback ABC: show multi-tap map for current key
        const char* map = multiTapMap[t9TapKey - '0'];
        char bar[32];
        snprintf(bar, sizeof(bar), "?[%s] %d/%d", map, t9TapIndex + 1, (int)strlen(map));
        u8g2.drawStr(1, 62, bar);
    } else if (t9Fallback) {
        u8g2.drawStr(1, 62, "?ABC 0:sp exits");
    } else if (t9InputMode == MODE_T9 && t9predict.hasInput()) {
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
