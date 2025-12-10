// [Revision: v1.0] [Path: src/apps/file_browser.cpp] [Date: 2025-12-10]
// Description: File browser implementation with dual storage and context menus.

#include "file_browser.h"
#include <FS.h>
#include <SPIFFS.h>
#include <SD.h>

FileBrowserApp::FileBrowserApp() {
    selectedIndex = 0;
    scrollOffset = 0;
    menuState = MENU_CLOSED;
    menuSelectedIndex = 0;
    currentPath = "/";
    currentStorage = STORAGE_SPIFFS;
    spiffsAvailable = false;
    sdAvailable = false;
}

void FileBrowserApp::start() {
    u8g2.setContrast(systemContrast);
    
    // Check storage availability
    spiffsAvailable = SPIFFS.totalBytes() > 0;
    sdAvailable = SD.cardSize() > 0;
    
    // Default to SPIFFS if available, otherwise SD
    if (spiffsAvailable) {
        currentStorage = STORAGE_SPIFFS;
    } else if (sdAvailable) {
        currentStorage = STORAGE_SDCARD;
    }
    
    currentPath = "/";
    selectedIndex = 0;
    scrollOffset = 0;
    menuState = MENU_CLOSED;
    
    scanDirectory(currentPath);
}

void FileBrowserApp::stop() {}

void FileBrowserApp::update() {}

String FileBrowserApp::truncateFilename(const String& name, int maxLen) {
    if (name.length() <= maxLen) return name;
    return name.substring(0, maxLen - 1) + "~";
}

void FileBrowserApp::scanDirectory(const String& path) {
    fileList.clear();
    selectedIndex = 0;
    scrollOffset = 0;
    menuState = MENU_CLOSED;
    
    // Add ".." parent directory entry (unless at root)
    if (path != "/") {
        FileEntry parentEntry;
        parentEntry.name = "..";
        parentEntry.isDir = true;
        parentEntry.isParent = true;
        fileList.push_back(parentEntry);
    }
    
    // Open directory based on current storage
    File dir;
    if (currentStorage == STORAGE_SPIFFS) {
        dir = SPIFFS.open(path);
    } else {
        dir = SD.open(path);
    }
    
    if (!dir || !dir.isDirectory()) {
        return;
    }
    
    // Scan directory entries
    File file = dir.openNextFile();
    while (file) {
        FileEntry entry;
        entry.name = String(file.name());
        entry.isDir = file.isDirectory();
        entry.isParent = false;
        
        // Extract just the filename (remove leading path)
        int lastSlash = entry.name.lastIndexOf('/');
        if (lastSlash >= 0) {
            entry.name = entry.name.substring(lastSlash + 1);
        }
        
        if (entry.name.length() > 0) {
            fileList.push_back(entry);
        }
        
        file = dir.openNextFile();
    }
    dir.close();
}

void FileBrowserApp::navigateToParent() {
    if (currentPath == "/") return;
    
    int lastSlash = currentPath.lastIndexOf('/');
    if (lastSlash == 0) {
        currentPath = "/";
    } else {
        currentPath = currentPath.substring(0, lastSlash);
    }
    
    scanDirectory(currentPath);
}

void FileBrowserApp::navigateToDirectory(const String& dirName) {
    String newPath = currentPath;
    if (newPath != "/") newPath += "/";
    newPath += dirName;
    
    currentPath = newPath;
    scanDirectory(currentPath);
}

void FileBrowserApp::switchStorage(StorageType newStorage) {
    // Check if target storage is available
    if (newStorage == STORAGE_SPIFFS && !spiffsAvailable) return;
    if (newStorage == STORAGE_SDCARD && !sdAvailable) return;
    
    currentStorage = newStorage;
    currentPath = "/";
    scanDirectory(currentPath);
}

void FileBrowserApp::openFileContextMenu() {
    menuState = MENU_FILE_CONTEXT;
    menuSelectedIndex = 0;
}

void FileBrowserApp::openFolderMenu() {
    menuState = MENU_FOLDER_OPS;
    menuSelectedIndex = 0;
}

void FileBrowserApp::closeMenu() {
    menuState = MENU_CLOSED;
    menuSelectedIndex = 0;
}

void FileBrowserApp::handleFileAction(int actionIndex) {
    // TODO: Implement file actions (view, edit, delete)
    // For now, close the menu
    closeMenu();
}

void FileBrowserApp::handleFolderAction(int actionIndex) {
    // TODO: Implement folder actions (create file, create folder, rename, delete)
    // For now, close the menu
    closeMenu();
}

