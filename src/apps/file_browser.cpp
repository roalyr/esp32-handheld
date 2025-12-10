// [Revision: v1.1] [Path: src/apps/file_browser.cpp] [Date: 2025-12-10]
// Description: Simplified SPIFFS-only file browser: list, view (RO), delete (confirm).

#include "file_browser.h"
#include "../app_transfer.h"
#include "../app_control.h"
#include "yes_no_prompt.h"
#include "../apps/t9_editor.h"
#include <FS.h>
#include <SPIFFS.h>
#include <algorithm>

extern T9EditorApp appT9Editor;

FileBrowserApp::FileBrowserApp() {
    selectedIndex = 0;
    scrollOffset = 0;
    menuState = MENU_CLOSED;
    menuSelectedIndex = 0;
    currentPath = "/";
    spiffsAvailable = false;
    awaitingSaveConfirmation = false;
    saveMessage = "";
    saveMessageUntil = 0;
    sortMode = FileBrowserApp::SORT_NAME;
}

void FileBrowserApp::start() {
    u8g2.setContrast(systemContrast);

    // SPIFFS presence
    spiffsAvailable = SPIFFS.totalBytes() > 0;

    // Handle pending delete action returned from yes/no prompt
    if (appTransferAction == ACTION_DELETE_FILE) {
        if (appTransferBool && appTransferPath.length() > 0) {
            if (!SPIFFS.remove(appTransferPath)) {
                Serial.print("SPIFFS: failed to remove file: ");
                Serial.println(appTransferPath);
            }
        }
        appTransferAction = ACTION_NONE;
        appTransferPath = "";
        appTransferBool = false;
    }

    // Handle returned filename request (user entered a filename -> open editor to create)
    if (appTransferAction == ACTION_REQUEST_FILENAME) {
        String fname = appTransferString;
        // Clear the transfer string but if name provided, launch editor to create file
        appTransferString = "";
        if (fname.length() > 0) {
            // Compose a simple path in root
            appTransferPath = String("/") + fname;
            appTransferAction = ACTION_CREATE_FILE;
            appTransferCaller = this;
            appTransferString = ""; // empty content to start editing
            switchApp(&appT9Editor);
            return;
        } else {
            // Nothing entered; clear action
            appTransferAction = ACTION_NONE;
        }
    }

    // Handle returned edit/create from T9 editor: ask to save and write if confirmed
    if (appTransferAction == ACTION_EDIT_FILE || appTransferAction == ACTION_CREATE_FILE) {
        // If we haven't yet prompted the user, start the yes/no prompt
        if (!awaitingSaveConfirmation) {
            awaitingSaveConfirmation = true;
            appTransferCaller = this;
            yesNoPrompt.startPrompt(this, String("Save changes?"));
            switchApp(&yesNoPrompt);
            return;
        }

        // We have come back from the prompt; check user's choice
        if (appTransferBool) {
            // Write buffer to SPIFFS at appTransferPath
            if (appTransferPath.length() > 0) {
                File f = SPIFFS.open(appTransferPath, FILE_WRITE);
                if (!f) {
                    Serial.print("SPIFFS: failed to open for write: ");
                    Serial.println(appTransferPath);
                    saveMessage = "Save failed";
                    saveMessageUntil = millis() + 2000;
                } else {
                    f.print(appTransferString);
                    f.close();
                    saveMessage = "Saved";
                    saveMessageUntil = millis() + 2000;
                }
            } else {
                Serial.println("FileBrowser: no path provided for save");
                saveMessage = "Save failed";
                saveMessageUntil = millis() + 2000;
            }
        }

        // Clear transfer state and reset flag
        appTransferAction = ACTION_NONE;
        appTransferPath = "";
        appTransferString = "";
        appTransferBool = false;
        awaitingSaveConfirmation = false;
    }

    scanDirectory(currentPath);
}

void FileBrowserApp::stop() {}
void FileBrowserApp::update() {}

String FileBrowserApp::truncateFilename(const String& name, int maxLen) {
    if (name.length() <= maxLen) return name;
    if (maxLen <= 3) return name.substring(0, maxLen);
    return name.substring(0, maxLen - 3) + "...";
}

