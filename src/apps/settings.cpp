// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/apps/settings.cpp
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TACTICAL_TODO TASK_1
// LOG_REF: 2026-04-22
// Description: Settings app — brightness, sleep toggle, key tester, T9 editor launcher, SD card info.

#include "settings.h"
#include "t9_editor.h"
#include "../app_control.h"
#include "../app_transfer.h"
#include "../gui.h"
#include "../lua_binding_help.h"
#include "../lua_blank_app_template.h"
#include "../config.h"
#include "../clock.h"
#include "../hal.h"

extern T9EditorApp appT9Editor;

// External sleep control (defined in main.cpp)
extern bool sleepEnabled;

static const char* kSettingsBatteryStub = "BAT --.-V";
static const uint32_t kSettingsPhysicalRamK = 320;

namespace {

static const uint16_t kKeyTesterSettleDelayPresetsUs[] = {10, 25, 50, 100, 200, 400};

static bool isHeldNow(char key) {
    for (int i = 0; i < activeKeyCount; i++) {
        if (activeKeys[i] == key) {
            return true;
        }
    }
    return false;
}

static uint16_t nextKeyTesterSettleDelayUs(uint16_t current) {
    const size_t presetCount = sizeof(kKeyTesterSettleDelayPresetsUs) / sizeof(kKeyTesterSettleDelayPresetsUs[0]);
    for (size_t index = 0; index < presetCount; index++) {
        if (kKeyTesterSettleDelayPresetsUs[index] > current) {
            return kKeyTesterSettleDelayPresetsUs[index];
        }
    }
    return kKeyTesterSettleDelayPresetsUs[0];
}

static uint16_t prevKeyTesterSettleDelayUs(uint16_t current) {
    const size_t presetCount = sizeof(kKeyTesterSettleDelayPresetsUs) / sizeof(kKeyTesterSettleDelayPresetsUs[0]);
    for (size_t index = presetCount; index-- > 0;) {
        if (kKeyTesterSettleDelayPresetsUs[index] < current) {
            return kKeyTesterSettleDelayPresetsUs[index];
        }
    }
    return kKeyTesterSettleDelayPresetsUs[presetCount - 1];
}

struct SettingsSdSessionGuard {
    bool active = false;

    bool begin() {
        active = sdBeginSession();
        return active;
    }

    ~SettingsSdSessionGuard() {
        if (active) {
            sdEndSession();
        }
    }
};

static String getPathTail(const String& path) {
    int slash = path.lastIndexOf('/');
    if (slash >= 0 && slash + 1 < path.length()) {
        return path.substring(slash + 1);
    }
    return path;
}

static void launchSettingsViewerBuffer(App* caller, const String& label, const String& content) {
    appTransferAction = ACTION_VIEW_FILE;
    appTransferBool = false;
    appTransferResultReady = false;
    appTransferString = content;
    appTransferPath = "";
    appTransferLabel = label;
    appTransferEditorMode = APP_TRANSFER_EDITOR_READ_ONLY;
    appTransferSourceKind = APP_TRANSFER_SOURCE_BUFFER;
    appTransferCaller = caller;
    switchApp(&appT9Editor);
}

static void launchSettingsEditorFile(App* caller, const String& path, const String& label) {
    appTransferAction = ACTION_EDIT_FILE;
    appTransferBool = false;
    appTransferResultReady = false;
    appTransferString = "";
    appTransferPath = path;
    appTransferLabel = label;
    appTransferEditorMode = APP_TRANSFER_EDITOR_READ_WRITE;
    appTransferSourceKind = APP_TRANSFER_SOURCE_PAGED_FILE;
    appTransferCaller = caller;
    switchApp(&appT9Editor);
}

static bool createBlankLuaAppOnSd(String& createdPath, String& error) {
    createdPath = "";
    error = "";

    if (!isSDMounted()) {
        error = "SD not mounted";
        return false;
    }

    SettingsSdSessionGuard session;
    if (!session.begin()) {
        error = "Failed to open SD session";
        return false;
    }

    String templateText = buildLuaBlankAppTemplateText();
    for (int index = 0; index < 1000; index++) {
        String candidate = (index == 0)
            ? String("/blank_app.lua")
            : String("/blank_app_") + String(index) + String(".lua");

        FsFile existing;
        if (existing.open(candidate.c_str(), O_RDONLY)) {
            existing.close();
            continue;
        }

        FsFile file;
        if (!file.open(candidate.c_str(), O_WRONLY | O_CREAT | O_TRUNC)) {
            error = String("Failed to create: ") + candidate;
            return false;
        }

        const size_t bytesWritten = file.write(
            reinterpret_cast<const uint8_t*>(templateText.c_str()),
            templateText.length());
        if (bytesWritten != templateText.length()) {
            error = String("Failed to write: ") + candidate;
            return false;
        }
        if (!file.sync()) {
            error = String("Failed to sync: ") + candidate;
            return false;
        }

        createdPath = candidate;
        return true;
    }

    error = "No free blank_app name";
    return false;
}

}  // namespace

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

static char getKeyHistoryToken(char key) {
    switch (key) {
        case KEY_ESC:   return 'E';
        case KEY_BKSP:  return 'B';
        case KEY_TAB:   return 'T';
        case KEY_ENTER: return 'N';
        case KEY_SHIFT: return 'S';
        case KEY_ALT:   return 'A';
        case KEY_UP:    return 'U';
        case KEY_DOWN:  return 'D';
        case KEY_LEFT:  return 'L';
        case KEY_RIGHT: return 'R';
        default:        return key;
    }
}

// Multi-tap character maps (same layout as t9_engine.cpp)
static const char* multiTapMap[] = {
    " 0",        // 0
    ".,?!1()[]{}<>:;\"'`=+-*/%#_\\|&$", // 1 (Lua coding symbols)
    "abc2",      // 2
    "def3",      // 3
    "ghi4",      // 4
    "jkl5",      // 5
    "mno6",      // 6
    "pqrs7",     // 7
    "tuv8",      // 8
    "wxyz9"      // 9
};

static char getMatchingBracket(char left) {
    switch (left) {
        case '(': return ')';
        case '[': return ']';
        case '{': return '}';
        case '<': return '>';
        case '"': return '"';
        case '\'': return '\'';
        default:  return '\0';
    }
}

static void insertCharWithAutoBracket(String& text, int& cursor, char c) {
    char right = getMatchingBracket(c);
    if (right != '\0') {
        String pair = String(c) + String(right);
        text = text.substring(0, cursor) + pair + text.substring(cursor);
        cursor += 1; // keep cursor between brackets
        return;
    }
    text = text.substring(0, cursor) + String(c) + text.substring(cursor);
    cursor += 1;
}

