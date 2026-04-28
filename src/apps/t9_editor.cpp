// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/apps/t9_editor.cpp
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TACTICAL_TODO TASK_2
// LOG_REF: 2026-04-27
// Description: Canonical T9 editor/viewer app with predictive input, view cycling, and RO/RW modes.

#include "t9_editor.h"
#include "../app_transfer.h"
#include "../gui.h"

static const char* multiTapMap[] = {
    " 0",
    ",.=:()[]{}+-*/_?!1<>;\"'`%#\\|&$",
    "abc2",
    "def3",
    "ghi4",
    "jkl5",
    "mno6",
    "pqrs7",
    "tuv8",
    "wxyz9"
};

static char getMatchingBracket(char left) {
    switch (left) {
        case '(': return ')';
        case '[': return ']';
        case '{': return '}';
        case '<': return '>';
        default:  return '\0';
    }
}

static void insertCharWithAutoBracket(String& text, int& cursor, char c) {
    char right = getMatchingBracket(c);
    if (right != '\0') {
        String pair = String(c) + String(right);
        text = text.substring(0, cursor) + pair + text.substring(cursor);
        cursor += 1;
        return;
    }
    text = text.substring(0, cursor) + String(c) + text.substring(cursor);
    cursor += 1;
}

static void drawHighlightedChoiceBar(int xStart, int baselineY, const char* choices, int selectedIndex) {
    if (!choices) return;

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
    while (widthToSelected > 56 && startIndex < selectedIndex) {
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
        if (x + paddedWidth > 122) {
            clippedRight = (i < safeCount);
            break;
        }

        if (i == selectedIndex) {
            u8g2.drawBox(x - 1, baselineY - 7, paddedWidth + 1, 9);
            u8g2.setDrawColor(0);
            u8g2.drawStr(x, baselineY, item);
            u8g2.setDrawColor(1);
        } else {
            u8g2.drawStr(x, baselineY, item);
        }
        x += paddedWidth;
    }

    if (clippedRight) {
        u8g2.drawStr(123, baselineY, ">");
    }
}

static bool isWrapBoundaryChar(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (uc <= ' ') return true;
    if ((uc >= '0' && uc <= '9') ||
        (uc >= 'A' && uc <= 'Z') ||
        (uc >= 'a' && uc <= 'z') ||
        uc == '_') {
        return false;
    }
    if (uc >= 0x80) return false;
    return true;
}

static int getWrappedFitLength(const String& segment, int segStart, int segmentLen, int textAreaWidth) {
    int fitLen = 0;
    int lastBoundaryLen = -1;

    while (segStart + fitLen < segmentLen) {
        String sub = segment.substring(segStart, segStart + fitLen + 1);
        if (u8g2.getUTF8Width(sub.c_str()) > textAreaWidth) break;
        fitLen++;

        if (isWrapBoundaryChar(segment[segStart + fitLen - 1])) {
            lastBoundaryLen = fitLen;
        }
    }

    if (fitLen == 0 && segStart < segmentLen) return 1;

    if (segStart + fitLen < segmentLen && lastBoundaryLen > 0) {
        return lastBoundaryLen;
    }

    return fitLen;
}

T9EditorApp::T9EditorApp() {
    scrollOffset = 0;
    openMode = OPEN_READ_WRITE;
    sourceKind = SOURCE_BUFFER;
    sourcePageSize = 256;
    documentLabel = "T9 EDITOR";
    documentPath = "";
    documentBuffer = "";
    exitRequested = false;
    resetEditorSession();
}

void T9EditorApp::start() {
    u8g2.setContrast(systemContrast);
    exitRequested = false;
    openMode = (appTransferEditorMode == APP_TRANSFER_EDITOR_READ_ONLY || appTransferAction == ACTION_VIEW_FILE)
               ? OPEN_READ_ONLY
               : OPEN_READ_WRITE;
    sourceKind = (appTransferSourceKind == APP_TRANSFER_SOURCE_PAGED_PLACEHOLDER)
                 ? SOURCE_PAGED_PLACEHOLDER
                 : SOURCE_BUFFER;
    documentPath = appTransferPath;
    documentBuffer = appTransferString;
    documentLabel = appTransferLabel;
    if (documentLabel.length() == 0) {
        if (appTransferAction == ACTION_CREATE_FILE) {
            documentLabel = "New File";
        } else if (appTransferAction == ACTION_REQUEST_FILENAME ||
                   appTransferAction == ACTION_CREATE_FOLDER ||
                   appTransferAction == ACTION_RENAME) {
            documentLabel = "Enter name";
        } else if (documentPath.length() > 0) {
            documentLabel = documentPath;
        } else if (isReadOnly()) {
            documentLabel = "VIEWER";
        } else {
            documentLabel = "T9 EDITOR";
        }
    }
    resetEditorSession();
    recalculateLayout();
}

