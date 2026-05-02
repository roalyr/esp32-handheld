// [Revision: v2.3] [Path: src/apps/lua_runner.cpp] [Date: 2025-12-11]
// Description: Lua script browser and runner - refactored to use unified GUI module.

#include "lua_runner.h"
#include "../hal.h"
#include "../gui.h"
#include "../lua_vm.h"
#include "../config.h"

extern "C" {
#include <lua/lua.h>  // For LUA_VERSION_MAJOR/MINOR
}

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
    // No filesystem mounted (SPIFFS disabled, SD card not wired)
}

void LuaRunnerApp::drawBrowser() {
    const GUI::FontMetrics& metrics = GUI::getSystemFontMetrics();
    const int visibleLines = GUI::getVisibleListRows();

    // Header with Lua version and current/total
    char headerInfo[20];
    if (fileCount > 0) {
        snprintf(headerInfo, sizeof(headerInfo), "%s.%s %d/%d", LUA_VERSION_MAJOR, LUA_VERSION_MINOR, selectedIndex + 1, fileCount);
    } else {
        snprintf(headerInfo, sizeof(headerInfo), "%s.%s 0/0", LUA_VERSION_MAJOR, LUA_VERSION_MINOR);
    }
    GUI::drawHeader("LUA", headerInfo);
    
    if (fileCount == 0) {
        GUI::setFontSystem();
        u8g2.drawUTF8(10, GUI::getContentBaselineStart() + metrics.lineHeight, "No .lua files found");
        u8g2.drawUTF8(10, GUI::getContentBaselineStart() + (metrics.lineHeight * 2), "Upload via SPIFFS");
    } else {
        GUI::setFontSystem();
        for (int i = 0; i < visibleLines && (scrollOffset + i) < fileCount; i++) {
            int idx = scrollOffset + i;
            int y = GUI::getContentBaselineStart() + (i * metrics.lineHeight);
            
            if (idx == selectedIndex) {
                u8g2.drawBox(0, GUI::getHighlightTop(y), 123, GUI::getHighlightHeight());
                u8g2.setDrawColor(0);
            }
            
            String displayName = files[idx];
            if (displayName.startsWith("/")) {
                displayName = displayName.substring(1);
            }
            displayName = GUI::truncateStringToWidth(displayName, GUI::SCREEN_WIDTH - GUI::SCROLLBAR_WIDTH - 6);
            u8g2.drawUTF8(2, y, displayName.c_str());
            
            u8g2.setDrawColor(1);
        }
        
        if (fileCount > visibleLines) {
            GUI::drawScrollbar(GUI::SCREEN_WIDTH - GUI::SCROLLBAR_WIDTH - 1,
                              GUI::getContentAreaTop(),
                              GUI::getContentHeight(),
                              fileCount, visibleLines, scrollOffset);
        }
    }
    
    GUI::drawFooterHints("Enter:Run", "Esc:Back");
}

void LuaRunnerApp::drawRunning() {
    GUI::drawHeader("RUNNING");
    
    GUI::setFontSystem();
    const GUI::FontMetrics& metrics = GUI::getSystemFontMetrics();
    String name = currentScript;
    if (name.startsWith("/")) name = name.substring(1);
    name = GUI::truncateStringToWidth(name, GUI::SCREEN_WIDTH - 4);
    u8g2.drawUTF8(2, GUI::getContentBaselineStart(), name.c_str());
    
    u8g2.drawUTF8(2, GUI::getContentBaselineStart() + (metrics.lineHeight * 2), "Press ESC to exit");
    
    char memStr[32];
    snprintf(memStr, sizeof(memStr), "Mem: %d KB", (int)(LuaVM::getMemoryUsage() / 1024));
    u8g2.drawUTF8(2, GUI::getFooterBaselineY(), memStr);
}

void LuaRunnerApp::drawError() {
    GUI::drawHeader("LUA ERROR");
    
    GUI::setFontSystem();
    const GUI::FontMetrics& metrics = GUI::getSystemFontMetrics();
    
    int y = GUI::getContentBaselineStart();
    int lineStart = 0;
    int charWidth = static_cast<int>(u8g2.getStrWidth("W"));
    if (charWidth < 1) charWidth = 1;
    int maxChars = max(1, (GUI::SCREEN_WIDTH - 4) / charWidth);
    
    for (int i = 0; i <= (int)errorMessage.length() && y <= GUI::getContentBottom(); i++) {
        if (i == (int)errorMessage.length() || (i - lineStart) >= maxChars) {
            String line = GUI::truncateStringToWidth(errorMessage.substring(lineStart, i), GUI::SCREEN_WIDTH - 4);
            u8g2.drawUTF8(2, y, line.c_str());
            y += metrics.lineHeight;
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
