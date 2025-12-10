// [Revision: v4.1] [Path: src/apps/t9_editor.cpp] [Date: 2025-12-10]
// Description: Fixed cursor ghosting/hiding, navigation alignment, and exit logic.

#include "t9_editor.h"
#include "../app_transfer.h"
#include "../app_control.h"

const char* HELP_TEXT =
    "CONTROLS:\n"
    "2-9: Type\n"
    "#: Space\n"
    "*: Shift\n"
    "Z: Newline\n"
    "M: Backspace\n"
    "5: Confirm (when prompted)\n"
    "\n"
    "NAV:\n"
    "A/X: Left/Right\n"
    "B/Y: Up/Down\n"
    "\n"
    "SYSTEM:\n"
    "C: Help\n"
    "D: Exit\n";

T9EditorApp::T9EditorApp() {
    scrollOffset = 0;
    currentState = STATE_EDITING;
    fileName = "Unnamed"; // Default as requested
    helpScrollY = 0;
    exitRequested = false;
    readOnly = false;
}

void T9EditorApp::start() {
    u8g2.setContrast(systemContrast);
    currentState = STATE_EDITING;
    exitRequested = false;
    // If invoked as a transfer target, preload buffer or filename
    readOnly = false;
    if (appTransferAction == ACTION_EDIT_FILE) {
        // Editing file contents (read-write)
        engine.textBuffer = appTransferString;
        fileName = appTransferPath;
        readOnly = false;
    } else if (appTransferAction == ACTION_VIEW_FILE) {
        // View-only mode: load contents but disable typing
        engine.textBuffer = appTransferString;
        fileName = appTransferPath;
        readOnly = true;
    } else if (appTransferAction == ACTION_CREATE_FILE) {
        // Create new file: start with empty buffer, read-write
        engine.textBuffer = appTransferString; // likely empty
        fileName = "New File";
        readOnly = false;
    } else if (appTransferAction == ACTION_REQUEST_FILENAME || appTransferAction == ACTION_CREATE_FOLDER || appTransferAction == ACTION_RENAME) {
        // Editing a single-line name (not used in simplified SPIFFS mode)
        engine.textBuffer = appTransferString;
        fileName = "Enter name";
    }
    recalculateLayout();
}

void T9EditorApp::stop() {
    visualLines.clear();
    // Clear the engine buffer when closing the editor (per request)
    engine.reset();
}

void T9EditorApp::handleInput(char key) {
    // Exit confirmation is handled by shared Yes/No prompt app now.

    // --- 2. HELP POPUP HANDLING ---
    if (currentState == STATE_HELP) {
        if (key == 'C') currentState = STATE_EDITING; 
        if (key == '2') helpScrollY += 5; 
        if (key == '8') helpScrollY -= 5; 
        if (helpScrollY > 0) helpScrollY = 0; 
        return;
    }

    // --- 3. EDITOR HANDLING ---
    if (key == 'C') {
        currentState = STATE_HELP;
        helpScrollY = 0;
        return;
    }
    
    if (key == 'D') {
        // Signal exit requested. If this editor was opened as a filename/content
        // transfer target, the caller will be returned to by main loop.
        // Save buffer to transfer area so the caller can read it.
        appTransferString = engine.textBuffer;
        exitRequested = true;
        return;
    }

    if (key == 'A') { engine.moveCursor(-1); return; }
    if (key == 'X') { engine.moveCursor(1); return; }
    if (key == 'B') { moveCursorVertically(-1); return; }
    if (key == 'Y') { moveCursorVertically(1); return; }

    // If in read-only mode, ignore typing keys and only allow navigation and exit
    if (readOnly) {
        return;
    }

    engine.handleInput(key);
}

