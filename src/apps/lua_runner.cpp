// [Revision: v1.1] [Path: src/apps/lua_runner.cpp] [Date: 2025-12-11]
// Description: Implementation of Lua script browser and runner.

#include "lua_runner.h"
#include "../hal.h"
#include "../lua_vm.h"
#include <SPIFFS.h>

// Number of visible lines in browser
static constexpr int VISIBLE_LINES = 6;
static constexpr int LINE_HEIGHT = 9;
static constexpr int HEADER_HEIGHT = 10;

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
    u8g2.setFont(u8g2_font_5x7_tf);
    
    // Header
    u8g2.drawStr(0, 7, "Lua Scripts");
    u8g2.drawHLine(0, HEADER_HEIGHT, 128);
    
    if (fileCount == 0) {
        u8g2.drawStr(10, 35, "No .lua files found");
        u8g2.drawStr(10, 45, "Upload via SPIFFS");
    } else {
        // Draw file list
        for (int i = 0; i < VISIBLE_LINES && (scrollOffset + i) < fileCount; i++) {
            int idx = scrollOffset + i;
            int y = HEADER_HEIGHT + 8 + (i * LINE_HEIGHT);
            
            // Highlight selected
            if (idx == selectedIndex) {
                u8g2.drawBox(0, y - 7, 128, LINE_HEIGHT);
                u8g2.setDrawColor(0);
            }
            
            // Draw filename (truncate if too long)
            String displayName = files[idx];
            if (displayName.startsWith("/")) {
                displayName = displayName.substring(1);
            }
            if (displayName.length() > 20) {
                displayName = displayName.substring(0, 17) + "...";
            }
            u8g2.drawStr(2, y, displayName.c_str());
            
            u8g2.setDrawColor(1);
        }
        
        // Scroll indicator
        if (fileCount > VISIBLE_LINES) {
            int barHeight = (VISIBLE_LINES * LINE_HEIGHT * VISIBLE_LINES) / fileCount;
            int barY = HEADER_HEIGHT + 2 + (scrollOffset * (VISIBLE_LINES * LINE_HEIGHT - barHeight)) / (fileCount - VISIBLE_LINES);
            u8g2.drawVLine(126, HEADER_HEIGHT + 2, VISIBLE_LINES * LINE_HEIGHT);
            u8g2.drawBox(125, barY, 3, barHeight);
        }
    }
    
    // Footer with controls
    u8g2.drawHLine(0, 55, 128);
    u8g2.drawStr(0, 63, "5:Run  D:Back");
}

void LuaRunnerApp::drawRunning() {
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(0, 7, "Running:");
    
    String name = currentScript;
    if (name.startsWith("/")) name = name.substring(1);
    if (name.length() > 20) name = name.substring(0, 17) + "...";
    u8g2.drawStr(0, 17, name.c_str());
    
    u8g2.drawStr(0, 35, "Press * or # to exit");
    
    // Memory usage
    char memStr[32];
    snprintf(memStr, sizeof(memStr), "Lua Mem: %d KB", (int)(LuaVM::getMemoryUsage() / 1024));
    u8g2.drawStr(0, 55, memStr);
}

void LuaRunnerApp::drawError() {
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(0, 7, "Lua Error:");
    u8g2.drawHLine(0, 9, 128);
    
    // Word-wrap error message
    int y = 18;
    int lineStart = 0;
    int maxChars = 25;
    
    for (int i = 0; i <= (int)errorMessage.length() && y < 55; i++) {
        if (i == (int)errorMessage.length() || (i - lineStart) >= maxChars) {
            String line = errorMessage.substring(lineStart, i);
            u8g2.drawStr(0, y, line.c_str());
            y += 8;
            lineStart = i;
        }
    }
    
    u8g2.drawHLine(0, 55, 128);
    u8g2.drawStr(0, 63, "Any key: Back");
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
        case BROWSE:
            switch (key) {
                case '2': // Up
                    if (selectedIndex > 0) {
                        selectedIndex--;
                        if (selectedIndex < scrollOffset) {
                            scrollOffset = selectedIndex;
                        }
                    }
                    break;
                    
                case '8': // Down
                    if (selectedIndex < fileCount - 1) {
                        selectedIndex++;
                        if (selectedIndex >= scrollOffset + VISIBLE_LINES) {
                            scrollOffset = selectedIndex - VISIBLE_LINES + 1;
                        }
                    }
                    break;
                    
                case '5': // OK - Run script
                    if (fileCount > 0) {
                        runSelectedScript();
                    }
                    break;
            }
            break;
            
        case RUNNING:
            // Allow exiting with any key for now
            if (key == '*' || key == '#') {
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
