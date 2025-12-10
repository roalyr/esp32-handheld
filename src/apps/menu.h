// [Revision: v1.0] [Path: src/apps/menu.h] [Date: 2025-12-10]
// Description: Main Menu Application. Allows selecting and launching other apps.

#ifndef APP_MENU_H
#define APP_MENU_H

#include "../app_interface.h"

class MenuApp : public App {
  private:
    static const int ITEM_COUNT = 4;
    const char* menuItems[ITEM_COUNT] = {
      "1. T9 Editor",
      "2. Key Tester",
      "3. Snake Game",
      "4. GFX Test"
    };

    int selectedIndex;
    int pendingSwitchIndex; // -1 if no switch, 0-3 if selection made

  public:
    MenuApp();
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;

    // Helper for main.cpp to check if we need to change apps
    int getPendingSwitch(); 
};

#endif