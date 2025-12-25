// [Revision: v2.3] [Path: src/apps/lua_runner.cpp] [Date: 2025-12-11]
// Description: Lua script browser and runner - refactored to use unified GUI module.

#include "lua_runner.h"
#include "../hal.h"
#include "../gui.h"
#include "../lua_vm.h"
#include "../config.h"
#include <SPIFFS.h>

extern "C" {
#include <lua/lua.h>  // For LUA_VERSION_MAJOR/MINOR
}

// Number of visible lines in browser
static constexpr int VISIBLE_LINES = 3;

void LuaRunnerApp::start() {
    mode = BROWSE;
    selectedIndex = 0;
    scrollOffset = 0;
    fileCount = 0;
    errorMessage = "";
    currentScript = "";
    
    // Initialize Lua VM if not already
    if (!LuaVM::isReady()) {
        LuaVM::init();
    }
    
    scanForLuaFiles();
}

void LuaRunnerApp::stop() {
    // Keep VM alive for next use, don't shut it down
}

void LuaRunnerApp::update() {
    // No continuous updates needed
}

void LuaRunnerApp::scanForLuaFiles() {
    fileCount = 0;
    
    File root = SPIFFS.open("/");
    if (!root || !root.isDirectory()) {
        return;
    }
    
    File file = root.openNextFile();
    while (file && fileCount < MAX_FILES) {
        String name = file.name();
        // Check for .lua extension
        if (name.endsWith(".lua")) {
            // Ensure path starts with /
            if (!name.startsWith("/")) {
                name = "/" + name;
            }
            files[fileCount++] = name;
        }
        file = root.openNextFile();
    }
    
    // Sort files alphabetically (simple bubble sort)
    for (int i = 0; i < fileCount - 1; i++) {
        for (int j = 0; j < fileCount - i - 1; j++) {
            if (files[j] > files[j + 1]) {
                String temp = files[j];
                files[j] = files[j + 1];
                files[j + 1] = temp;
            }
        }
    }
}

void LuaRunnerApp::drawBrowser() {
    // Header with Lua version and current/total
    char headerInfo[20];
    if (fileCount > 0) {
        snprintf(headerInfo, sizeof(headerInfo), "%s.%s %d/%d", LUA_VERSION_MAJOR, LUA_VERSION_MINOR, selectedIndex + 1, fileCount);
    } else {
        snprintf(headerInfo, sizeof(headerInfo), "%s.%s 0/0", LUA_VERSION_MAJOR, LUA_VERSION_MINOR);
    }
    GUI::drawHeader("LUA", headerInfo);
    
    if (fileCount == 0) {
        GUI::setFontSmall();
        u8g2.drawStr(10, 35, "No .lua files found");
        u8g2.drawStr(10, 45, "Upload via SPIFFS");
    } else {
        // Prepare display names (strip leading /)
        GUI::setFontSmall();
        for (int i = 0; i < VISIBLE_LINES && (scrollOffset + i) < fileCount; i++) {
            int idx = scrollOffset + i;
            int y = GUI::CONTENT_START_Y + (i * GUI::LINE_HEIGHT);
            
            // Highlight selected
            if (idx == selectedIndex) {
                u8g2.drawBox(0, y - 8, 123, GUI::LINE_HEIGHT);
                u8g2.setDrawColor(0);
            }
            
            // Draw filename (strip leading /, truncate)
            String displayName = files[idx];
            if (displayName.startsWith("/")) {
                displayName = displayName.substring(1);
            }
            displayName = GUI::truncateString(displayName, 20);
            u8g2.drawStr(2, y, displayName.c_str());
            
            u8g2.setDrawColor(1);
        }
        
        // Scrollbar (accounting for header and footer)
        if (fileCount > VISIBLE_LINES) {
            GUI::drawScrollbar(GUI::SCREEN_WIDTH - GUI::SCROLLBAR_WIDTH - 1,
                              GUI::HEADER_HEIGHT,
                              GUI::SCREEN_HEIGHT - GUI::HEADER_HEIGHT - GUI::FOOTER_HEIGHT,
                              fileCount, VISIBLE_LINES, scrollOffset);
        }
    }
    
    GUI::drawFooterHints("Enter:Run", "Esc:Back");
}

void LuaRunnerApp::drawRunning() {
    GUI::drawHeader("RUNNING");
    
    GUI::setFontSmall();
    String name = currentScript;
    if (name.startsWith("/")) name = name.substring(1);
    name = GUI::truncateString(name, 20);
    u8g2.drawStr(2, 24, name.c_str());
    
    u8g2.drawStr(2, 40, "Press ESC to exit");
    
    // Memory usage
    char memStr[32];
    snprintf(memStr, sizeof(memStr), "Mem: %d KB", (int)(LuaVM::getMemoryUsage() / 1024));
    u8g2.drawStr(2, 55, memStr);
}

void LuaRunnerApp::drawError() {
    GUI::drawHeader("LUA ERROR");
    
    GUI::setFontSmall();
    
    // Word-wrap error message
    int y = 24;
    int lineStart = 0;
    int maxChars = 25;
    
    for (int i = 0; i <= (int)errorMessage.length() && y < 52; i++) {
        if (i == (int)errorMessage.length() || (i - lineStart) >= maxChars) {
            String line = errorMessage.substring(lineStart, i);
            u8g2.drawStr(2, y, line.c_str());
            y += 8;
            lineStart = i;
        }
    }
    
    GUI::drawFooter("Any key: Back");
}

void LuaRunnerApp::runSelectedScript() {
    if (selectedIndex < 0 || selectedIndex >= fileCount) {
        return;
    }
    
    currentScript = files[selectedIndex];
    mode = RUNNING;
    
    // Execute the script
    if (!LuaVM::executeFile(currentScript.c_str())) {
        errorMessage = LuaVM::getLastError();
        mode = ERROR;
    }
}

void LuaRunnerApp::render() {
    switch (mode) {
        case BROWSE:
            drawBrowser();
            break;
        case RUNNING:
            drawRunning();
            break;
        case ERROR:
            drawError();
            break;
    }
}

void LuaRunnerApp::handleInput(char key) {
    switch (mode) {
        case BROWSE: {
            GUI::ScrollState state = {selectedIndex, scrollOffset, fileCount, VISIBLE_LINES};
            if (GUI::handleListNavigation(state, key)) {
                selectedIndex = state.selectedIndex;
                scrollOffset = state.scrollOffset;
                break;
            }
            
            if (key == KEY_ENTER && fileCount > 0) {
                runSelectedScript();
            }
            break;
        }
            
        case RUNNING:
            // Allow exiting with ESC or BKSP
            if (key == KEY_ESC || key == KEY_BKSP) {
                mode = BROWSE;
                LuaVM::collectGarbage();
            }
            break;
            
        case ERROR:
            // Any key returns to browser
            mode = BROWSE;
            LuaVM::clearError();
            break;
    }
}