void T9EditorApp::moveCursorVertically(int dir) {
    if (engine.pendingCommit) engine.commit();
    recalculateLayout(); 
    
    // 1. Find cursor line
    int currentLineIdx = -1;
    int cursorRelPos = 0;
    
    for(int i=0; i<visualLines.size(); i++) {
        if (visualLines[i].hasCursor) {
            currentLineIdx = i;
            cursorRelPos = engine.cursorPos - visualLines[i].byteStartIndex;
            break;
        }
    }

    if (currentLineIdx == -1) return;

    // 2. Target Line
    int targetLineIdx = currentLineIdx + dir;
    if (targetLineIdx < 0) targetLineIdx = 0;
    if (targetLineIdx >= visualLines.size()) targetLineIdx = visualLines.size() - 1;
    
    if (targetLineIdx == currentLineIdx) return;

    // 3. Map Offset
    VisualLine& target = visualLines[targetLineIdx];
    int newRelPos = cursorRelPos;
    if (newRelPos > target.byteLength) newRelPos = target.byteLength;
    
    // 4. Align to UTF-8 Boundary (Fixes weird navigation)
    int tentativePos = target.byteStartIndex + newRelPos;
    
    // Safety clamp
    if (tentativePos > engine.textBuffer.length()) tentativePos = engine.textBuffer.length();
    
    // Backtrack if we landed in the middle of a multibyte sequence (0b10xxxxxx)
    while (tentativePos > 0 && (engine.textBuffer[tentativePos] & 0xC0) == 0x80) {
        tentativePos--;
    }
    
    engine.setCursor(tentativePos);
}

void T9EditorApp::update() {
    if (currentState == STATE_EDITING) {
        engine.update();
        
        String currentCandidate = engine.pendingCommit ? engine.getCurrentChar() : "";
        if (engine.textBuffer != lastProcessedText || 
            engine.pendingCommit != lastPendingState ||
            currentCandidate != lastCandidate ||
            engine.cursorPos != lastCursorPos) {
            
            recalculateLayout();
            
            lastProcessedText = engine.textBuffer;
            lastPendingState = engine.pendingCommit;
            lastCandidate = currentCandidate;
            lastCursorPos = engine.cursorPos;
        }
    }
}

