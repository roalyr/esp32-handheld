// [Revision: v3.2] [Path: src/apps/menu.h] [Date: 2025-12-11]
// Description: Hierarchical main menu with categories. Shows clock in header.
//              Uses APP_ID constants from config.h.

#ifndef APP_MENU_H
#define APP_MENU_H

#include "../app_interface.h"
#include "../config.h"

// Menu item types
enum MenuItemType {
    ITEM_CATEGORY = 0,  // Opens a submenu
    ITEM_APP = 1        // Launches an app
};

// Menu item structure
struct MenuItem {
    const char* label;
    MenuItemType type;
    int appIndex;       // For ITEM_APP: which app to launch
    int submenuIndex;   // For ITEM_CATEGORY: which submenu to show
};

// Menu level (for navigation)
enum MenuLevel {
    LEVEL_ROOT = 0,
    LEVEL_SUBMENU = 1
};

class MenuApp : public App {
  private:
    // Root menu categories
    static const int ROOT_COUNT = 2;
    MenuItem rootMenu[ROOT_COUNT] = {
        {"Tools", ITEM_CATEGORY, -1, 0},
        {"Files", ITEM_CATEGORY, -1, 1}
    };
    
    // Tools submenu
    static const int TOOLS_COUNT = 4;
    MenuItem toolsMenu[TOOLS_COUNT] = {
        {"Key Tester", ITEM_APP, APP_KEY_TESTER, -1},
        {"GFX Test", ITEM_APP, APP_GFX_TEST, -1},
        {"Stopwatch", ITEM_APP, APP_STOPWATCH, -1},
        {"Clock", ITEM_APP, APP_CLOCK, -1}
    };
    
    // Files submenu
    static const int FILES_COUNT = 2;
    MenuItem filesMenu[FILES_COUNT] = {
        {"File Browser", ITEM_APP, APP_FILE_BROWSER, -1},
        {"Lua Runner", ITEM_APP, APP_LUA_RUNNER, -1}
    };
    
    // Navigation state
    MenuLevel currentLevel;
    int currentSubmenu;     // Which submenu is active (0=Tools, 1=Files)
    int selectedIndex;
    int scrollOffset;
    int pendingSwitchIndex;
    
    // Helper methods
    int getCurrentMenuCount();
    MenuItem* getCurrentMenu();
    const char* getCurrentTitle();

  public:
    MenuApp();
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
    int getPendingSwitch();
    bool isInSubmenu() { return currentLevel == LEVEL_SUBMENU; }
};

#endif