void FileBrowserApp::scanDirectory(const String& path) {
    fileList.clear();
    selectedIndex = 0;
    scrollOffset = 0;
    menuState = MENU_CLOSED;

    File dir = SPIFFS.open(path);
    if (!dir || !dir.isDirectory()) return;

    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            FileEntry entry;
            String full = String(file.name());
            int lastSlash = full.lastIndexOf('/');
            if (lastSlash >= 0) entry.name = full.substring(lastSlash + 1);
            else entry.name = full;
            entry.isDir = false;
            entry.isParent = false;
            entry.size = file.size();
            // SPIFFS does not reliably expose modification time; keep 0 as placeholder
            entry.mtime = 0;
            fileList.push_back(entry);
        }
        file = dir.openNextFile();
    }
    dir.close();

    // Sort according to mode
    if (fileList.size() > 1) {
        if (sortMode == SORT_NAME) {
            std::sort(fileList.begin(), fileList.end(), [](const FileEntry &a, const FileEntry &b){
                String A = a.name; A.toLowerCase();
                String B = b.name; B.toLowerCase();
                return A < B;
            });
        } else {
            // SORT_MTIME: fallback to size if mtime not available
            std::sort(fileList.begin(), fileList.end(), [](const FileEntry &a, const FileEntry &b){
                if (a.mtime != b.mtime) return a.mtime > b.mtime; // newest first
                return a.size > b.size; // fallback: larger files first
            });
        }
    }
}

void FileBrowserApp::openFileContextMenu() {
    menuState = MENU_FILE_CONTEXT;
    menuSelectedIndex = 0;
}

void FileBrowserApp::closeMenu() {
    menuState = MENU_CLOSED;
    menuSelectedIndex = 0;
}

void FileBrowserApp::handleFileAction(int actionIndex) {
    closeMenu();
    if (selectedIndex < 0 || selectedIndex >= (int)fileList.size()) return;
    FileEntry& entry = fileList[selectedIndex];
    String fullPath = String("/") + entry.name;
    if (actionIndex == 0) {
        // View (read-only)
        appTransferAction = ACTION_VIEW_FILE;
        appTransferPath = fullPath;
        appTransferString = "";
        appTransferCaller = this;
        // Load content
        File f = SPIFFS.open(fullPath, FILE_READ);
        if (f) {
            while (f.available()) appTransferString += (char)f.read();
            f.close();
        }
        switchApp(&appT9Editor);
        return;
    }

    if (actionIndex == 1) {
        // Edit (read-write)
        appTransferAction = ACTION_EDIT_FILE;
        appTransferPath = fullPath;
        appTransferString = "";
        appTransferCaller = this;
        // Load content
        File f = SPIFFS.open(fullPath, FILE_READ);
        if (f) {
            while (f.available()) appTransferString += (char)f.read();
            f.close();
        }
        switchApp(&appT9Editor);
        return;
    }

    if (actionIndex == 2) {
        // Delete -> confirm via yes/no prompt
        appTransferAction = ACTION_DELETE_FILE;
        appTransferPath = fullPath;
        appTransferCaller = this;
        yesNoPrompt.startPrompt(this, String("Delete: ") + entry.name + "?");
        switchApp(&yesNoPrompt);
        return;
    }
}

void FileBrowserApp::handleInput(char key) {
    // If menu open -> navigate menu
    if (menuState != MENU_CLOSED) {
        if (key == 'D') { closeMenu(); return; }
        if (key == '2') { menuSelectedIndex--; if (menuSelectedIndex < 0) menuSelectedIndex = 0; return; }
        if (key == '8') { menuSelectedIndex++; if (menuSelectedIndex > 2) menuSelectedIndex = 2; return; }
        if (key == '5') { handleFileAction(menuSelectedIndex); return; }
        return;
    }

    // Sorting is fixed to name; '3' disabled

    // Navigation
    if (key == '2') {
        selectedIndex--;
        if (selectedIndex < 0) {
            selectedIndex = fileList.size() - 1;
            scrollOffset = (int)fileList.size() - VISIBLE_ITEMS;
            if (scrollOffset < 0) scrollOffset = 0;
        } else if (selectedIndex < scrollOffset) {
            scrollOffset--;
        }
        return;
    }
    if (key == '8') {
        selectedIndex++;
        if (selectedIndex >= (int)fileList.size()) {
            selectedIndex = 0;
            scrollOffset = 0;
        } else if (selectedIndex >= scrollOffset + VISIBLE_ITEMS) {
            scrollOffset++;
        }
        return;
    }

    if (key == '5') {
        // Open file context menu
        if (selectedIndex < (int)fileList.size()) openFileContextMenu();
        return;
    }

    // Create new file: press '1'
    if (key == '1') {
        // Launch T9 to request a filename
        appTransferAction = ACTION_REQUEST_FILENAME;
        appTransferString = "";
        appTransferCaller = this;
        switchApp(&appT9Editor);
        return;
    }
}