void T9EditorApp::recalculateLayout() {
    visualLines.clear();

    String fullText = engine.textBuffer;
    String displayText = fullText;
    if (engine.pendingCommit) {
        String c = engine.getCurrentChar();
        displayText = fullText.substring(0, engine.cursorPos) + c + fullText.substring(engine.cursorPos);
    }

    u8g2.setFont(u8g2_font_5x8_tr);

    int currentLogicalLine = 1;
    int start = 0;
    
    while (start <= displayText.length()) {
        int end = displayText.indexOf('\n', start);
        if (end == -1) end = displayText.length();

        String segment = displayText.substring(start, end);
        int segmentLen = segment.length();
        
        if (segmentLen == 0) {
             // Empty line case
             VisualLine vl;
             vl.content = "";
             vl.logicalLineNum = currentLogicalLine;
             vl.byteStartIndex = start;
             vl.byteLength = 0;
             vl.hasCursor = (engine.cursorPos == start);
             visualLines.push_back(vl);
        } else {
            int segStart = 0;
            while (segStart < segmentLen) {
                int fitLen = 0;
                while (segStart + fitLen < segmentLen) {
                    String sub = segment.substring(segStart, segStart + fitLen + 1);
                    if (u8g2.getUTF8Width(sub.c_str()) > TEXT_AREA_WIDTH) break;
                    fitLen++;
                }
                if (fitLen == 0 && segStart < segmentLen) fitLen = 1;

                VisualLine vl;
                vl.content = segment.substring(segStart, segStart + fitLen);
                vl.logicalLineNum = (segStart == 0) ? currentLogicalLine : -1;
                vl.byteStartIndex = start + segStart;
                vl.byteLength = fitLen;
                
                // --- FIXED CURSOR LOGIC ---
                // Prevents double cursors on wrap and invisible cursors at newlines.
                int absStart = start + segStart;
                int absEnd = absStart + fitLen;
                bool isSegmentEnd = (segStart + fitLen == segmentLen);

                // Normal Case: Cursor inside the line
                if (engine.cursorPos >= absStart && engine.cursorPos < absEnd) {
                    vl.hasCursor = true;
                }
                // Wrap Case: Cursor at end of wrapped line belongs to NEXT line, so we ignore it here.
                // Newline/EOF Case: Cursor at very end of segment belongs to THIS line.
                else if (isSegmentEnd && engine.cursorPos == absEnd) {
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
    
    // Auto-Scroll
    int cursorLineIndex = 0;
    for(int i=0; i<visualLines.size(); i++) {
        if (visualLines[i].hasCursor) {
            cursorLineIndex = i;
            break;
        }
    }
    if (cursorLineIndex < scrollOffset) scrollOffset = cursorLineIndex;
    else if (cursorLineIndex >= scrollOffset + VISIBLE_LINES) scrollOffset = cursorLineIndex - VISIBLE_LINES + 1;
}

void T9EditorApp::renderHeader() {
    u8g2.setFont(FONT_SMALL);
    u8g2.drawBox(0, 0, 128, 11);
    u8g2.setDrawColor(0);
    
    String dispName = fileName;
    // Reserve space on the right for the RW/RO indicator and C-HELP label
    if (dispName.length() > 14) dispName = dispName.substring(0, 11) + "...";
    u8g2.drawStr(2, 9, dispName.c_str());

    // Draw read-only indicator moved right to avoid overlapping long filenames
    if (readOnly) {
        u8g2.drawStr(88, 9, "RO");
    }
    // Help label further to the right
    u8g2.drawStr(100, 9, "C-HELP");
    u8g2.setDrawColor(1);
}

void T9EditorApp::renderHelpPopup() {
    u8g2.setDrawColor(0);
    u8g2.drawBox(10, 10, 108, 44);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(10, 10, 108, 44);
    
    int x = 14;
    int y = 20 + helpScrollY;
    int lineH = 9;
    String h = HELP_TEXT;
    int start = 0;
    while(start < h.length()) {
        int end = h.indexOf('\n', start);
        if (end == -1) end = h.length();
        String line = h.substring(start, end);
        
        if (y > 10 && y < 54) {
             u8g2.drawStr(x, y, line.c_str());
        }
        y += lineH;
        start = end + 1;
    }
}

// exit popup removed; use shared yes/no prompt app

void T9EditorApp::render() {
    renderHeader();

    if (currentState == STATE_HELP) {
        renderHelpPopup();
        return;
    }

    u8g2.drawVLine(GUTTER_WIDTH, 12, 52);
    u8g2.setFont(u8g2_font_5x8_tr); 
    
    int yStart = HEADER_HEIGHT + LINE_HEIGHT; 
    
    for(int i=0; i<VISIBLE_LINES; i++) {
        int idx = scrollOffset + i;
        if (idx >= visualLines.size()) break;

        VisualLine& vl = visualLines[idx];
        int y = yStart + (i * LINE_HEIGHT);

        if (vl.logicalLineNum != -1) {
            // Use the same small font for line numbers so all text is consistent
            u8g2.setFont(u8g2_font_5x8_tr);
            u8g2.setCursor(1, y - 1);
            u8g2.print(vl.logicalLineNum);
            u8g2.setFont(u8g2_font_5x8_tr);
        }

        // Ensure content is rendered with the small font as well
        u8g2.setFont(u8g2_font_5x8_tr);
        u8g2.drawUTF8(GUTTER_WIDTH + 2, y, vl.content.c_str());

        if (vl.hasCursor) {
            int localCursorIdx = engine.cursorPos - vl.byteStartIndex;
            if (localCursorIdx < 0) localCursorIdx = 0;
            if (localCursorIdx > vl.content.length()) localCursorIdx = vl.content.length();
            
            String preCursor = vl.content.substring(0, localCursorIdx);
            int cursorX = GUTTER_WIDTH + 2 + u8g2.getUTF8Width(preCursor.c_str());
            
            if (engine.pendingCommit) {
                String c = engine.getCurrentChar();
                int charW = u8g2.getUTF8Width(c.c_str());
                u8g2.drawHLine(cursorX, y+2, charW);
            } else {
                if ((millis() / CURSOR_BLINK_RATE) % 2) {
                    u8g2.drawVLine(cursorX, y - 10, 10);
                }
            }
        }
    }
}