// [Revision: v1.0] [Path: src/apps/settings.h] [Date: 2025-12-11]
// Description: Settings app for contrast adjustment and system info.

#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include "../app_interface.h"

class SettingsApp : public App {
  private:
    enum SettingItem {
        SETTING_CONTRAST = 0,
        SETTING_SLEEP,
        SETTING_INFO,
        SETTING_COUNT
    };
    
    int selectedIndex;
    bool editMode;
    
    // Editable values
    int tempContrast;
    bool tempSleepEnabled;

  public:
    SettingsApp();
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
};

#endif
