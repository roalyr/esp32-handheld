// [Revision: v4.0] [Path: src/apps/menu.cpp] [Date: 2025-12-11]
// Description: Hierarchical main menu with categories - uses unified GUI module.

#include "menu.h"
#include "../gui.h"

MenuApp::MenuApp() {
    currentLevel = LEVEL_ROOT;
    currentSubmenu = -1;
    selectedIndex = 0;
    scrollOffset = 0;
    pendingSwitchIndex = -1;
}

void MenuApp::start() {
    u8g2.setContrast(systemContrast);
    pendingSwitchIndex = -1;
    // Reset to root menu on start
    currentLevel = LEVEL_ROOT;
    currentSubmenu = -1;
    selectedIndex = 0;
    scrollOffset = 0;
}

void MenuApp::stop() {}
void MenuApp::update() {}

int MenuApp::getPendingSwitch() {
    int temp = pendingSwitchIndex;
    pendingSwitchIndex = -1;
    return temp;
}

int MenuApp::getCurrentMenuCount() {
    if (currentLevel == LEVEL_ROOT) {
        return ROOT_COUNT;
    } else {
        switch (currentSubmenu) {
            case 0: return TOOLS_COUNT;
            case 1: return FILES_COUNT;
            default: return 0;
        }
    }
}

MenuItem* MenuApp::getCurrentMenu() {
    if (currentLevel == LEVEL_ROOT) {
        return rootMenu;
    } else {
        switch (currentSubmenu) {
            case 0: return toolsMenu;
            case 1: return filesMenu;
            default: return nullptr;
        }
    }
}

const char* MenuApp::getCurrentTitle() {
    if (currentLevel == LEVEL_ROOT) {
        return "MAIN MENU";
    } else {
        switch (currentSubmenu) {
            case 0: return "TOOLS";
            case 1: return "FILES";
            default: return "MENU";
        }
    }
}

void MenuApp::handleInput(char key) {
    int count = getCurrentMenuCount();
    
    GUI::ScrollState state = {selectedIndex, scrollOffset, count, 4};
    
    if (GUI::handleListNavigation(state, key)) {
        selectedIndex = state.selectedIndex;
        scrollOffset = state.scrollOffset;
    }
    
    // ENTER (5) - select item
    if (key == '5') {
        MenuItem* menu = getCurrentMenu();
        if (menu && selectedIndex < count) {
            MenuItem& item = menu[selectedIndex];
            if (item.type == ITEM_CATEGORY) {
                // Enter submenu
                currentLevel = LEVEL_SUBMENU;
                currentSubmenu = item.submenuIndex;
                selectedIndex = 0;
                scrollOffset = 0;
            } else if (item.type == ITEM_APP) {
                // Launch app
                pendingSwitchIndex = item.appIndex;
            }
        }
    }
    
    // BACK (D) - go back to parent menu
    if (key == 'D' && currentLevel == LEVEL_SUBMENU) {
        currentLevel = LEVEL_ROOT;
        currentSubmenu = -1;
        selectedIndex = 0;
        scrollOffset = 0;
    }
}

void MenuApp::render() {
    GUI::drawHeader(getCurrentTitle());
    
    // Build labels array for drawList
    MenuItem* menu = getCurrentMenu();
    int count = getCurrentMenuCount();
    
    if (menu && count > 0) {
        const char* labels[10]; // Max items in any menu
        for (int i = 0; i < count && i < 10; i++) {
            labels[i] = menu[i].label;
        }
        GUI::drawList(labels, count, selectedIndex, scrollOffset);
    }
    
    // Footer hints
    if (currentLevel == LEVEL_ROOT) {
        GUI::drawFooterHints("5:Open", "");
    } else {
        GUI::drawFooterHints("5:Select", "D:Back");
    }
}