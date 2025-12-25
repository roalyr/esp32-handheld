// [Revision: v3.0] [Path: src/apps/file_browser.cpp] [Date: 2025-12-11]
// Description: SPIFFS file browser - refactored for clarity.
//              Organized into: initialization, file operations, input handling, rendering.

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

// ==========================================================================
// CONSTRUCTOR & LIFECYCLE
// ==========================================================================

FileBrowserApp::FileBrowserApp() {
    resetState();
}

void FileBrowserApp::resetState() {
    selectedIndex = 0;
    scrollOffset = 0;
    menuState = MENU_CLOSED;
    menuSelectedIndex = 0;
    currentPath = "/";
    spiffsAvailable = false;
    awaitingSaveConfirmation = false;
    saveMessage = "";
    saveMessageUntil = 0;
    sortMode = SORT_NAME;
    browserMode = BROWSE_MODE;
    currentLuaScript = "";
    luaErrorMessage = "";
}

void FileBrowserApp::start() {
    u8g2.setContrast(systemContrast);
    spiffsAvailable = SPIFFS.totalBytes() > 0;

    // Process any pending app transfer actions
    if (processPendingActions()) {
        return;  // Action triggered app switch
    }

    scanDirectory(currentPath);
}

void FileBrowserApp::stop() {}
void FileBrowserApp::update() {}

// ==========================================================================
// APP TRANSFER HANDLING
// ==========================================================================

bool FileBrowserApp::processPendingActions() {
    switch (appTransferAction) {
        case ACTION_DELETE_FILE:
            return handleDeleteReturn();
        case ACTION_REQUEST_FILENAME:
            return handleFilenameReturn();
        case ACTION_EDIT_FILE:
        case ACTION_CREATE_FILE:
            return handleEditorReturn();
        default:
            return false;
    }
}

bool FileBrowserApp::handleDeleteReturn() {
    if (appTransferBool && appTransferPath.length() > 0) {
        if (!SPIFFS.remove(appTransferPath)) {
            showMessage("Delete failed", 2000);
        }
    }
    clearTransferState();
    return false;
}

bool FileBrowserApp::handleFilenameReturn() {
    String fname = appTransferString;
    appTransferString = "";
    
    if (fname.length() > 0) {
        appTransferPath = "/" + fname;
        appTransferAction = ACTION_CREATE_FILE;
        appTransferCaller = this;
        appTransferString = "";
        switchApp(&appT9Editor);
        return true;
    }
    
    appTransferAction = ACTION_NONE;
    return false;
}

bool FileBrowserApp::handleEditorReturn() {
    if (!awaitingSaveConfirmation) {
        awaitingSaveConfirmation = true;
        appTransferCaller = this;
        yesNoPrompt.startPrompt(this, "Save changes?");
        switchApp(&yesNoPrompt);
        return true;
    }

    // User responded to save prompt
    if (appTransferBool && appTransferPath.length() > 0) {
        saveFileContent(appTransferPath, appTransferString);
    }

    clearTransferState();
    awaitingSaveConfirmation = false;
    return false;
}

void FileBrowserApp::clearTransferState() {
    appTransferAction = ACTION_NONE;
    appTransferPath = "";
    appTransferString = "";
    appTransferBool = false;
}

void FileBrowserApp::showMessage(const char* msg, unsigned long duration) {
    saveMessage = msg;
    saveMessageUntil = millis() + duration;
}

// ==========================================================================
// FILE OPERATIONS
// ==========================================================================

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
            entry.name = (lastSlash >= 0) ? full.substring(lastSlash + 1) : full;
            entry.isDir = false;
            entry.isParent = false;
            entry.size = file.size();
            entry.mtime = 0;  // SPIFFS doesn't expose mtime
            fileList.push_back(entry);
        }
        file = dir.openNextFile();
    }
    dir.close();

    sortFileList();
}

void FileBrowserApp::sortFileList() {
    if (fileList.size() <= 1) return;
    
    if (sortMode == SORT_NAME) {
        std::sort(fileList.begin(), fileList.end(), [](const FileEntry &a, const FileEntry &b) {
            String A = a.name; A.toLowerCase();
            String B = b.name; B.toLowerCase();
            return A < B;
        });
    } else {
        std::sort(fileList.begin(), fileList.end(), [](const FileEntry &a, const FileEntry &b) {
            if (a.mtime != b.mtime) return a.mtime > b.mtime;
            return a.size > b.size;
        });
    }
}

bool FileBrowserApp::isLuaFile(const String& name) {
    String lower = name;
    lower.toLowerCase();
    return lower.endsWith(".lua");
}

String FileBrowserApp::loadFileContent(const String& path) {
    String content = "";
    File f = SPIFFS.open(path, FILE_READ);
    if (f) {
        while (f.available()) content += (char)f.read();
        f.close();
    }
    return content;
}