static void drawHighlightedChoiceBar(int xStart, int baselineY, const char* choices, int selectedIndex) {
    if (!choices) return;

    GUI::setFontSystem();
    const GUI::FontMetrics& metrics = GUI::getSystemFontMetrics();
    const int availableWidth = GUI::SCREEN_WIDTH - xStart - 5;

    int count = strlen(choices);
    if (count <= 0) return;
    if (selectedIndex < 0) selectedIndex = 0;
    if (selectedIndex >= count) selectedIndex = count - 1;

    int widths[40];
    int safeCount = min(count, 40);
    for (int i = 0; i < safeCount; i++) {
        char item[2] = {choices[i], '\0'};
        widths[i] = u8g2.getStrWidth(item) + 2;
    }

    int startIndex = 0;
    int widthToSelected = 0;
    for (int i = 0; i < selectedIndex && i < safeCount; i++) widthToSelected += widths[i];
    while (widthToSelected > (availableWidth / 2) && startIndex < selectedIndex) {
        widthToSelected -= widths[startIndex];
        startIndex++;
    }

    int x = xStart;
    if (startIndex > 0) {
        u8g2.drawStr(x, baselineY, "<");
        x += u8g2.getStrWidth("<") + 2;
    }

    bool clippedRight = false;
    for (int i = startIndex; i < safeCount; i++) {
        char item[2] = {choices[i], '\0'};
        int charWidth = u8g2.getStrWidth(item);
        int paddedWidth = charWidth + 2;
        if (x + paddedWidth > GUI::SCREEN_WIDTH - 5) {
            clippedRight = (i < safeCount);
            break;
        }

        if (i == selectedIndex) {
            u8g2.drawBox(x - 1, GUI::getHighlightTop(baselineY), paddedWidth + 1, GUI::getHighlightHeight());
            u8g2.setDrawColor(0);
            u8g2.drawStr(x, baselineY, item);
            u8g2.setDrawColor(1);
        } else {
            u8g2.drawStr(x, baselineY, item);
        }
        x += paddedWidth;
    }

    if (clippedRight) {
        u8g2.drawStr(GUI::SCREEN_WIDTH - 5, baselineY, ">");
    }
}

SettingsApp::SettingsApp() {
    selectedIndex = 0;
    editMode = false;
    inKeyTester = false;
    inT9Editor = false;
    tempBrightness = systemBrightness;
    tempContrast = systemContrast;
    tempSleepEnabled = SLEEP_ENABLED;
    tempT9FontSizeIndex = GUI::getSystemFontOptionIndex();
    tempReadOnlyPageSizeIndex = getT9EditorReadOnlyPageSizeOptionIndex();
    lastPressedKey = ' ';
    for (int i = 0; i < HISTORY_SIZE; i++) keyHistory[i] = ' ';
    keyHistory[HISTORY_SIZE] = '\0';
    inLcdTest = false;
    lcdTestStep = 0;
    inSdTest = false;
    sdTestRan = false;
    sdRemountPending = false;
    sdRemountDeadline = 0;
    keyTesterPollBaseline = 0;
    keyTesterRawHitBaseline = 0;
    keyTesterLatchedBaseline = 0;
    keyTesterDuplicateBaseline = 0;
    keyTesterDeliveredCount = 0;
    sdTestLineCount = 0;
    t9EditorReset();
}

void SettingsApp::start() {
    tempBrightness = systemBrightness;
    tempContrast = systemContrast;
    tempSleepEnabled = sleepEnabled;
    tempT9FontSizeIndex = GUI::getSystemFontOptionIndex();
    tempReadOnlyPageSizeIndex = getT9EditorReadOnlyPageSizeOptionIndex();
    editMode = false;
    inKeyTester = false;
    inT9Editor = false;
    inLcdTest = false;
    lcdTestStep = 0;
    inSdTest = false;
    sdTestRan = false;
    sdRemountPending = false;
    sdRemountDeadline = 0;
    selectedIndex = 0;
}

void SettingsApp::stop() {
    systemBrightness = tempBrightness;
    systemContrast = tempContrast;
    sleepEnabled = tempSleepEnabled;
    GUI::setSystemFontOptionIndex(tempT9FontSizeIndex);
    setT9EditorReadOnlyPageSizeOptionIndex(tempReadOnlyPageSizeIndex);
    inKeyTester = false;
    inT9Editor = false;
    inLcdTest = false;
    inSdTest = false;
    sdRemountPending = false;
}

bool SettingsApp::isInSubmenu() {
    return inKeyTester || inT9Editor || inLcdTest || inSdTest;
}