void T9EditorApp::stop() {
    visualLines.clear();
    documentBuffer = "";
    documentLabel = "T9 EDITOR";
    documentPath = "";
    exitRequested = false;
}

void T9EditorApp::resetEditorSession() {
    t9predict.reset();
    cursorPos = 0;
    shiftMode = 0;
    inputMode = MODE_T9;
    tapKey = '\0';
    tapIndex = 0;
    tapTime = 0;
    cursorMoveTime = 0;
    viewMode = VIEW_FULL;
    savePromptActive = false;
    savePromptSelection = 0;
    singleKeyCycleIndex = 0;
    zeroPending = false;
    fallback = false;
    fallbackStart = 0;
    scrollOffset = 0;
}

String T9EditorApp::readDocumentSlice(int start, int length) const {
    if (length <= 0 || start < 0) return "";
    int docLen = getDocumentLength();
    if (start >= docLen) return "";
    int end = start + length;
    if (end > docLen) end = docLen;
    return documentBuffer.substring(start, end);
}

int T9EditorApp::getDocumentLength() const {
    return documentBuffer.length();
}

bool T9EditorApp::isReadOnly() const {
    return openMode == OPEN_READ_ONLY;
}

bool T9EditorApp::showHeader() const {
    return viewMode == VIEW_FULL || viewMode == VIEW_FULL_LINENO;
}

bool T9EditorApp::showFooter() const {
    return viewMode == VIEW_FULL || viewMode == VIEW_FULL_LINENO;
}

bool T9EditorApp::showLineNumbers() const {
    return viewMode == VIEW_FULL_LINENO || viewMode == VIEW_MIN_LINENO;
}

int T9EditorApp::getVisibleLineCount() const {
    int textTop = showHeader() ? 13 : 1;
    int textBottom = showFooter() ? 53 : 63;
    int visible = (textBottom - textTop + 1) / 9;
    return max(1, visible);
}

int T9EditorApp::countLogicalLines() const {
    int count = 1;
    for (int i = 0; i < getDocumentLength(); i++) {
        if (documentBuffer[i] == '\n') count++;
    }
    return count;
}

int T9EditorApp::getGutterWidth(int logicalLineCount) const {
    if (!showLineNumbers()) return 0;
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", max(1, logicalLineCount));
    u8g2.setFont(u8g2_font_5x8_tr);
    return u8g2.getStrWidth(buf) + 4;
}

int T9EditorApp::getTextLeft(int logicalLineCount) const {
    return showLineNumbers() ? getGutterWidth(logicalLineCount) + 2 : 1;
}

void T9EditorApp::requestExit(bool saveRequested) {
    if (saveRequested) {
        Serial.println("[T9] Save stub: requested, no filesystem write performed");
    }
    appTransferString = documentBuffer;
    appTransferBool = saveRequested;
    exitRequested = true;
    savePromptActive = false;
}

String T9EditorApp::getMultiTapChar() const {
    if (tapKey < '0' || tapKey > '9') return "";
    const char* map = multiTapMap[tapKey - '0'];
    int len = strlen(map);
    if (tapIndex >= len) return "";
    char c = map[tapIndex];
    if (shiftMode >= 1 && c >= 'a' && c <= 'z') c -= 32;
    return String(c);
}

