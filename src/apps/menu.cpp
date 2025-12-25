// [Revision: v4.2] [Path: src/apps/menu.cpp] [Date: 2025-12-11]
// Description: Hierarchical main menu with categories - uses unified GUI module. Shows clock in header.
//              Submenus show item count (e.g., "1/4") in header.

#include "menu.h"
#include "../gui.h"
#include "../clock.h"

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
    int visibleItems = (currentLevel == LEVEL_SUBMENU) ? 3 : 4;
    
    GUI::ScrollState state = {selectedIndex, scrollOffset, count, visibleItems};
    
    if (GUI::handleListNavigation(state, key)) {
        selectedIndex = state.selectedIndex;
        scrollOffset = state.scrollOffset;
    }
    
    // ENTER - select item
    if (key == KEY_ENTER) {
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
    
    // ESC - go back to parent menu
    if (key == KEY_ESC && currentLevel == LEVEL_SUBMENU) {
        currentLevel = LEVEL_ROOT;
        currentSubmenu = -1;
        selectedIndex = 0;
        scrollOffset = 0;
    }
}

void MenuApp::render() {
    // Header with clock on right side (root) or item count (submenu)
    char infoStr[16];
    int count = getCurrentMenuCount();
    
    if (currentLevel == LEVEL_ROOT) {
        SystemClock::getTimeString(infoStr, sizeof(infoStr));
    } else {
        // Show "1/4" style counter in submenu
        snprintf(infoStr, sizeof(infoStr), "%d/%d", selectedIndex + 1, count);
    }
    GUI::drawHeader(getCurrentTitle(), infoStr);
    
    // Build labels array for drawList
    MenuItem* menu = getCurrentMenu();
    int visibleItems = (currentLevel == LEVEL_SUBMENU) ? 3 : 4;
    
    if (menu && count > 0) {
        const char* labels[10]; // Max items in any menu
        for (int i = 0; i < count && i < 10; i++) {
            labels[i] = menu[i].label;
        }
        GUI::ListConfig cfg;
        cfg.visibleItems = visibleItems;
        GUI::drawList(labels, count, selectedIndex, scrollOffset, cfg);
    }
    
    // Footer hints
    if (currentLevel == LEVEL_ROOT) {
        GUI::drawFooterHints("Enter:Open", "");
    } else {
        GUI::drawFooterHints("Enter:Sel", "Esc:Back");
    }
}