void SettingsApp::update() {
    if (sdRemountPending && millis() >= sdRemountDeadline) {
        bool mounted = mountSD();
        GUI::showToast(mounted ? "SD card mounted" : "SD card failed", 1400);
        sdRemountPending = false;
    }

    // Auto-commit multi-tap after timeout (ABC mode or T9 fallback)
    if (inT9Editor && !t9SavePromptActive && (t9InputMode == MODE_ABC || t9Fallback) && t9TapKey != '\0') {
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

    if (inT9Editor && !t9SavePromptActive && t9InputMode == MODE_T9 && t9ZeroPending) {
        bool zeroStillPressed = false;
        for (int i = 0; i < activeKeyCount; i++) {
            if (activeKeys[i] == '0') {
                zeroStillPressed = true;
                break;
            }
        }

        if (!zeroStillPressed) {
            t9Text = t9Text.substring(0, t9Cursor) + " " + t9Text.substring(t9Cursor);
            t9Cursor++;
            t9CursorMoveTime = millis();
            t9ZeroPending = false;
        }
    }
}

void SettingsApp::addToHistory(char c) {
    for (int i = 0; i < HISTORY_SIZE - 1; i++) keyHistory[i] = keyHistory[i + 1];
    keyHistory[HISTORY_SIZE - 1] = getKeyHistoryToken(c);
}

void SettingsApp::resetKeyTesterSession() {
    lastPressedKey = ' ';
    for (int i = 0; i < HISTORY_SIZE; i++) keyHistory[i] = ' ';
    keyTesterPollBaseline = getMatrixPollCount();
    keyTesterRawHitBaseline = getMatrixRawHitCount();
    keyTesterLatchedBaseline = getMatrixLatchedHitCount();
    keyTesterDuplicateBaseline = getMatrixDuplicateHitCount();
    keyTesterDeliveredCount = 0;
}

void SettingsApp::handleInput(char key) {
    if (sdRemountPending) {
        return;
    }

    if (inKeyTester) {
        if (key == KEY_ESC) {
            inKeyTester = false;
        } else if (isHeldNow(KEY_ALT) && key == KEY_LEFT) {
            setMatrixSettleDelayUs(prevKeyTesterSettleDelayUs(getMatrixSettleDelayUs()));
        } else if (isHeldNow(KEY_ALT) && key == KEY_RIGHT) {
            setMatrixSettleDelayUs(nextKeyTesterSettleDelayUs(getMatrixSettleDelayUs()));
        } else if (isHeldNow(KEY_ALT) && key == KEY_ENTER) {
            resetKeyTesterSession();
        } else {
            lastPressedKey = key;
            keyTesterDeliveredCount++;
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
            if (selectedIndex == SETTING_SD_REMOUNT) {
                unmountSD();
                GUI::showToast("Checking SDcard", 450);
                sdRemountPending = true;
                sdRemountDeadline = millis() + 450;
            } else if (selectedIndex == SETTING_NEW_BLANK_LUA_APP) {
                String createdPath;
                String error;
                if (!createBlankLuaAppOnSd(createdPath, error)) {
                    GUI::showToast(error == "SD not mounted" ? "Mount SD first" : "Create failed", 1400);
                } else {
                    launchSettingsEditorFile(this, createdPath, getPathTail(createdPath));
                }
            } else if (selectedIndex == SETTING_KEY_TESTER) {
                inKeyTester = true;
                resetKeyTesterSession();
            } else if (selectedIndex == SETTING_LCD_TEST) {
                inLcdTest = true;
                lcdTestStep = 0;
            } else if (selectedIndex == SETTING_SD_TEST) {
                inSdTest = true;
                sdTestRan = false;
            } else if (selectedIndex == SETTING_HELP) {
                launchSettingsViewerBuffer(this, "Lua Help", buildLuaBindingHelpText());
            } else {
                editMode = true;
            }
        }
    } else {
        if (key == KEY_ENTER) {
            editMode = false;
            if (selectedIndex == SETTING_T9_FONT_SIZE) {
                GUI::setSystemFontOptionIndex(tempT9FontSizeIndex);
            }
            if (selectedIndex == SETTING_BRIGHTNESS) {
                systemBrightness = tempBrightness;
                ledcWrite(0, systemBrightness);
            }
            if (selectedIndex == SETTING_RO_PAGE_SIZE) {
                setT9EditorReadOnlyPageSizeOptionIndex(tempReadOnlyPageSizeIndex);
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

        if (selectedIndex == SETTING_T9_FONT_SIZE) {
            if (key == KEY_LEFT || key == KEY_RIGHT) {
                int nextIndex = tempT9FontSizeIndex;
                if (key == KEY_LEFT) nextIndex--;
                else nextIndex++;

                if (nextIndex < 0) nextIndex = GUI::kSystemFontOptionCount - 1;
                if (nextIndex >= GUI::kSystemFontOptionCount) nextIndex = 0;

                tempT9FontSizeIndex = nextIndex;
            }
        }

        if (selectedIndex == SETTING_RO_PAGE_SIZE) {
            if (key == KEY_LEFT || key == KEY_RIGHT) {
                int nextIndex = tempReadOnlyPageSizeIndex;
                if (key == KEY_LEFT) nextIndex--;
                else nextIndex++;

                if (nextIndex < 0) nextIndex = kT9EditorReadOnlyPageSizeOptionCount - 1;
                if (nextIndex >= kT9EditorReadOnlyPageSizeOptionCount) nextIndex = 0;

                tempReadOnlyPageSizeIndex = nextIndex;
            }
        }
    }
}

void SettingsApp::renderInfoHeader() {
    const GUI::FontMetrics& infoMetrics = GUI::getSecondaryFontMetrics();
    u8g2.drawBox(0, 0, GUI::SCREEN_WIDTH, GUI::SCREEN_HEIGHT);
    u8g2.setDrawColor(0);
    GUI::setFontSecondary();

    const uint32_t totalHeapK = ESP.getHeapSize() / 1024;
    const uint32_t freeHeapK = ESP.getFreeHeap() / 1024;
    const uint32_t usedHeapK = totalHeapK >= freeHeapK ? totalHeapK - freeHeapK : 0;
    const uint32_t totalPsramK = ESP.getPsramSize() / 1024;
    const uint32_t freePsramK = ESP.getFreePsram() / 1024;
    const uint32_t usedPsramK = totalPsramK >= freePsramK ? totalPsramK - freePsramK : 0;

    auto drawInfoRow = [](int baselineY, const char* leftText, const char* rightText) {
        if (leftText && leftText[0] != '\0') {
            u8g2.drawStr(2, baselineY, leftText);
        }
        if (rightText && rightText[0] != '\0') {
            int rightX = GUI::SCREEN_WIDTH - 2 - u8g2.getStrWidth(rightText);
            if (rightX < 2) rightX = 2;
            u8g2.drawStr(rightX, baselineY, rightText);
        }
    };

    char heapLeft[24];
    char heapRight[16];
    snprintf(heapLeft, sizeof(heapLeft), "HEAP %lu/%luk",
             static_cast<unsigned long>(usedHeapK),
             static_cast<unsigned long>(totalHeapK));
    snprintf(heapRight, sizeof(heapRight), "INT %luk",
             static_cast<unsigned long>(kSettingsPhysicalRamK));
    const int row1Y = infoMetrics.baselineOffset;
    const int row2Y = row1Y + infoMetrics.lineHeight;
    const int row3Y = row2Y + infoMetrics.lineHeight;

    drawInfoRow(row1Y, heapLeft, heapRight);

    char psramLeft[28];
    if (totalPsramK > 0) {
        snprintf(psramLeft, sizeof(psramLeft), "PSRAM %lu/%luk",
                 static_cast<unsigned long>(usedPsramK),
                 static_cast<unsigned long>(totalPsramK));
    } else {
        snprintf(psramLeft, sizeof(psramLeft), "PSRAM none");
    }
    drawInfoRow(row2Y, psramLeft, kSettingsBatteryStub);

    char timeBuf[8];
    SystemClock::getTimeString(timeBuf, sizeof(timeBuf));

    char sdLeft[24];
    if (isSDMounted()) {
        const uint64_t totalMb = sdTotalBytes() / (1024 * 1024);
        const uint64_t usedMb = sdUsedBytes() / (1024 * 1024);
        snprintf(sdLeft, sizeof(sdLeft), "SD %llu/%lluM",
                 static_cast<unsigned long long>(usedMb),
                 static_cast<unsigned long long>(totalMb));
    } else {
        snprintf(sdLeft, sizeof(sdLeft), "SD --/--M");
    }

    char statusRight[24];
    snprintf(statusRight, sizeof(statusRight), "v%s %s", FIRMWARE_VERSION, timeBuf);
    drawInfoRow(row3Y, sdLeft, statusRight);
    u8g2.drawHLine(0, row3Y + 4, GUI::SCREEN_WIDTH);
}

void SettingsApp::renderSettingsList() {
    const GUI::FontMetrics& infoMetrics = GUI::getSecondaryFontMetrics();
    const GUI::FontMetrics& metrics = GUI::getSystemFontMetrics();

    renderInfoHeader();
    GUI::setFontSystem();

    const int totalItems = SETTING_COUNT;
    const int headerBottom = (infoMetrics.lineHeight * 3) + 4;
    const int listTop = headerBottom + metrics.baselineOffset + 2;
    const int lineHeight = metrics.lineHeight;
    int maxVisible = (GUI::SCREEN_HEIGHT - listTop - 1) / lineHeight;
    if (maxVisible < 1) maxVisible = 1;

    int scrollOff = 0;
    if (selectedIndex >= maxVisible) {
        scrollOff = selectedIndex - maxVisible + 1;
    }
    if (scrollOff < 0) scrollOff = 0;

    for (int vis = 0; vis < maxVisible && (scrollOff + vis) < totalItems; vis++) {
        int idx = scrollOff + vis;
        int y = listTop + vis * lineHeight;
        bool isSelected = (idx == selectedIndex);
        char buf[40];

        if (isSelected) {
            u8g2.drawBox(0, GUI::getHighlightTop(y), 123, GUI::getHighlightHeight());
            u8g2.setDrawColor(1);
        }

        switch ((SettingItem)idx) {
        case SETTING_SD_REMOUNT:
            snprintf(buf, sizeof(buf), "(Re)mount SD card");
            break;
        case SETTING_T9_FONT_SIZE:
            if (editMode && isSelected)
                snprintf(buf, sizeof(buf), "System font: <%s>", GUI::getSystemFontOptionLabel(tempT9FontSizeIndex));
            else
                snprintf(buf, sizeof(buf), "System font: %s", GUI::getSystemFontOptionLabel(tempT9FontSizeIndex));
            break;
        case SETTING_BRIGHTNESS: {
            int pct = (tempBrightness * 100) / 255;
            if (editMode && isSelected)
                snprintf(buf, sizeof(buf), "Backlight: <%d%%>", pct);
            else
                snprintf(buf, sizeof(buf), "Backlight: %d%%", pct);
            break;
        }
        case SETTING_NEW_BLANK_LUA_APP:
            snprintf(buf, sizeof(buf), "New blank .lua app");
            break;
        case SETTING_RO_PAGE_SIZE: {
            const size_t pageBytes = getT9EditorReadOnlyPageSizeOption(tempReadOnlyPageSizeIndex);
            if (editMode && isSelected)
                snprintf(buf, sizeof(buf), "Reader page: <%uB>", static_cast<unsigned>(pageBytes));
            else
                snprintf(buf, sizeof(buf), "Reader page: %uB", static_cast<unsigned>(pageBytes));
            break;
        }
        case SETTING_KEY_TESTER:
            snprintf(buf, sizeof(buf), "Key tester");
            break;
        case SETTING_LCD_TEST:
            snprintf(buf, sizeof(buf), "LCD tester");
            break;
        case SETTING_SD_TEST:
            snprintf(buf, sizeof(buf), "SD pin tester");
            break;
        case SETTING_HELP:
            snprintf(buf, sizeof(buf), "Help");
            break;
        default:
            buf[0] = '\0';
        }

        String text = GUI::truncateStringToWidth(String(buf), GUI::SCREEN_WIDTH - GUI::SCROLLBAR_WIDTH - 6);
        u8g2.drawUTF8(2, y, text.c_str());
        if (isSelected) u8g2.setDrawColor(0);
    }

    if (totalItems > maxVisible) {
        GUI::drawScrollbar(
            GUI::SCREEN_WIDTH - GUI::SCROLLBAR_WIDTH - 1,
            listTop - metrics.glyphTopOffset,
            maxVisible * lineHeight,
            totalItems,
            maxVisible,
            scrollOff
        );
    }

    u8g2.setDrawColor(1);
}

void SettingsApp::renderKeyTester() {
    GUI::drawHeader("KEY TESTER");
    GUI::setFontSystem();
    const GUI::FontMetrics& metrics = GUI::getSystemFontMetrics();
    const int line1 = GUI::getContentBaselineStart();
    const int line2 = line1 + metrics.lineHeight;
    const int line3 = line2 + metrics.lineHeight;
    const int line4 = line3 + metrics.lineHeight;
    const int line5 = line4 + metrics.lineHeight;

    char lastBuf[8] = "-";
    if (lastPressedKey != ' ') {
        const char* name = getKeyName(lastPressedKey);
        if (name) {
            snprintf(lastBuf, sizeof(lastBuf), "%s", name);
        } else if (lastPressedKey >= 32 && lastPressedKey <= 126) {
            snprintf(lastBuf, sizeof(lastBuf), "[%c]", lastPressedKey);
        } else {
            snprintf(lastBuf, sizeof(lastBuf), "0x%02X", static_cast<unsigned char>(lastPressedKey));
        }
    }

    char lineBuf[32];
    snprintf(lineBuf, sizeof(lineBuf), "LAST:%s", lastBuf);
    u8g2.drawStr(0, line1, lineBuf);

    snprintf(lineBuf, sizeof(lineBuf), "EVT:%s", keyHistory);
    u8g2.drawStr(0, line2, lineBuf);

    const uint32_t rawDelta = getMatrixRawHitCount() - keyTesterRawHitBaseline;
    const uint32_t latchedDelta = getMatrixLatchedHitCount() - keyTesterLatchedBaseline;
    const uint32_t duplicateDelta = getMatrixDuplicateHitCount() - keyTesterDuplicateBaseline;
    const uint32_t pollDelta = getMatrixPollCount() - keyTesterPollBaseline;

    snprintf(lineBuf, sizeof(lineBuf), "RAW:%lu LAT:%lu",
             static_cast<unsigned long>(rawDelta),
             static_cast<unsigned long>(latchedDelta));
    u8g2.drawStr(0, line3, lineBuf);

    snprintf(lineBuf, sizeof(lineBuf), "DUP:%lu IN:%lu",
             static_cast<unsigned long>(duplicateDelta),
             static_cast<unsigned long>(keyTesterDeliveredCount));
    u8g2.drawStr(0, line4, lineBuf);

    snprintf(lineBuf, sizeof(lineBuf), "SET:%uus P:%lu",
             static_cast<unsigned>(getMatrixSettleDelayUs()),
             static_cast<unsigned long>(pollDelta));
    u8g2.drawStr(0, line5, lineBuf);

    u8g2.drawStr(0, GUI::getFooterBaselineY() - metrics.lineHeight, "A+L/R:set A+E:rst");

    int xPos = 28;
    u8g2.drawUTF8(0, GUI::getFooterBaselineY(), "HLD:");
    for (int i = 0; i < activeKeyCount; i++) {
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

void SettingsApp::render() {
    bool invertedMainList = false;
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
        invertedMainList = true;
    }

    if (invertedMainList) {
        u8g2.setDrawColor(0);
        GUI::updateToast();
        u8g2.setDrawColor(1);
    } else {
        GUI::updateToast();
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
        GUI::setFontSystem();
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
        GUI::setFontSystem();
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
        GUI::setFontSystem();
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
        GUI::setFontSystem();
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
        GUI::setFontSecondary();
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
        GUI::setFontSystem();
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

    // --- Step 1: Test GPIO pins ---
    // Test SD CS pin
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
        delayMicroseconds(10);
        digitalWrite(PIN_SPI_SCLK, LOW);
        delayMicroseconds(10);
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
            delayMicroseconds(10);
            digitalWrite(PIN_SPI_SCLK, HIGH);
            delayMicroseconds(10);
            digitalWrite(PIN_SPI_SCLK, LOW);
        }
    }

    // Read response — wait up to 16 bytes for non-0xFF
    uint8_t resp = 0xFF;
    for (int attempt = 0; attempt < 16; attempt++) {
        uint8_t rxByte = 0;
        for (int bit = 7; bit >= 0; bit--) {
            digitalWrite(PIN_SPI_SCLK, HIGH);
            delayMicroseconds(10);
            if (digitalRead(PIN_SPI_MISO)) rxByte |= (1 << bit);
            digitalWrite(PIN_SPI_SCLK, LOW);
            delayMicroseconds(10);
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

    // --- Step 2b: bit-bang helper lambdas ---
    auto bbSendByte = [](uint8_t b) {
        for (int bit = 7; bit >= 0; bit--) {
            digitalWrite(PIN_SPI_MOSI, (b >> bit) & 1);
            delayMicroseconds(10);
            digitalWrite(PIN_SPI_SCLK, HIGH);
            delayMicroseconds(20);
            digitalWrite(PIN_SPI_SCLK, LOW);
            delayMicroseconds(10);
        }
    };
    auto bbRecvByte = []() -> uint8_t {
        digitalWrite(PIN_SPI_MOSI, HIGH);  // SD spec: DI HIGH while receiving
        uint8_t d = 0;
        for (int bit = 7; bit >= 0; bit--) {
            digitalWrite(PIN_SPI_SCLK, HIGH);
            delayMicroseconds(20);
            if (digitalRead(PIN_SPI_MISO)) d |= (1 << bit);
            digitalWrite(PIN_SPI_SCLK, LOW);
            delayMicroseconds(20);
        }
        return d;
    };
    auto bbWaitR1 = [&bbRecvByte]() -> uint8_t {
        for (int i = 0; i < 16; i++) {
            uint8_t r = bbRecvByte();
            if (r != 0xFF) return r;
        }
        return 0xFF;
    };
    auto bbSendCmd = [&bbSendByte](const uint8_t* cmd) {
        for (int i = 0; i < 6; i++) bbSendByte(cmd[i]);
    };
    // Dummy clocks between commands — MOSI HIGH, CS can be either state
    auto bbDummyClocks = [](int count) {
        digitalWrite(PIN_SPI_MOSI, HIGH);
        for (int i = 0; i < count; i++) {
            digitalWrite(PIN_SPI_SCLK, HIGH);
            delayMicroseconds(20);
            digitalWrite(PIN_SPI_SCLK, LOW);
            delayMicroseconds(20);
        }
    };

    // --- Step 2c: CMD8 (SEND_IF_COND) — the command SdFat fails on ---
    if (resp == 0x01) {
        // Dummy clocks with CS HIGH, MOSI HIGH — let card process CMD0
        digitalWrite(PIN_SD_CS, HIGH);
        bbDummyClocks(80);
        digitalWrite(PIN_SD_CS, LOW);
        delayMicroseconds(50);

        // CMD8: 0x48 0x00 0x00 0x01 0xAA 0x87
        uint8_t cmd8[] = {0x48, 0x00, 0x00, 0x01, 0xAA, 0x87};
        bbSendCmd(cmd8);
        uint8_t r1 = bbWaitR1();
        // R7 response: R1 + 4 bytes
        uint8_t r7[4] = {0};
        if (r1 != 0xFF) {
            for (int i = 0; i < 4; i++) r7[i] = bbRecvByte();
        }
        addLine("CMD8: R1=0x%02X", r1);
        addLine(" R7=%02X%02X%02X%02X", r7[0], r7[1], r7[2], r7[3]);
        // Expected: R1=0x01, R7=000001AA

        // --- Step 2d: CMD55+ACMD41 (init card) ---
        if (r1 == 0x01) {
            uint8_t acmdR1 = 0xFF;
            for (int a = 0; a < 50; a++) {
                digitalWrite(PIN_SD_CS, HIGH);
                bbDummyClocks(16);
                digitalWrite(PIN_SD_CS, LOW);
                delayMicroseconds(50);

                // CMD55: 0x77 0x00 0x00 0x00 0x00 0x65
                uint8_t cmd55[] = {0x77, 0x00, 0x00, 0x00, 0x00, 0x65};
                bbSendCmd(cmd55);
                bbWaitR1();

                digitalWrite(PIN_SD_CS, HIGH);
                bbDummyClocks(16);
                digitalWrite(PIN_SD_CS, LOW);
                delayMicroseconds(50);

                // ACMD41: 0x69 0x40 0x00 0x00 0x00 0x77
                uint8_t acmd41[] = {0x69, 0x40, 0x00, 0x00, 0x00, 0x77};
                bbSendCmd(acmd41);
                acmdR1 = bbWaitR1();
                if (acmdR1 == 0x00) break;
                delay(10);
            }
            addLine("ACMD41: 0x%02X %s", acmdR1,
                    acmdR1 == 0x00 ? "READY" : "FAIL");
        }
    }

    // --- Step 3: Try SdFat mount ---
    bool sdOk = mountSD();
    addLine("SdFat mount: %s", sdOk ? "OK" : "FAIL");

    sdTestRan = true;
}

void SettingsApp::renderSdTest() {
    if (!sdTestRan) {
        runSdPinDiagnostic();
    }

    GUI::drawHeader("SD PIN TEST");
    GUI::setFontSecondary();
    const GUI::FontMetrics& metrics = GUI::getSecondaryFontMetrics();

    int y = GUI::getContentAreaTop() + metrics.baselineOffset;
    for (int i = 0; i < sdTestLineCount && y <= GUI::getContentBottom() - metrics.lineHeight; i++) {
        u8g2.drawStr(1, y, sdTestResults[i]);
        y += metrics.lineHeight;
    }

    GUI::setFontSystem();
    u8g2.drawStr(1, GUI::getFooterBaselineY(), "Esc:Back  >:Retest");
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
    t9ViewMode = VIEW_FULL;
    t9SavePromptActive = false;
    t9SavePromptSelection = 0;
    t9SingleKeyCycleIndex = 0;
    t9ZeroPending = false;
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
    if (c.length() > 0) {
        insertCharWithAutoBracket(t9Text, t9Cursor, c[0]);
    }
    t9TapKey = '\0';
    t9TapIndex = 0;
    t9TapTime = 0;
}

void SettingsApp::t9CommitPrediction() {
    // Use single-key letter-first ordering for one digit, then dictionary prefix.
    int dc = t9predict.getDigitCount();
    String commitText = "";

    if (dc == 1) {
        const char* digs = t9predict.getDigits();
        char digit = (digs && digs[0]) ? digs[0] : '\0';
        int letterCount = t9predict.getSingleKeyLetterCount(digit);
        int dictCount = t9predict.getPrefixCandidateCount();
        int total = letterCount + dictCount;
        if (total > 0) {
            int idx = t9SingleKeyCycleIndex % total;
            if (idx < letterCount) {
                const char* c = t9predict.getSingleKeyLetter(digit, idx);
                if (c) commitText = c;
            } else {
                const char* word = t9predict.getPrefixCandidate(idx - letterCount);
                if (word) {
                    commitText = word;
                    if ((int)commitText.length() > dc) commitText = commitText.substring(0, dc);
                }
            }
        }
    } else {
        const char* word = t9predict.getSelectedPrefixWord();
        if (word) {
            commitText = word;
            if ((int)commitText.length() > dc) commitText = commitText.substring(0, dc);
        }
    }

    if (commitText.length() == 0) {
        // No prefix match — commit raw digits as literal text
        const char* digs = t9predict.getDigits();
        if (digs && dc > 0) {
            String d = digs;
            t9Text = t9Text.substring(0, t9Cursor) + d + t9Text.substring(t9Cursor);
            t9Cursor += d.length();
        }
        t9predict.reset();
        t9SingleKeyCycleIndex = 0;
        return;
    }

    // Apply shift
    if (t9ShiftMode == 1 && commitText.length() > 0) {
        commitText[0] = toupper(commitText[0]);
    } else if (t9ShiftMode == 2) {
        for (int i = 0; i < (int)commitText.length(); i++) {
            if (commitText[i] >= 'a' && commitText[i] <= 'z') commitText[i] -= 32;
        }
    }
    if (commitText.length() == 1) {
        insertCharWithAutoBracket(t9Text, t9Cursor, commitText[0]);
    } else {
        t9Text = t9Text.substring(0, t9Cursor) + commitText + t9Text.substring(t9Cursor);
        t9Cursor += commitText.length();
    }
    t9predict.reset();
    t9SingleKeyCycleIndex = 0;
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

void SettingsApp::t9HandleSavePromptInput(char key) {
    if (!t9SavePromptActive) return;

    if (key == KEY_ESC) {
        t9SavePromptSelection = 0;
        return;
    }

    if (key == KEY_LEFT || key == KEY_RIGHT ||
        key == KEY_UP || key == KEY_DOWN ||
        key == KEY_TAB) {
        t9SavePromptSelection = (t9SavePromptSelection == 0) ? 1 : 0;
        return;
    }

    if (key == KEY_ENTER) {
        bool saveSelected = (t9SavePromptSelection == 1);
        if (saveSelected) {
            Serial.println("[SettingsT9] Save requested, but inline settings editor has no file-save caller");
        }
        t9SavePromptActive = false;
        inT9Editor = false;
        return;
    }
}

void SettingsApp::renderT9SavePrompt() {
    u8g2.drawBox(16, 18, 96, 28);
    u8g2.setDrawColor(0);
    u8g2.drawStr(22, 30, "Save buffer?");

    const bool noSel = (t9SavePromptSelection == 0);
    if (noSel) {
        u8g2.drawBox(24, 34, 26, 10);
        u8g2.setDrawColor(1);
        u8g2.drawStr(29, 42, "No");
        u8g2.setDrawColor(0);
        u8g2.drawStr(68, 42, "Yes");
    } else {
        u8g2.drawStr(29, 42, "No");
        u8g2.drawBox(62, 34, 30, 10);
        u8g2.setDrawColor(1);
        u8g2.drawStr(68, 42, "Yes");
    }
    u8g2.setDrawColor(1);
}

void SettingsApp::t9HandleInput(char key) {
    if (t9SavePromptActive) {
        t9HandleSavePromptInput(key);
        return;
    }

    if (t9ZeroPending && key != '0') {
        t9Text = t9Text.substring(0, t9Cursor) + " " + t9Text.substring(t9Cursor);
        t9Cursor++;
        t9CursorMoveTime = millis();
        t9ZeroPending = false;
    }

    if (key == KEY_ESC) {
        t9SavePromptActive = true;
        t9SavePromptSelection = 0;
        return;
    }

    // ALT: cycle T9 -> ABC -> 123
    if (key == KEY_ALT) {
        if ((t9InputMode == MODE_ABC || t9Fallback) && t9TapKey != '\0') t9CommitMultiTap();
        if (t9InputMode == MODE_T9 && t9predict.hasInput()) t9CommitPrediction();
        t9Fallback = false;
        t9SingleKeyCycleIndex = 0;
        if (t9InputMode == MODE_T9)       t9InputMode = MODE_ABC;
        else if (t9InputMode == MODE_ABC) t9InputMode = MODE_123;
        else                               t9InputMode = MODE_T9;
        t9predict.reset();
        t9TapKey = '\0';
        return;
    }

    if (key == KEY_SHIFT) {
        t9ShiftMode = (t9ShiftMode + 1) % 3;
        return;
    }

    if (key == KEY_BKSP) {
        t9CursorMoveTime = millis();
        if (t9InputMode == MODE_ABC || (t9InputMode == MODE_T9 && t9Fallback)) {
            if (t9TapKey != '\0') {
                t9TapKey = '\0';
                t9TapIndex = 0;
            } else if (t9Cursor > 0) {
                t9Text.remove(t9Cursor - 1, 1);
                t9Cursor--;
                if (t9Fallback && t9Cursor <= t9FallbackStart) {
                    t9Fallback = false;
                }
            }
        } else if (t9InputMode == MODE_T9) {
            if (t9ZeroPending) {
                t9ZeroPending = false;
            } else if (t9TapKey == '1') {
                t9TapKey = '\0';
                t9TapIndex = 0;
                t9TapTime = 0;
            } else if (t9predict.hasInput()) {
                t9predict.popDigit();
                t9SingleKeyCycleIndex = 0;
            } else if (t9Cursor > 0) {
                t9Text.remove(t9Cursor - 1, 1);
                t9Cursor--;
            }
        } else if (t9Cursor > 0) {
            t9Text.remove(t9Cursor - 1, 1);
            t9Cursor--;
        }
        return;
    }

    if (key == KEY_ENTER) {
        t9CursorMoveTime = millis();
        if (((t9InputMode == MODE_ABC || t9Fallback) && t9TapKey != '\0') ||
            (t9InputMode == MODE_T9 && t9TapKey == '1')) {
            t9CommitMultiTap();
        } else if (t9InputMode == MODE_T9 && t9predict.hasInput()) {
            t9CommitPrediction();
        } else {
            t9Text = t9Text.substring(0, t9Cursor) + "\n" + t9Text.substring(t9Cursor);
            t9Cursor++;
        }
        t9Fallback = false;
        return;
    }

    // TAB cycles view modes and no longer inserts newline.
    if (key == KEY_TAB) {
        if (t9ViewMode == VIEW_FULL) t9ViewMode = VIEW_FULL_LINENO;
        else if (t9ViewMode == VIEW_FULL_LINENO) t9ViewMode = VIEW_MIN_LINENO;
        else if (t9ViewMode == VIEW_MIN_LINENO) t9ViewMode = VIEW_MIN;
        else t9ViewMode = VIEW_FULL;
        return;
    }

    if (t9InputMode == MODE_T9 && t9TapKey == '1' &&
        (key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT || key == KEY_RIGHT)) {
        const char* map = multiTapMap[1];
        int count = strlen(map);
        if (count > 0) {
            if (key == KEY_UP || key == KEY_LEFT) t9TapIndex = (t9TapIndex - 1 + count) % count;
            else t9TapIndex = (t9TapIndex + 1) % count;
        }
        t9TapTime = millis();
        return;
    }

    if (key == KEY_UP || key == KEY_DOWN) {
        t9CursorMoveTime = millis();
        if (t9InputMode == MODE_T9 && t9predict.hasInput() && !t9Fallback) {
            if (t9predict.getDigitCount() == 1) {
                const char* digs = t9predict.getDigits();
                char digit = (digs && digs[0]) ? digs[0] : '\0';
                int letterCount = t9predict.getSingleKeyLetterCount(digit);
                int dictCount = t9predict.getPrefixCandidateCount();
                int total = letterCount + dictCount;
                if (total > 0) {
                    if (key == KEY_UP) t9SingleKeyCycleIndex = (t9SingleKeyCycleIndex - 1 + total) % total;
                    else t9SingleKeyCycleIndex = (t9SingleKeyCycleIndex + 1) % total;
                }
            } else {
                if (key == KEY_UP) t9predict.prevPrefixCandidate();
                else t9predict.nextPrefixCandidate();
            }
        } else {
            if ((t9InputMode == MODE_ABC || t9Fallback) && t9TapKey != '\0') t9CommitMultiTap();
            if (t9InputMode == MODE_T9 && t9predict.hasInput()) t9CommitPrediction();
            t9Fallback = false;
            t9MoveCursorVertically(key == KEY_UP ? -1 : 1);
        }
        return;
    }

    if (key == KEY_LEFT || key == KEY_RIGHT) {
        t9CursorMoveTime = millis();
        if ((t9InputMode == MODE_ABC || t9Fallback) && t9TapKey != '\0') t9CommitMultiTap();
        if (t9InputMode == MODE_T9 && t9predict.hasInput()) t9CommitPrediction();
        t9Fallback = false;
        if (key == KEY_LEFT && t9Cursor > 0) t9Cursor--;
        if (key == KEY_RIGHT && t9Cursor < (int)t9Text.length()) t9Cursor++;
        return;
    }

    if (key < '0' || key > '9') return;

    if ((t9InputMode == MODE_T9 || t9InputMode == MODE_ABC) && isKeyHeld(key)) {
        if (t9InputMode == MODE_T9 && key == '0') {
            return;
        }
        if (t9InputMode == MODE_T9 && t9TapKey == '1') {
            if (key == '1') {
                t9TapKey = '\0';
                t9TapIndex = 0;
                t9TapTime = 0;
            } else {
                t9CommitMultiTap();
            }
        } else if ((t9InputMode == MODE_ABC || t9Fallback) && t9TapKey != '\0') {
            t9TapKey = '\0';
            t9TapIndex = 0;
        }
        if (t9InputMode == MODE_T9 && !t9Fallback) {
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

    if (t9InputMode == MODE_123 && isKeyHeld(key)) {
        return;
    }

    if (t9InputMode == MODE_123) {
        t9Text = t9Text.substring(0, t9Cursor) + String(key) + t9Text.substring(t9Cursor);
        t9Cursor++;
        return;
    }

    if (t9InputMode == MODE_ABC || (t9InputMode == MODE_T9 && t9Fallback)) {
        if (t9Fallback && key == '0') {
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
        return;
    }

    // MODE_T9 predictive
    if (key == '0') {
        if (t9TapKey == '1') t9CommitMultiTap();
        if (t9predict.hasInput()) t9CommitPrediction();
        if (t9ZeroPending && isKeyHeld('0')) {
            t9Text = t9Text.substring(0, t9Cursor) + "0" + t9Text.substring(t9Cursor);
            t9Cursor++;
            t9CursorMoveTime = millis();
            t9ZeroPending = false;
        } else if (!t9ZeroPending) {
            t9ZeroPending = true;
        }
        return;
    }

    if (key == '1') {
        if (t9predict.hasInput()) t9CommitPrediction();
        const char* map = multiTapMap[1];
        if (t9TapKey == '1') {
            t9TapIndex = (t9TapIndex + 1) % strlen(map);
        } else {
            t9TapKey = '1';
            t9TapIndex = 0;
        }
        t9TapTime = millis();
        return;
    }

    if (t9TapKey == '1') t9CommitMultiTap();
    t9predict.pushDigit(key);
    t9SingleKeyCycleIndex = 0;
    if (t9predict.getPrefixCandidateCount() == 0) {
        t9predict.popDigit();
        if (t9predict.hasInput()) t9CommitPrediction();
        t9Fallback = true;
        t9FallbackStart = t9Cursor;
        t9TapKey = key;
        t9TapIndex = 0;
        t9TapTime = millis();
    }
}

void SettingsApp::renderT9Editor() {
    const bool showHeader = (t9ViewMode == VIEW_FULL || t9ViewMode == VIEW_FULL_LINENO);
    const bool showFooter = (t9ViewMode == VIEW_FULL || t9ViewMode == VIEW_FULL_LINENO);
    const bool showLineNumbers = (t9ViewMode == VIEW_FULL_LINENO || t9ViewMode == VIEW_MIN_LINENO);
    const GUI::FontMetrics& metrics = GUI::getSystemFontMetrics();

    const char* modeStr = t9Fallback ? "?ABC" :
                          (t9InputMode == MODE_T9) ? "T9" :
                          (t9InputMode == MODE_ABC) ? "ABC" : "123";
    const char* shiftStr = (t9ShiftMode == 2) ? "^^" :
                           (t9ShiftMode == 1) ? "^" : "";
    if (showHeader) {
        char headerRight[16];
        snprintf(headerRight, sizeof(headerRight), "%s%s", shiftStr, modeStr);
        GUI::drawHeader("T9 EDITOR", headerRight);
    }

    GUI::setFontSystem();

    String displayText = t9Text;
    int previewInsertPos = t9Cursor;
    String preview = "";

    if (t9ZeroPending) {
        preview = " ";
    } else if (t9InputMode == MODE_T9 && t9TapKey == '1') {
        preview = t9GetMultiTapChar();
    } else if ((t9InputMode == MODE_ABC || t9Fallback) && t9TapKey != '\0') {
        preview = t9GetMultiTapChar();
    } else if (t9InputMode == MODE_T9 && t9predict.hasInput()) {
        int dc = t9predict.getDigitCount();
        if (dc == 1) {
            const char* digs = t9predict.getDigits();
            char digit = (digs && digs[0]) ? digs[0] : '\0';
            int letterCount = t9predict.getSingleKeyLetterCount(digit);
            int dictCount = t9predict.getPrefixCandidateCount();
            int total = letterCount + dictCount;
            if (total > 0) {
                int idx = t9SingleKeyCycleIndex % total;
                if (idx < letterCount) {
                    const char* c = t9predict.getSingleKeyLetter(digit, idx);
                    if (c) preview = c;
                } else {
                    const char* word = t9predict.getPrefixCandidate(idx - letterCount);
                    if (word) {
                        preview = word;
                        if ((int)preview.length() > dc) preview = preview.substring(0, dc);
                    }
                }
            }
        } else {
            const char* word = t9predict.getSelectedPrefixWord();
            if (word) {
                preview = word;
                if ((int)preview.length() > dc) preview = preview.substring(0, dc);
            }
        }
        if (preview.length() == 0) preview = t9predict.getDigits();

        if (t9ShiftMode == 1 && preview.length() > 0) {
            preview[0] = toupper(preview[0]);
        } else if (t9ShiftMode == 2) {
            for (int i = 0; i < (int)preview.length(); i++) {
                if (preview[i] >= 'a' && preview[i] <= 'z') preview[i] -= 32;
            }
        }
    }

    if (preview.length() > 0) {
        displayText = displayText.substring(0, previewInsertPos) + preview + displayText.substring(previewInsertPos);
    }

    int lineHeight = metrics.lineHeight;
    int textTop = showHeader ? GUI::getContentAreaTop() : 0;
    int textBottom = showFooter ? GUI::getContentBottom() : (GUI::SCREEN_HEIGHT - 1);
    int maxVisibleLines = (textBottom - textTop + 1) / lineHeight;
    if (maxVisibleLines < 1) maxVisibleLines = 1;
    int textX = showLineNumbers ? 12 : 1;
    int charWidth = u8g2.getStrWidth("W");
    if (charWidth < 1) charWidth = 1;
    int maxCharsPerLine = (GUI::SCREEN_WIDTH - textX - 1) / charWidth;
    if (maxCharsPerLine < 1) maxCharsPerLine = 1;
    int textLen = displayText.length();

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
        if (textLen == 0) break;
        if (pos == textLen && lineEnd == textLen && displayText[textLen - 1] != '\n') break;
    }
    if (totalLines == 0) totalLines = 1;

    int cursorLine = 0;
    for (int i = 0; i < totalLines; i++) {
        if (t9Cursor >= lineStarts[i] && t9Cursor <= lineEnds[i]) {
            cursorLine = i;
            break;
        }
    }

    if (cursorLine < t9ScrollOffset) t9ScrollOffset = cursorLine;
    if (cursorLine >= t9ScrollOffset + maxVisibleLines) t9ScrollOffset = cursorLine - maxVisibleLines + 1;

    int fbDispStart = -1, fbDispEnd = -1;
    if (t9Fallback) {
        fbDispStart = t9FallbackStart;
        fbDispEnd = t9Cursor + (int)preview.length();
    }

    if (showLineNumbers) {
        u8g2.drawVLine(10, textTop, textBottom - textTop + 1);
    }

    int y = textTop + metrics.baselineOffset;
    int cursorScreenX = -1, cursorScreenY = -1;
    for (int i = 0; i < maxVisibleLines; i++) {
        int li = t9ScrollOffset + i;
        if (li >= totalLines) break;
        int lStart = lineStarts[li];
        int lEnd = lineEnds[li];
        String line = displayText.substring(lStart, lEnd);

        if (showLineNumbers) {
            char ln[5];
            snprintf(ln, sizeof(ln), "%d", li + 1);
            u8g2.drawStr(1, y, ln);
        }

        if (fbDispStart >= 0 && fbDispStart < lEnd && fbDispEnd > lStart) {
            int regionS = max(fbDispStart, lStart) - lStart;
            int regionE = min(fbDispEnd, lEnd) - lStart;

            if (regionS > 0) {
                String before = line.substring(0, regionS);
                u8g2.drawStr(textX, y, before.c_str());
            }

            String fbPart = line.substring(regionS, regionE);
            int xFb = textX + u8g2.getStrWidth(line.substring(0, regionS).c_str());
            int wFb = u8g2.getStrWidth(fbPart.c_str());
            u8g2.drawBox(xFb, GUI::getHighlightTop(y), wFb, GUI::getHighlightHeight());
            u8g2.setDrawColor(0);
            u8g2.drawStr(xFb, y, fbPart.c_str());
            u8g2.setDrawColor(1);

            if (regionE < (int)line.length()) {
                String after = line.substring(regionE);
                u8g2.drawStr(xFb + wFb, y, after.c_str());
            }
        } else {
            u8g2.drawStr(textX, y, line.c_str());
        }

        if (li == cursorLine && cursorScreenX < 0) {
            int localIdx = t9Cursor - lineStarts[li];
            String before = displayText.substring(lineStarts[li], lineStarts[li] + localIdx);
            cursorScreenX = textX + u8g2.getStrWidth(before.c_str());
            cursorScreenY = y;
        }
        y += lineHeight;
    }

    if (cursorScreenX >= 0) {
        if (preview.length() > 0) {
            int pw = u8g2.getStrWidth(preview.c_str());
            u8g2.drawHLine(cursorScreenX, cursorScreenY + 2, pw);
        } else {
            bool recentMove = (millis() - t9CursorMoveTime < CURSOR_BLINK_RATE);
            if (recentMove || (millis() / CURSOR_BLINK_RATE) % 2) {
                u8g2.drawVLine(cursorScreenX, cursorScreenY - metrics.glyphTopOffset, metrics.cursorHeight);
            }
        }
    }

    if (showFooter) {
        const int footerBaselineY = GUI::getFooterBaselineY();
        u8g2.drawHLine(0, GUI::getFooterSeparatorY(), GUI::SCREEN_WIDTH);
        GUI::setFontSystem();
        if (t9Fallback && t9TapKey != '\0') {
            const char* map = multiTapMap[t9TapKey - '0'];
            char bar[32];
            snprintf(bar, sizeof(bar), "?[%s] %d/%d", map, t9TapIndex + 1, (int)strlen(map));
            String text = GUI::truncateStringToWidth(String(bar), GUI::SCREEN_WIDTH - 2);
            u8g2.drawUTF8(1, footerBaselineY, text.c_str());
        } else if (t9Fallback) {
            u8g2.drawStr(1, footerBaselineY, "?ABC 0:sp exits");
        } else if (t9InputMode == MODE_T9 && t9predict.hasInput()) {
            char bar[40];
            int dc = t9predict.getDigitCount();
            if (dc == 1) {
                const char* digs = t9predict.getDigits();
                char digit = (digs && digs[0]) ? digs[0] : '\0';
                int letterCount = t9predict.getSingleKeyLetterCount(digit);
                int dictCount = t9predict.getPrefixCandidateCount();
                int total = letterCount + dictCount;
                if (total > 0) {
                    int idx = t9SingleKeyCycleIndex % total;
                    if (idx < letterCount) {
                        const char* c = t9predict.getSingleKeyLetter(digit, idx);
                        snprintf(bar, sizeof(bar), "%s %d/%d", c ? c : "?", idx + 1, total);
                    } else {
                        const char* w = t9predict.getPrefixCandidate(idx - letterCount);
                        snprintf(bar, sizeof(bar), "%s %d/%d", w ? w : "?", idx + 1, total);
                    }
                } else {
                    snprintf(bar, sizeof(bar), "? [%s]", t9predict.getDigits());
                }
            } else {
                const char* word = t9predict.getSelectedPrefixWord();
                if (word) {
                    snprintf(bar, sizeof(bar), "%s %d/%d", word,
                             t9predict.getSelectedPrefixIndex() + 1,
                             t9predict.getPrefixCandidateCount());
                } else {
                    snprintf(bar, sizeof(bar), "? [%s]", t9predict.getDigits());
                }
            }
            String text = GUI::truncateStringToWidth(String(bar), GUI::SCREEN_WIDTH - 2);
            u8g2.drawUTF8(1, footerBaselineY, text.c_str());
        } else if (t9InputMode == MODE_T9 && t9TapKey == '1') {
            drawHighlightedChoiceBar(1, footerBaselineY, multiTapMap[1], t9TapIndex);
        } else if (t9InputMode == MODE_ABC && t9TapKey != '\0') {
            const char* map = multiTapMap[t9TapKey - '0'];
            char bar[32];
            snprintf(bar, sizeof(bar), "[%s] %d/%d", map, t9TapIndex + 1, (int)strlen(map));
            String text = GUI::truncateStringToWidth(String(bar), GUI::SCREEN_WIDTH - 2);
            u8g2.drawUTF8(1, footerBaselineY, text.c_str());
        } else {
            const char* hint = (t9InputMode == MODE_T9)  ? "ALT:ABC 2-9:T9 0:sp" :
                               (t9InputMode == MODE_ABC) ? "ALT:123 0-9:tap" :
                                                           "ALT:T9 0-9:digits";
            String text = GUI::truncateStringToWidth(String(hint), GUI::SCREEN_WIDTH - 2);
            u8g2.drawUTF8(1, footerBaselineY, text.c_str());
        }
    }

    if (t9SavePromptActive) {
        renderT9SavePrompt();
    }
}
