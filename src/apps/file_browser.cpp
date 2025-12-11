// [Revision: v2.1] [Path: src/apps/file_browser.cpp] [Date: 2025-12-11]
// Description: SPIFFS file browser - refactored to use unified GUI module.
//              Now supports running .lua files directly via context menu.

#include "file_browser.h"
#include "../gui.h"
#include "../app_transfer.h"
#include "../app_control.h"
#include "yes_no_prompt.h"
#include "../apps/t9_editor.h"
#include "../lua_vm.h"
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
    browserMode = BROWSE_MODE;
    currentLuaScript = "";
    luaErrorMessage = "";
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

bool FileBrowserApp::isLuaFile(const String& name) {
    String lower = name;
    lower.toLowerCase();
    return lower.endsWith(".lua");
}

void FileBrowserApp::runLuaScript(const String& path) {
    // Initialize Lua VM if needed
    if (!LuaVM::isReady()) {
        LuaVM::init();
    }
    
    currentLuaScript = path;
    browserMode = LUA_RUNNING;
    
    // Execute the script
    if (!LuaVM::executeFile(path.c_str())) {
        luaErrorMessage = LuaVM::getLastError();
        browserMode = LUA_ERROR;
    }
}

void FileBrowserApp::handleFileAction(int actionIndex) {
    closeMenu();
    if (selectedIndex < 0 || selectedIndex >= (int)fileList.size()) return;
    FileEntry& entry = fileList[selectedIndex];
    String fullPath = String("/") + entry.name;
    
    // For .lua files: Run(0), View(1), Edit(2), Delete(3)
    // For other files: View(0), Edit(1), Delete(2)
    bool isLua = isLuaFile(entry.name);
    
    if (isLua && actionIndex == 0) {
        // Run lua script
        runLuaScript(fullPath);
        return;
    }
    
    // Adjust index for non-lua files (lua has Run as index 0)
    int adjustedIndex = isLua ? actionIndex - 1 : actionIndex;
    
    if (adjustedIndex == 0) {
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

    if (adjustedIndex == 1) {
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

    if (adjustedIndex == 2) {
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
    // Handle Lua running mode
    if (browserMode == LUA_RUNNING) {
        // Pass input to Lua (gfx.poll handles this internally)
        // Check for exit keys (* or #)
        if (key == '*' || key == '#') {
            browserMode = BROWSE_MODE;
            currentLuaScript = "";
        }
        return;
    }
    
    // Handle Lua error mode
    if (browserMode == LUA_ERROR) {
        // Any key returns to browser
        browserMode = BROWSE_MODE;
        luaErrorMessage = "";
        return;
    }
    
    // If menu open -> navigate menu
    if (menuState != MENU_CLOSED) {
        // Determine max menu index based on file type
        int maxMenuIndex = 2;  // View, Edit, Delete = 3 items
        if (selectedIndex >= 0 && selectedIndex < (int)fileList.size()) {
            if (isLuaFile(fileList[selectedIndex].name)) {
                maxMenuIndex = 3;  // Run, View, Edit, Delete = 4 items
            }
        }
        
        if (key == 'D') { closeMenu(); return; }
        if (key == '2') { menuSelectedIndex--; if (menuSelectedIndex < 0) menuSelectedIndex = 0; return; }
        if (key == '8') { menuSelectedIndex++; if (menuSelectedIndex > maxMenuIndex) menuSelectedIndex = maxMenuIndex; return; }
        if (key == '5') { handleFileAction(menuSelectedIndex); return; }
        return;
    }

    // Navigation using GUI helper
    GUI::ScrollState state = {selectedIndex, scrollOffset, (int)fileList.size(), VISIBLE_ITEMS};
    if (GUI::handleListNavigation(state, key)) {
        selectedIndex = state.selectedIndex;
        scrollOffset = state.scrollOffset;
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

void FileBrowserApp::drawLuaRunning() {
    GUI::drawHeader("RUNNING");
    
    GUI::setFontSmall();
    String name = currentLuaScript;
    if (name.startsWith("/")) name = name.substring(1);
    name = GUI::truncateString(name, 20);
    u8g2.drawStr(2, 24, name.c_str());
    
    u8g2.drawStr(2, 40, "Press * or # to exit");
    
    // Memory usage
    char memStr[32];
    snprintf(memStr, sizeof(memStr), "Mem: %d KB", (int)(LuaVM::getMemoryUsage() / 1024));
    u8g2.drawStr(2, 55, memStr);
}

void FileBrowserApp::drawLuaError() {
    GUI::drawHeader("LUA ERROR");
    
    GUI::setFontSmall();
    
    // Word-wrap error message
    int y = 24;
    int lineStart = 0;
    int maxChars = 25;
    
    for (int i = 0; i <= (int)luaErrorMessage.length() && y < 52; i++) {
        if (i == (int)luaErrorMessage.length() || (i - lineStart) >= maxChars) {
            String line = luaErrorMessage.substring(lineStart, i);
            u8g2.drawStr(2, y, line.c_str());
            y += 8;
            lineStart = i;
        }
    }
    
    GUI::drawFooter("Any key: Back");
}

void FileBrowserApp::render() {
    // Handle Lua running mode
    if (browserMode == LUA_RUNNING) {
        drawLuaRunning();
        return;
    }
    
    // Handle Lua error mode
    if (browserMode == LUA_ERROR) {
        drawLuaError();
        return;
    }
    
    // Header with current/total and human readable space
    char headerInfo[24];
    unsigned long usedKB = SPIFFS.usedBytes() / 1024;
    unsigned long totalKB = SPIFFS.totalBytes() / 1024;
    int fileTotal = (int)fileList.size();
    if (fileTotal > 0) {
        snprintf(headerInfo, sizeof(headerInfo), "%d/%d %luK", selectedIndex + 1, fileTotal, totalKB - usedKB);
    } else {
        snprintf(headerInfo, sizeof(headerInfo), "0/0 %luK", totalKB - usedKB);
    }
    GUI::drawHeader("FILES", headerInfo);

    // File list
    GUI::setFontSmall();
    GUI::ListConfig listConfig;
    listConfig.showSelector = false;
    listConfig.leftMargin = 2;
    
    for (int i = 0; i < VISIBLE_ITEMS && scrollOffset + i < (int)fileList.size(); i++) {
        int y = GUI::CONTENT_START_Y + i * GUI::LINE_HEIGHT;
        int listIndex = scrollOffset + i;

        if (listIndex == selectedIndex) {
            u8g2.drawBox(0, y - 8, 123, GUI::LINE_HEIGHT);
            u8g2.setDrawColor(0);
        }

        String displayName = GUI::truncateString(fileList[listIndex].name, MAX_FILENAME_DISPLAY);
        u8g2.drawStr(2, y, displayName.c_str());

        if (listIndex == selectedIndex) u8g2.setDrawColor(1);
    }

    // Scrollbar (accounting for header and footer)
    if ((int)fileList.size() > VISIBLE_ITEMS) {
        GUI::drawScrollbar(GUI::SCREEN_WIDTH - GUI::SCROLLBAR_WIDTH - 1,
                          GUI::HEADER_HEIGHT,
                          GUI::SCREEN_HEIGHT - GUI::HEADER_HEIGHT - GUI::FOOTER_HEIGHT,
                          (int)fileList.size(), VISIBLE_ITEMS, scrollOffset);
    }

    // Context menu - dynamic based on file type
    if (menuState == MENU_FILE_CONTEXT && selectedIndex >= 0 && selectedIndex < (int)fileList.size()) {
        if (isLuaFile(fileList[selectedIndex].name)) {
            // Lua file: Run, View, Edit, Delete
            const char* menuItems[] = {"Run", "View", "Edit", "Delete"};
            GUI::drawContextMenu(menuItems, 4, menuSelectedIndex, 16, 16);
        } else {
            // Regular file: View, Edit, Delete
            const char* menuItems[] = {"View", "Edit", "Delete"};
            GUI::drawContextMenu(menuItems, 3, menuSelectedIndex, 16, 16);
        }
    }

    // Footer hints (only when context menu is closed)
    if (menuState == MENU_CLOSED) {
        GUI::drawFooterHints("5:Menu", "D:Back");
    }

    // Toast message for save feedback
    if (saveMessage.length() > 0 && millis() < saveMessageUntil) {
        GUI::showToast(saveMessage.c_str(), 0);  // Duration already set
        GUI::updateToast();
    } else if (saveMessageUntil != 0 && millis() >= saveMessageUntil) {
        saveMessage = "";
        saveMessageUntil = 0;
    }
}