String T9EditorApp::getPreviewText() const {
    String preview = "";

    if (zeroPending) {
        preview = " ";
    } else if (inputMode == MODE_T9 && tapKey == '1') {
        preview = getMultiTapChar();
    } else if ((inputMode == MODE_ABC || fallback) && tapKey != '\0') {
        preview = getMultiTapChar();
    } else if (inputMode == MODE_T9 && t9predict.hasInput()) {
        int dc = t9predict.getDigitCount();
        if (dc == 1) {
            const char* digs = t9predict.getDigits();
            char digit = (digs && digs[0]) ? digs[0] : '\0';
            int letterCount = t9predict.getSingleKeyLetterCount(digit);
            int dictCount = t9predict.getPrefixCandidateCount();
            int total = letterCount + dictCount;
            if (total > 0) {
                int idx = singleKeyCycleIndex % total;
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
    }

    if (shiftMode == 1 && preview.length() > 0) {
        preview[0] = toupper(preview[0]);
    } else if (shiftMode == 2) {
        for (int i = 0; i < (int)preview.length(); i++) {
            if (preview[i] >= 'a' && preview[i] <= 'z') preview[i] -= 32;
        }
    }

    return preview;
}

String T9EditorApp::getDisplayText(String* previewOut) const {
    String preview = getPreviewText();
    if (previewOut) *previewOut = preview;
    if (preview.length() == 0) return documentBuffer;
    return documentBuffer.substring(0, cursorPos) + preview + documentBuffer.substring(cursorPos);
}

void T9EditorApp::commitMultiTap() {
    if (tapKey == '\0') return;
    String c = getMultiTapChar();
    if (c.length() > 0) {
        insertCharWithAutoBracket(documentBuffer, cursorPos, c[0]);
    }
    tapKey = '\0';
    tapIndex = 0;
    tapTime = 0;
}

void T9EditorApp::commitPrediction() {
    int dc = t9predict.getDigitCount();
    String commitText = "";

    if (dc == 1) {
        const char* digs = t9predict.getDigits();
        char digit = (digs && digs[0]) ? digs[0] : '\0';
        int letterCount = t9predict.getSingleKeyLetterCount(digit);
        int dictCount = t9predict.getPrefixCandidateCount();
        int total = letterCount + dictCount;
        if (total > 0) {
            int idx = singleKeyCycleIndex % total;
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
        const char* digs = t9predict.getDigits();
        if (digs && dc > 0) {
            String d = digs;
            documentBuffer = documentBuffer.substring(0, cursorPos) + d + documentBuffer.substring(cursorPos);
            cursorPos += d.length();
        }
        t9predict.reset();
        singleKeyCycleIndex = 0;
        return;
    }

    if (shiftMode == 1 && commitText.length() > 0) {
        commitText[0] = toupper(commitText[0]);
    } else if (shiftMode == 2) {
        for (int i = 0; i < (int)commitText.length(); i++) {
            if (commitText[i] >= 'a' && commitText[i] <= 'z') commitText[i] -= 32;
        }
    }

    if (commitText.length() == 1) {
        insertCharWithAutoBracket(documentBuffer, cursorPos, commitText[0]);
    } else {
        documentBuffer = documentBuffer.substring(0, cursorPos) + commitText + documentBuffer.substring(cursorPos);
        cursorPos += commitText.length();
    }
    t9predict.reset();
    singleKeyCycleIndex = 0;
    if (shiftMode == 1) shiftMode = 0;
}

void T9EditorApp::moveCursorVertically(int dir) {
    int lineStart = 0;
    for (int i = cursorPos - 1; i >= 0; i--) {
        if (documentBuffer[i] == '\n') {
            lineStart = i + 1;
            break;
        }
    }
    int col = cursorPos - lineStart;

    if (dir < 0) {
        if (lineStart == 0) return;
        int prevLineEnd = lineStart - 1;
        int prevLineStart = 0;
        for (int i = prevLineEnd - 1; i >= 0; i--) {
            if (documentBuffer[i] == '\n') {
                prevLineStart = i + 1;
                break;
            }
        }
        int prevLineLen = prevLineEnd - prevLineStart;
        cursorPos = prevLineStart + min(col, prevLineLen);
    } else {
        int nextLineStart = -1;
        for (int i = cursorPos; i < getDocumentLength(); i++) {
            if (documentBuffer[i] == '\n') {
                nextLineStart = i + 1;
                break;
            }
        }
        if (nextLineStart < 0) return;
        int nextLineEnd = getDocumentLength();
        for (int i = nextLineStart; i < getDocumentLength(); i++) {
            if (documentBuffer[i] == '\n') {
                nextLineEnd = i;
                break;
            }
        }
        int nextLineLen = nextLineEnd - nextLineStart;
        cursorPos = nextLineStart + min(col, nextLineLen);
    }
}

void T9EditorApp::handleSavePromptInput(char key) {
    if (key == KEY_LEFT || key == KEY_UP || key == KEY_ESC) {
        savePromptSelection = 0;
        return;
    }
    if (key == KEY_RIGHT || key == KEY_DOWN || key == KEY_TAB) {
        savePromptSelection = 1;
        return;
    }
    if (key == KEY_ENTER) {
        requestExit(savePromptSelection == 1);
    }
}

void T9EditorApp::handleInput(char key) {
    if (savePromptActive) {
        handleSavePromptInput(key);
        return;
    }

    bool layoutChanged = false;

    if (!isReadOnly() && zeroPending && key != '0') {
        documentBuffer = documentBuffer.substring(0, cursorPos) + " " + documentBuffer.substring(cursorPos);
        cursorPos++;
        cursorMoveTime = millis();
        zeroPending = false;
        layoutChanged = true;
    }

    if (key == KEY_ESC) {
        if (isReadOnly()) {
            requestExit(false);
        } else {
            savePromptActive = true;
            savePromptSelection = 0;
        }
        return;
    }

    if (key == KEY_TAB) {
        if (viewMode == VIEW_FULL) viewMode = VIEW_FULL_LINENO;
        else if (viewMode == VIEW_FULL_LINENO) viewMode = VIEW_MIN_LINENO;
        else if (viewMode == VIEW_MIN_LINENO) viewMode = VIEW_MIN;
        else viewMode = VIEW_FULL;
        recalculateLayout();
        return;
    }

    if (!isReadOnly() && inputMode == MODE_T9 && tapKey == '1' &&
        (key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT || key == KEY_RIGHT)) {
        const char* map = multiTapMap[1];
        int count = strlen(map);
        if (count > 0) {
            if (key == KEY_UP || key == KEY_LEFT) tapIndex = (tapIndex - 1 + count) % count;
            else tapIndex = (tapIndex + 1) % count;
        }
        tapTime = millis();
        recalculateLayout();
        return;
    }

    if (key == KEY_UP || key == KEY_DOWN) {
        cursorMoveTime = millis();
        if (!isReadOnly() && inputMode == MODE_T9 && t9predict.hasInput() && !fallback) {
            if (t9predict.getDigitCount() == 1) {
                const char* digs = t9predict.getDigits();
                char digit = (digs && digs[0]) ? digs[0] : '\0';
                int letterCount = t9predict.getSingleKeyLetterCount(digit);
                int dictCount = t9predict.getPrefixCandidateCount();
                int total = letterCount + dictCount;
                if (total > 0) {
                    if (key == KEY_UP) singleKeyCycleIndex = (singleKeyCycleIndex - 1 + total) % total;
                    else singleKeyCycleIndex = (singleKeyCycleIndex + 1) % total;
                }
            } else {
                if (key == KEY_UP) t9predict.prevPrefixCandidate();
                else t9predict.nextPrefixCandidate();
            }
            recalculateLayout();
            return;
        }

        if (!isReadOnly()) {
            if ((inputMode == MODE_ABC || fallback) && tapKey != '\0') commitMultiTap();
            if (inputMode == MODE_T9 && t9predict.hasInput()) commitPrediction();
            fallback = false;
        }
        moveCursorVertically(key == KEY_UP ? -1 : 1);
        recalculateLayout();
        return;
    }

    if (key == KEY_LEFT || key == KEY_RIGHT) {
        cursorMoveTime = millis();
        if (!isReadOnly()) {
            if ((inputMode == MODE_ABC || fallback) && tapKey != '\0') commitMultiTap();
            if (inputMode == MODE_T9 && t9predict.hasInput()) commitPrediction();
            fallback = false;
        }
        if (key == KEY_LEFT && cursorPos > 0) cursorPos--;
        if (key == KEY_RIGHT && cursorPos < getDocumentLength()) cursorPos++;
        recalculateLayout();
        return;
    }

    if (isReadOnly()) {
        return;
    }

    if (key == KEY_ALT) {
        if ((inputMode == MODE_ABC || fallback) && tapKey != '\0') commitMultiTap();
        if (inputMode == MODE_T9 && t9predict.hasInput()) commitPrediction();
        fallback = false;
        singleKeyCycleIndex = 0;
        if (inputMode == MODE_T9) inputMode = MODE_ABC;
        else if (inputMode == MODE_ABC) inputMode = MODE_123;
        else inputMode = MODE_T9;
        t9predict.reset();
        tapKey = '\0';
        recalculateLayout();
        return;
    }

    if (key == KEY_SHIFT) {
        shiftMode = (shiftMode + 1) % 3;
        recalculateLayout();
        return;
    }

    if (key == KEY_BKSP) {
        cursorMoveTime = millis();
        if (inputMode == MODE_ABC || (inputMode == MODE_T9 && fallback)) {
            if (tapKey != '\0') {
                tapKey = '\0';
                tapIndex = 0;
            } else if (cursorPos > 0) {
                documentBuffer.remove(cursorPos - 1, 1);
                cursorPos--;
                if (fallback && cursorPos <= fallbackStart) {
                    fallback = false;
                }
            }
        } else if (inputMode == MODE_T9) {
            if (zeroPending) {
                zeroPending = false;
            } else if (tapKey == '1') {
                tapKey = '\0';
                tapIndex = 0;
                tapTime = 0;
            } else if (t9predict.hasInput()) {
                t9predict.popDigit();
                singleKeyCycleIndex = 0;
            } else if (cursorPos > 0) {
                documentBuffer.remove(cursorPos - 1, 1);
                cursorPos--;
            }
        } else if (cursorPos > 0) {
            documentBuffer.remove(cursorPos - 1, 1);
            cursorPos--;
        }
        recalculateLayout();
        return;
    }

    if (key == KEY_ENTER) {
        cursorMoveTime = millis();
        if (((inputMode == MODE_ABC || fallback) && tapKey != '\0') ||
            (inputMode == MODE_T9 && tapKey == '1')) {
            commitMultiTap();
        } else if (inputMode == MODE_T9 && t9predict.hasInput()) {
            commitPrediction();
        } else {
            documentBuffer = documentBuffer.substring(0, cursorPos) + "\n" + documentBuffer.substring(cursorPos);
            cursorPos++;
        }
        fallback = false;
        recalculateLayout();
        return;
    }

    if (key < '0' || key > '9') {
        if (layoutChanged) recalculateLayout();
        return;
    }

    if (key == '0' && inputMode != MODE_123) {
        if (inputMode == MODE_T9 && tapKey == '1') {
            commitMultiTap();
        }
        if (inputMode == MODE_T9 && t9predict.hasInput()) {
            commitPrediction();
        }
        if ((inputMode == MODE_ABC || fallback) && tapKey != '\0') {
            tapKey = '\0';
            tapIndex = 0;
            tapTime = 0;
        }

        if (zeroPending && isKeyHeld('0')) {
            documentBuffer = documentBuffer.substring(0, cursorPos) + "0" + documentBuffer.substring(cursorPos);
            cursorPos++;
            cursorMoveTime = millis();
            zeroPending = false;
            fallback = false;
        } else if (!zeroPending) {
            zeroPending = true;
        }
        recalculateLayout();
        return;
    }

    if ((inputMode == MODE_T9 || inputMode == MODE_ABC) && isKeyHeld(key)) {
        if (inputMode == MODE_T9 && key == '0') {
            return;
        }
        if (inputMode == MODE_T9 && tapKey == '1') {
            if (key == '1') {
                tapKey = '\0';
                tapIndex = 0;
                tapTime = 0;
            } else {
                commitMultiTap();
            }
        } else if ((inputMode == MODE_ABC || fallback) && tapKey != '\0') {
            tapKey = '\0';
            tapIndex = 0;
        }
        if (inputMode == MODE_T9 && !fallback) {
            if (t9predict.hasInput()) {
                t9predict.popDigit();
                if (t9predict.hasInput()) commitPrediction();
            }
        }
        documentBuffer = documentBuffer.substring(0, cursorPos) + String(key) + documentBuffer.substring(cursorPos);
        cursorPos++;
        cursorMoveTime = millis();
        recalculateLayout();
        return;
    }

    if (inputMode == MODE_123 && isKeyHeld(key)) {
        return;
    }

    if (inputMode == MODE_123) {
        documentBuffer = documentBuffer.substring(0, cursorPos) + String(key) + documentBuffer.substring(cursorPos);
        cursorPos++;
        recalculateLayout();
        return;
    }

    if (inputMode == MODE_ABC || (inputMode == MODE_T9 && fallback)) {
        if (fallback && key == '0') {
            if (tapKey != '\0') commitMultiTap();
            fallback = false;
            documentBuffer = documentBuffer.substring(0, cursorPos) + " " + documentBuffer.substring(cursorPos);
            cursorPos++;
            recalculateLayout();
            return;
        }
        unsigned long now = millis();
        if (key == tapKey && (now - tapTime < MULTITAP_TIMEOUT)) {
            const char* map = multiTapMap[key - '0'];
            tapIndex = (tapIndex + 1) % strlen(map);
        } else {
            if (tapKey != '\0') commitMultiTap();
            tapKey = key;
            tapIndex = 0;
        }
        tapTime = now;
        recalculateLayout();
        return;
    }

    if (key == '1') {
        if (t9predict.hasInput()) commitPrediction();
        const char* map = multiTapMap[1];
        if (tapKey == '1') {
            tapIndex = (tapIndex + 1) % strlen(map);
        } else {
            tapKey = '1';
            tapIndex = 0;
        }
        tapTime = millis();
        recalculateLayout();
        return;
    }

    if (tapKey == '1') commitMultiTap();
    t9predict.pushDigit(key);
    singleKeyCycleIndex = 0;
    if (t9predict.getPrefixCandidateCount() == 0) {
        t9predict.popDigit();
        if (t9predict.hasInput()) commitPrediction();
        fallback = true;
        fallbackStart = cursorPos;
        tapKey = key;
        tapIndex = 0;
        tapTime = millis();
    }
    recalculateLayout();
}

void T9EditorApp::update() {
    bool layoutChanged = false;

    if (!isReadOnly() && !savePromptActive && (inputMode == MODE_ABC || fallback) && tapKey != '\0') {
        if (millis() - tapTime > MULTITAP_TIMEOUT) {
            bool isSeparator = false;
            if (fallback) {
                String c = getMultiTapChar();
                if (c == "." || c == "?" || c == "!" || c == ",") isSeparator = true;
            }
            commitMultiTap();
            if (isSeparator) fallback = false;
            layoutChanged = true;
        }
    }

    if (!isReadOnly() && !savePromptActive && inputMode != MODE_123 && zeroPending) {
        bool zeroStillPressed = false;
        for (int i = 0; i < activeKeyCount; i++) {
            if (activeKeys[i] == '0') {
                zeroStillPressed = true;
                break;
            }
        }

        if (!zeroStillPressed) {
            documentBuffer = documentBuffer.substring(0, cursorPos) + " " + documentBuffer.substring(cursorPos);
            cursorPos++;
            cursorMoveTime = millis();
            zeroPending = false;
            layoutChanged = true;
        }
    }

    if (layoutChanged) {
        recalculateLayout();
    }
}

void T9EditorApp::recalculateLayout() {
    visualLines.clear();

    String preview;
    String displayText = getDisplayText(&preview);
    u8g2.setFont(u8g2_font_5x8_tr);

    int logicalLineCount = countLogicalLines();
    int textLeft = getTextLeft(logicalLineCount);
    int textAreaWidth = GUI::SCREEN_WIDTH - textLeft - 1;
    if (textAreaWidth < 1) textAreaWidth = 1;

    int currentLogicalLine = 1;
    int start = 0;
    while (start <= displayText.length()) {
        int end = displayText.indexOf('\n', start);
        if (end == -1) end = displayText.length();

        String segment = displayText.substring(start, end);
        int segmentLen = segment.length();
        if (segmentLen == 0) {
            VisualLine vl;
            vl.content = "";
            vl.logicalLineNum = currentLogicalLine;
            vl.byteStartIndex = start;
            vl.byteLength = 0;
            vl.hasCursor = (cursorPos == start);
            visualLines.push_back(vl);
        } else {
            int segStart = 0;
            while (segStart < segmentLen) {
                int fitLen = getWrappedFitLength(segment, segStart, segmentLen, textAreaWidth);

                VisualLine vl;
                vl.content = segment.substring(segStart, segStart + fitLen);
                vl.logicalLineNum = (segStart == 0) ? currentLogicalLine : -1;
                vl.byteStartIndex = start + segStart;
                vl.byteLength = fitLen;

                int absStart = start + segStart;
                int absEnd = absStart + fitLen;
                bool isSegmentEnd = (segStart + fitLen == segmentLen);
                if (cursorPos >= absStart && cursorPos < absEnd) {
                    vl.hasCursor = true;
                } else if (isSegmentEnd && cursorPos == absEnd) {
                    vl.hasCursor = true;
                } else {
                    vl.hasCursor = false;
                }

                visualLines.push_back(vl);
                segStart += fitLen;
            }
        }
        start = end + 1;
        currentLogicalLine++;
    }

    int cursorLineIndex = 0;
    for (int i = 0; i < (int)visualLines.size(); i++) {
        if (visualLines[i].hasCursor) {
            cursorLineIndex = i;
            break;
        }
    }

    int visibleLines = getVisibleLineCount();
    if (cursorLineIndex < scrollOffset) scrollOffset = cursorLineIndex;
    else if (cursorLineIndex >= scrollOffset + visibleLines) scrollOffset = cursorLineIndex - visibleLines + 1;
    if (scrollOffset < 0) scrollOffset = 0;
}

void T9EditorApp::renderHeader() {
    if (!showHeader()) return;

    const char* modeStr = fallback ? "?ABC" :
                          (inputMode == MODE_T9) ? "T9" :
                          (inputMode == MODE_ABC) ? "ABC" : "123";
    const char* shiftStr = (shiftMode == 2) ? "^^" :
                           (shiftMode == 1) ? "^" : "";
    char rightText[24];
    if (isReadOnly()) {
        snprintf(rightText, sizeof(rightText), "RO %s%s", shiftStr, modeStr);
    } else {
        snprintf(rightText, sizeof(rightText), "%s%s", shiftStr, modeStr);
    }

    String title = documentLabel.length() > 0 ? documentLabel : String("T9 EDITOR");
    if (title.length() > 14) title = title.substring(0, 11) + "...";
    GUI::drawHeader(title.c_str(), rightText);
}

void T9EditorApp::renderFooter() const {
    if (!showFooter()) return;

    u8g2.drawHLine(0, 54, 128);
    u8g2.setFont(u8g2_font_5x8_tr);
    if (isReadOnly()) {
        u8g2.drawStr(1, 62, "RO TAB:view ESC:back");
        return;
    }

    if (fallback && tapKey != '\0') {
        const char* map = multiTapMap[tapKey - '0'];
        char bar[32];
        snprintf(bar, sizeof(bar), "?[%s] %d/%d", map, tapIndex + 1, (int)strlen(map));
        u8g2.drawStr(1, 62, bar);
    } else if (fallback) {
        u8g2.drawStr(1, 62, "?ABC 0:sp exits");
    } else if (inputMode == MODE_T9 && t9predict.hasInput()) {
        char bar[40];
        int dc = t9predict.getDigitCount();
        if (dc == 1) {
            const char* digs = t9predict.getDigits();
            char digit = (digs && digs[0]) ? digs[0] : '\0';
            int letterCount = t9predict.getSingleKeyLetterCount(digit);
            int dictCount = t9predict.getPrefixCandidateCount();
            int total = letterCount + dictCount;
            if (total > 0) {
                int idx = singleKeyCycleIndex % total;
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
        u8g2.drawStr(1, 62, bar);
    } else if (inputMode == MODE_T9 && tapKey == '1') {
        drawHighlightedChoiceBar(1, 62, multiTapMap[1], tapIndex);
    } else if (inputMode == MODE_ABC && tapKey != '\0') {
        const char* map = multiTapMap[tapKey - '0'];
        char bar[32];
        snprintf(bar, sizeof(bar), "[%s] %d/%d", map, tapIndex + 1, (int)strlen(map));
        u8g2.drawStr(1, 62, bar);
    } else {
        const char* hint = (inputMode == MODE_T9)  ? "ALT:ABC 2-9:T9 0:sp" :
                           (inputMode == MODE_ABC) ? "ALT:123 0-9:tap" :
                                                     "ALT:T9 0-9:digits";
        u8g2.drawStr(1, 62, hint);
    }
}

void T9EditorApp::renderSavePrompt() {
    GUI::drawYesNoDialog("Save buffer?", savePromptSelection == 1);
}

void T9EditorApp::render() {
    renderHeader();

    String preview;
    getDisplayText(&preview);
    int logicalLineCount = countLogicalLines();
    int gutterWidth = getGutterWidth(logicalLineCount);
    int textLeft = getTextLeft(logicalLineCount);
    int textTop = showHeader() ? 13 : 1;
    int textBottom = showFooter() ? 53 : 63;
    int visibleLines = getVisibleLineCount();

    if (showLineNumbers()) {
        u8g2.drawVLine(gutterWidth, textTop, textBottom - textTop + 1);
    }

    int fbDispStart = -1;
    int fbDispEnd = -1;
    if (fallback) {
        fbDispStart = fallbackStart;
        fbDispEnd = cursorPos + (int)preview.length();
    }

    int y = textTop + 8;
    for (int i = 0; i < visibleLines; i++) {
        int idx = scrollOffset + i;
        if (idx >= (int)visualLines.size()) break;

        const VisualLine& vl = visualLines[idx];
        if (showLineNumbers() && vl.logicalLineNum != -1) {
            char ln[12];
            snprintf(ln, sizeof(ln), "%d", vl.logicalLineNum);
            u8g2.drawStr(1, y, ln);
        }

        if (fbDispStart >= 0 && fbDispStart < vl.byteStartIndex + vl.byteLength && fbDispEnd > vl.byteStartIndex) {
            int regionS = max(fbDispStart, vl.byteStartIndex) - vl.byteStartIndex;
            int regionE = min(fbDispEnd, vl.byteStartIndex + vl.byteLength) - vl.byteStartIndex;
            if (regionS > 0) {
                String before = vl.content.substring(0, regionS);
                u8g2.drawUTF8(textLeft, y, before.c_str());
            }

            String fbPart = vl.content.substring(regionS, regionE);
            int xFb = textLeft + u8g2.getUTF8Width(vl.content.substring(0, regionS).c_str());
            int wFb = u8g2.getUTF8Width(fbPart.c_str());
            u8g2.drawBox(xFb, y - 7, wFb, 9);
            u8g2.setDrawColor(0);
            u8g2.drawUTF8(xFb, y, fbPart.c_str());
            u8g2.setDrawColor(1);

            if (regionE < (int)vl.content.length()) {
                String after = vl.content.substring(regionE);
                u8g2.drawUTF8(xFb + wFb, y, after.c_str());
            }
        } else {
            u8g2.drawUTF8(textLeft, y, vl.content.c_str());
        }

        if (vl.hasCursor) {
            int localCursorIdx = cursorPos - vl.byteStartIndex;
            if (localCursorIdx < 0) localCursorIdx = 0;
            if (localCursorIdx > vl.byteLength) localCursorIdx = vl.byteLength;

            String before = vl.content.substring(0, localCursorIdx);
            int cursorX = textLeft + u8g2.getUTF8Width(before.c_str());
            if (preview.length() > 0) {
                int pw = u8g2.getUTF8Width(preview.c_str());
                u8g2.drawHLine(cursorX, y + 2, pw);
            } else {
                bool recentMove = (millis() - cursorMoveTime < CURSOR_BLINK_RATE);
                if (recentMove || (millis() / CURSOR_BLINK_RATE) % 2) {
                    u8g2.drawVLine(cursorX, y - 7, 8);
                }
            }
        }
        y += 9;
    }

    renderFooter();
    if (savePromptActive) renderSavePrompt();
}