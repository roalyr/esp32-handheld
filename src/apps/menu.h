// [Revision: v2.3] [Path: src/apps/menu.h] [Date: 2025-12-10]
// Description: Added File Browser menu item.

#ifndef APP_MENU_H
#define APP_MENU_H

#include "../app_interface.h"

class MenuApp : public App {
  private:
    static const int ITEM_COUNT = 6;
    const char* menuItems[ITEM_COUNT] = {
      "1. Key Tester",
      "2. Snake Game",
      "3. GFX Test",
      "4. Asteroids",
      "5. Stopwatch",
      "6. File Browser"
    };

    int selectedIndex;
    int scrollOffset; // Index of the first visible item
    int pendingSwitchIndex; 

  public:
    MenuApp();
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
    int getPendingSwitch(); 
};

#endif