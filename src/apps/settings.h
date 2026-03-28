// [Revision: v2.0] [Path: src/apps/settings.h] [Date: 2026-03-28]
// Description: Settings app with brightness, sleep, key tester.

#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include "../app_interface.h"

class SettingsApp : public App {
  private:
    enum SettingItem {
        SETTING_BRIGHTNESS = 0,
        SETTING_SLEEP,
        SETTING_KEY_TESTER,
        SETTING_COUNT
    };
    
    int selectedIndex;
    bool editMode;
    bool inKeyTester;
    
    // Editable values
    int tempBrightness;
    bool tempSleepEnabled;

    // Key tester state
    char lastPressedKey;
    static const int HISTORY_SIZE = 14;
    char keyHistory[HISTORY_SIZE + 1];
    void addToHistory(char c);

    // Sub-renderers
    void renderSettingsList();
    void renderKeyTester();
    void renderInfoHeader();

  public:
    SettingsApp();
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
    bool isInSubmenu();
};

#endif
