// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/apps/settings.h
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TACTICAL_TODO TASKs 5, 7, 12
// LOG_REF: 2026-04-02
// Description: Settings app with brightness, sleep, key tester, T9 editor.

#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include "../app_interface.h"
#include "../t9_predict.h"

class SettingsApp : public App {
  private:
    enum SettingItem {
        SETTING_BRIGHTNESS = 0,
        SETTING_SLEEP,
        SETTING_KEY_TESTER,
        SETTING_T9_EDITOR,
        SETTING_COUNT
    };
    
    int selectedIndex;
    bool editMode;
    bool inKeyTester;
    bool inT9Editor;
    
    // Editable values
    int tempBrightness;
    bool tempSleepEnabled;

    // Key tester state
    char lastPressedKey;
    static const int HISTORY_SIZE = 14;
    char keyHistory[HISTORY_SIZE + 1];
    void addToHistory(char c);

    // T9 editor state
    T9Predict t9predict;
    String t9Text;             // Committed text
    int t9Cursor;              // Byte position in t9Text
    int t9ShiftMode;           // 0=lower, 1=firstCap, 2=allCaps
    enum T9InputMode { MODE_T9, MODE_ABC, MODE_123 };
    T9InputMode t9InputMode;   // Current input mode
    char t9TapKey;             // Current multi-tap key
    int t9TapIndex;            // Cycle index in multi-tap
    unsigned long t9TapTime;   // Last tap timestamp
    int t9ScrollOffset;        // Vertical scroll offset for text area
    unsigned long t9CursorMoveTime; // Last cursor move timestamp (suppresses blink)

    void t9EditorReset();
    void t9HandleInput(char key);
    void t9CommitPrediction();  // Confirm predicted word into text
    void t9CommitMultiTap();    // Confirm multi-tap char into text
    String t9GetMultiTapChar() const;
    void t9MoveCursorVertically(int dir); // Vertical cursor movement

    // Sub-renderers
    void renderSettingsList();
    void renderKeyTester();
    void renderT9Editor();
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