void FileBrowserApp::handleInput(char key) {
    // Handle menu input first
    if (menuState != MENU_CLOSED) {
        if (key == 'D') {  // HOME key: close menu
            closeMenu();
            return;
        }
        
        if (key == '2') {  // UP
            menuSelectedIndex--;
            if (menuSelectedIndex < 0) menuSelectedIndex = 0;
            return;
        }
        
        if (key == '8') {  // DOWN
            if (menuState == MENU_FILE_CONTEXT) {
                if (menuSelectedIndex < 2) menuSelectedIndex++;  // 3 file actions (0-2)
            } else if (menuState == MENU_FOLDER_OPS) {
                if (menuSelectedIndex < 3) menuSelectedIndex++;  // 4 folder actions (0-3)
            }
            return;
        }
        
        if (key == '5') {  // SELECT: execute action
            if (menuState == MENU_FILE_CONTEXT) {
                handleFileAction(menuSelectedIndex);
            } else {
                handleFolderAction(menuSelectedIndex);
            }
            return;
        }
        
        return;  // Ignore other keys while menu is open
    }
    
    // Normal file browser input
    if (key == '2') {  // UP
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
    
    if (key == '8') {  // DOWN
        selectedIndex++;
        if (selectedIndex >= (int)fileList.size()) {
            selectedIndex = 0;
            scrollOffset = 0;
        } else if (selectedIndex >= scrollOffset + VISIBLE_ITEMS) {
            scrollOffset++;
        }
        return;
    }
    
    if (key == '5') {  // SELECT: open file or show context menu
        if (selectedIndex < (int)fileList.size()) {
            FileEntry& entry = fileList[selectedIndex];
            
            if (entry.isParent) {
                navigateToParent();
            } else if (entry.isDir) {
                navigateToDirectory(entry.name);
            } else {
                // Show context menu for file
                openFileContextMenu();
            }
        }
        return;
    }
    
    if (key == '1') {  // SWITCH TO SPIFFS
        if (spiffsAvailable) switchStorage(STORAGE_SPIFFS);
        return;
    }
    
    if (key == '3') {  // SWITCH TO SDCARD
        if (sdAvailable) switchStorage(STORAGE_SDCARD);
        return;
    }
    
    if (key == 'C') {  // FOLDER OPERATIONS
        openFolderMenu();
        return;
    }
}

void FileBrowserApp::render() {
    u8g2.setFont(FONT_SMALL);
    
    // 1. Draw Header
    u8g2.drawBox(0, 0, 128, HEADER_HEIGHT);
    u8g2.setDrawColor(0);
    
    String headerText = (currentStorage == STORAGE_SPIFFS) ? "SPIFFS" : "SDCARD";
    u8g2.drawStr(2, 9, headerText.c_str());
    
    u8g2.setDrawColor(1);
    
    // 2. Draw current path (line 2)
    String pathDisplay = truncateFilename(currentPath, 25);
    u8g2.drawStr(2, 20, pathDisplay.c_str());
    
    // 3. Draw file list
    for (int i = 0; i < VISIBLE_ITEMS && scrollOffset + i < (int)fileList.size(); i++) {
        int y = START_Y + i * LINE_HEIGHT;
        int listIndex = scrollOffset + i;
        
        // Draw selection highlight
        if (listIndex == selectedIndex) {
            u8g2.drawBox(0, y - 8, 128, LINE_HEIGHT);
            u8g2.setDrawColor(0);
        }
        
        // Draw file/folder indicator and name
        String displayName = truncateFilename(fileList[listIndex].name, MAX_FILENAME_DISPLAY);
        if (fileList[listIndex].isDir) {
            displayName = "[" + displayName + "]";
        }
        
        u8g2.drawStr(2, y, displayName.c_str());
        
        if (listIndex == selectedIndex) {
            u8g2.setDrawColor(1);
        }
    }
    
    // 4. Draw scrollbar indicator
    if (fileList.size() > VISIBLE_ITEMS) {
        int barHeight = (VISIBLE_ITEMS * LINE_HEIGHT * VISIBLE_ITEMS) / fileList.size();
        int barY = START_Y + (scrollOffset * LINE_HEIGHT * VISIBLE_ITEMS) / fileList.size();
        u8g2.drawBox(124, barY, 3, barHeight);
    }
    
    // 5. Draw context menu if open
    if (menuState == MENU_FILE_CONTEXT) {
        // Draw semi-transparent overlay (using pattern)
        for (int x = 20; x < 108; x++) {
            for (int y = 20; y < 55; y++) {
                if ((x + y) % 3 == 0) u8g2.drawPixel(x, y);
            }
        }
        
        // Draw menu box
        u8g2.drawFrame(20, 20, 88, 35);
        u8g2.drawBox(20, 20, 88, 10);
        u8g2.setDrawColor(0);
        u8g2.drawStr(30, 27, "FILE MENU");
        u8g2.setDrawColor(1);
        
        // Draw menu items
        const char* fileActions[] = {"View", "Edit", "Delete"};
        for (int i = 0; i < 3; i++) {
            int y = 32 + i * 8;
            if (i == menuSelectedIndex) {
                u8g2.drawBox(22, y - 7, 84, 8);
                u8g2.setDrawColor(0);
            }
            u8g2.drawStr(25, y, fileActions[i]);
            if (i == menuSelectedIndex) {
                u8g2.setDrawColor(1);
            }
        }
    } else if (menuState == MENU_FOLDER_OPS) {
        // Draw semi-transparent overlay
        for (int x = 20; x < 108; x++) {
            for (int y = 20; y < 55; y++) {
                if ((x + y) % 3 == 0) u8g2.drawPixel(x, y);
            }
        }
        
        // Draw menu box
        u8g2.drawFrame(20, 20, 88, 43);
        u8g2.drawBox(20, 20, 88, 10);
        u8g2.setDrawColor(0);
        u8g2.drawStr(22, 27, "FOLDER MENU");
        u8g2.setDrawColor(1);
        
        // Draw menu items
        const char* folderActions[] = {"New File", "New Folder", "Rename", "Delete"};
        for (int i = 0; i < 4; i++) {
            int y = 32 + i * 8;
            if (i == menuSelectedIndex) {
                u8g2.drawBox(22, y - 7, 84, 8);
                u8g2.setDrawColor(0);
            }
            u8g2.drawStr(25, y, folderActions[i]);
            if (i == menuSelectedIndex) {
                u8g2.setDrawColor(1);
            }
        }
    }
}