bool FileBrowserApp::saveFileContent(const String& path, const String& content) {
    File f = SPIFFS.open(path, FILE_WRITE);
    if (!f) {
        showMessage("Save failed", 2000);
        return false;
    }
    f.print(content);
    f.close();
    showMessage("Saved", 2000);
    return true;
}

// ==========================================================================
// FILE ACTIONS
// ==========================================================================

void FileBrowserApp::runLuaScript(const String& path) {
    if (!LuaVM::isReady()) {
        LuaVM::init();
    }
    
    currentLuaScript = path;
    browserMode = LUA_RUNNING;
    
    if (!LuaVM::executeFile(path.c_str())) {
        luaErrorMessage = LuaVM::getLastError();
        browserMode = LUA_ERROR;
    }
}

void FileBrowserApp::viewFile(const String& path) {
    appTransferAction = ACTION_VIEW_FILE;
    appTransferPath = path;
    appTransferString = loadFileContent(path);
    appTransferCaller = this;
    switchApp(&appT9Editor);
}

void FileBrowserApp::editFile(const String& path) {
    appTransferAction = ACTION_EDIT_FILE;
    appTransferPath = path;
    appTransferString = loadFileContent(path);
    appTransferCaller = this;
    switchApp(&appT9Editor);
}

void FileBrowserApp::deleteFile(const String& path, const String& name) {
    appTransferAction = ACTION_DELETE_FILE;
    appTransferPath = path;
    appTransferCaller = this;
    yesNoPrompt.startPrompt(this, "Delete: " + name + "?");
    switchApp(&yesNoPrompt);
}

void FileBrowserApp::createNewFile() {
    appTransferAction = ACTION_REQUEST_FILENAME;
    appTransferString = "";
    appTransferCaller = this;
    switchApp(&appT9Editor);
}

void FileBrowserApp::handleFileAction(int actionIndex) {
    closeMenu();
    if (selectedIndex < 0 || selectedIndex >= (int)fileList.size()) return;
    
    FileEntry& entry = fileList[selectedIndex];
    String fullPath = "/" + entry.name;
    bool isLua = isLuaFile(entry.name);
    
    // For .lua: Run(0), View(1), Edit(2), Delete(3)
    // For other: View(0), Edit(1), Delete(2)
    int adjustedIndex = isLua ? actionIndex : actionIndex + 1;
    
    switch (adjustedIndex) {
        case 0: runLuaScript(fullPath); break;
        case 1: viewFile(fullPath); break;
        case 2: editFile(fullPath); break;
        case 3: deleteFile(fullPath, entry.name); break;
    }
}

// ==========================================================================
// MENU MANAGEMENT
// ==========================================================================

void FileBrowserApp::openFileContextMenu() {
    menuState = MENU_FILE_CONTEXT;
    menuSelectedIndex = 0;
}

void FileBrowserApp::closeMenu() {
    menuState = MENU_CLOSED;
    menuSelectedIndex = 0;
}

int FileBrowserApp::getMenuItemCount() {
    if (selectedIndex >= 0 && selectedIndex < (int)fileList.size()) {
        return isLuaFile(fileList[selectedIndex].name) ? 4 : 3;
    }
    return 3;
}

// ==========================================================================
// INPUT HANDLING
// ==========================================================================

void FileBrowserApp::handleInput(char key) {
    switch (browserMode) {
        case LUA_RUNNING:
            handleLuaRunningInput(key);
            break;
        case LUA_ERROR:
            handleLuaErrorInput(key);
            break;
        case BROWSE_MODE:
            handleBrowseInput(key);
            break;
    }
}

void FileBrowserApp::handleLuaRunningInput(char key) {
    if (key == KEY_ESC) {
        browserMode = BROWSE_MODE;
        currentLuaScript = "";
    }
}

void FileBrowserApp::handleLuaErrorInput(char key) {
    browserMode = BROWSE_MODE;
    luaErrorMessage = "";
}

void FileBrowserApp::handleBrowseInput(char key) {
    if (menuState != MENU_CLOSED) {
        handleMenuInput(key);
        return;
    }

    // List navigation
    GUI::ScrollState state = {selectedIndex, scrollOffset, (int)fileList.size(), VISIBLE_ITEMS};
    if (GUI::handleListNavigation(state, key)) {
        selectedIndex = state.selectedIndex;
        scrollOffset = state.scrollOffset;
        return;
    }

    // Open context menu
    if (key == KEY_ENTER && selectedIndex < (int)fileList.size()) {
        openFileContextMenu();
        return;
    }

    // Create new file
    if (key == '1') {
        createNewFile();
    }
}

void FileBrowserApp::handleMenuInput(char key) {
    int maxIndex = getMenuItemCount() - 1;
    
    switch (key) {
        case KEY_ESC: closeMenu(); break;
        case KEY_UP: menuSelectedIndex = max(0, menuSelectedIndex - 1); break;
        case KEY_DOWN: menuSelectedIndex = min(maxIndex, menuSelectedIndex + 1); break;
        case KEY_ENTER: handleFileAction(menuSelectedIndex); break;
    }
}