void FileBrowserApp::render() {
    u8g2.setFont(FONT_SMALL);

    // Header
    u8g2.drawBox(0, 0, 128, HEADER_HEIGHT);
    u8g2.setDrawColor(0);
    u8g2.drawStr(2, 9, "SPIFFS");
    u8g2.setDrawColor(1);

    // Space used/total
    char buf[32];
    size_t total = SPIFFS.totalBytes();
    size_t used = SPIFFS.usedBytes();
    snprintf(buf, sizeof(buf), "%u/%u", (unsigned)used, (unsigned)total);
    u8g2.drawStr(64, 9, buf);

    // (Sorting currently fixed to name)

    // Path
    //String pathDisplay = truncateFilename(currentPath, 25);
    //u8g2.drawStr(2, 20, pathDisplay.c_str());

    // File list
    for (int i = 0; i < VISIBLE_ITEMS && scrollOffset + i < (int)fileList.size(); i++) {
        int y = START_Y + i * LINE_HEIGHT;
        int listIndex = scrollOffset + i;

        if (listIndex == selectedIndex) {
            u8g2.drawBox(0, y - 8, 128, LINE_HEIGHT);
            u8g2.setDrawColor(0);
        }

        String displayName = truncateFilename(fileList[listIndex].name, MAX_FILENAME_DISPLAY);
        u8g2.drawStr(2, y, displayName.c_str());

        if (listIndex == selectedIndex) u8g2.setDrawColor(1);
    }

    // Scrollbar
    if (fileList.size() > VISIBLE_ITEMS) {
        int scrollBarH = 64 - HEADER_HEIGHT;
        int knobH = (scrollBarH * VISIBLE_ITEMS) / (int)fileList.size();
        int knobY = HEADER_HEIGHT + (scrollBarH * scrollOffset) / (int)fileList.size();
        u8g2.drawVLine(126, HEADER_HEIGHT, scrollBarH);
        u8g2.drawBox(125, knobY, 3, knobH);
    }

    // Context menu (with opaque white background and padding)
    if (menuState == MENU_FILE_CONTEXT) {
        const int mx = 16; // menu x
        const int my = 16; // menu y
        const int mw = 96; // menu width
        const int mh = 40; // menu height (allow extra padding)
        const int padX = 8;
        const int padY = 8;

        // Background and frame
        u8g2.setDrawColor(1);
        u8g2.drawBox(mx, my, mw, mh);
        u8g2.setDrawColor(0);
        u8g2.drawFrame(mx, my, mw, mh);

        const char* items[] = {"View","Edit","Delete"};
        const int itemCount = 3;
        const int itemSpacing = 10;
        for (int i = 0; i < itemCount; i++) {
            int y = my + padY + i * itemSpacing + 6; // row baseline
            int tx = mx + padX;
            if (i == menuSelectedIndex) {
                // Selected: black box, white text
                u8g2.setDrawColor(0);
                u8g2.drawBox(tx - 2, y - 8, mw - padX * 2, itemSpacing);
                u8g2.setDrawColor(1);
                u8g2.drawStr(tx, y, items[i]);
            } else {
                // Normal: black text on white
                u8g2.setDrawColor(0);
                u8g2.drawStr(tx, y, items[i]);
            }
        }
        u8g2.setDrawColor(1);
    }

    // Save message (toast-like)
    if (saveMessage.length() > 0 && millis() < saveMessageUntil) {
        u8g2.setFont(FONT_SMALL);
        u8g2.drawFrame(30, 54, 68, 8);
        u8g2.drawStr(36, 60, saveMessage.c_str());
    } else {
        // Clear stale message
        if (saveMessageUntil != 0 && millis() >= saveMessageUntil) {
            saveMessage = "";
            saveMessageUntil = 0;
        }
    }
}
