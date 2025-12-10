// [Revision: v2.2] [Path: src/apps/menu.h] [Date: 2025-12-10]
// Description: Menu Header. Added scroll tracking.

#ifndef APP_MENU_H
#define APP_MENU_H

#include "../app_interface.h"

class MenuApp : public App {
  private:
    static const int ITEM_COUNT = 6;
    const char* menuItems[ITEM_COUNT] = {
      "1. T9 Editor",
      "2. Key Tester",
      "3. Snake Game",
      "4. GFX Test",
      "5. Asteroids",
      "6. Stopwatch"
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