// ==========================================================================
// RENDERING
// ==========================================================================

void FileBrowserApp::render() {
    switch (browserMode) {
        case LUA_RUNNING: renderLuaRunning(); break;
        case LUA_ERROR:   renderLuaError(); break;
        case BROWSE_MODE: renderBrowser(); break;
    }
}

void FileBrowserApp::renderLuaRunning() {
    GUI::drawHeader("RUNNING");
    GUI::setFontSmall();
    
    String name = currentLuaScript;
    if (name.startsWith("/")) name = name.substring(1);
    name = GUI::truncateString(name, 20);
    u8g2.drawStr(2, 24, name.c_str());
    u8g2.drawStr(2, 40, "Press ESC to exit");
    
    char memStr[32];
    snprintf(memStr, sizeof(memStr), "Mem: %d KB", (int)(LuaVM::getMemoryUsage() / 1024));
    u8g2.drawStr(2, 55, memStr);
}

void FileBrowserApp::renderLuaError() {
    GUI::drawHeader("LUA ERROR");
    GUI::setFontSmall();
    
    // Simple word-wrap for error message
    int y = 24;
    int lineStart = 0;
    const int maxChars = 25;
    
    for (int i = 0; i <= (int)luaErrorMessage.length() && y < 52; i++) {
        if (i == (int)luaErrorMessage.length() || (i - lineStart) >= maxChars) {
            u8g2.drawStr(2, y, luaErrorMessage.substring(lineStart, i).c_str());
            y += 8;
            lineStart = i;
        }
    }
    
    GUI::drawFooter("Any key: Back");
}

void FileBrowserApp::renderBrowser() {
    renderHeader();
    renderFileList();
    renderScrollbar();
    renderContextMenu();
    renderFooter();
    renderToast();
}

void FileBrowserApp::renderHeader() {
    char headerInfo[24];
    unsigned long freeKB = (SPIFFS.totalBytes() - SPIFFS.usedBytes()) / 1024;
    int fileTotal = (int)fileList.size();
    
    if (fileTotal > 0) {
        snprintf(headerInfo, sizeof(headerInfo), "%d/%d %luK", selectedIndex + 1, fileTotal, freeKB);
    } else {
        snprintf(headerInfo, sizeof(headerInfo), "0/0 %luK", freeKB);
    }
    GUI::drawHeader("FILES", headerInfo);
}

void FileBrowserApp::renderFileList() {
    GUI::setFontSmall();
    
    for (int i = 0; i < VISIBLE_ITEMS && scrollOffset + i < (int)fileList.size(); i++) {
        int y = GUI::CONTENT_START_Y + i * GUI::LINE_HEIGHT;
        int listIndex = scrollOffset + i;
        bool isSelected = (listIndex == selectedIndex);

        if (isSelected) {
            u8g2.drawBox(0, y - 8, 123, GUI::LINE_HEIGHT);
            u8g2.setDrawColor(0);
        }

        String displayName = GUI::truncateString(fileList[listIndex].name, MAX_FILENAME_DISPLAY);
        u8g2.drawStr(2, y, displayName.c_str());

        if (isSelected) u8g2.setDrawColor(1);
    }
}

void FileBrowserApp::renderScrollbar() {
    if ((int)fileList.size() > VISIBLE_ITEMS) {
        GUI::drawScrollbar(
            GUI::SCREEN_WIDTH - GUI::SCROLLBAR_WIDTH - 1,
            GUI::HEADER_HEIGHT,
            GUI::SCREEN_HEIGHT - GUI::HEADER_HEIGHT - GUI::FOOTER_HEIGHT,
            (int)fileList.size(), VISIBLE_ITEMS, scrollOffset
        );
    }
}

void FileBrowserApp::renderContextMenu() {
    if (menuState != MENU_FILE_CONTEXT) return;
    if (selectedIndex < 0 || selectedIndex >= (int)fileList.size()) return;
    
    if (isLuaFile(fileList[selectedIndex].name)) {
        const char* items[] = {"Run", "View", "Edit", "Delete"};
        GUI::drawContextMenu(items, 4, menuSelectedIndex, 16, 16);
    } else {
        const char* items[] = {"View", "Edit", "Delete"};
        GUI::drawContextMenu(items, 3, menuSelectedIndex, 16, 16);
    }
}

void FileBrowserApp::renderFooter() {
    if (menuState == MENU_CLOSED) {
        GUI::drawFooterHints("5:Menu", "D:Back");
    }
}

void FileBrowserApp::renderToast() {
    if (saveMessage.length() > 0 && millis() < saveMessageUntil) {
        GUI::showToast(saveMessage.c_str(), 0);
        GUI::updateToast();
    } else if (saveMessageUntil != 0 && millis() >= saveMessageUntil) {
        saveMessage = "";
        saveMessageUntil = 0;
    }
}
