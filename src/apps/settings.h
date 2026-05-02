// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/apps/settings.h
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TACTICAL_TODO TASK_1
// LOG_REF: 2026-04-22
// Description: Settings app with brightness, sleep, key tester, T9 editor.

#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include "../app_interface.h"
#include "../t9_predict.h"

class SettingsApp : public App {
  private:
    enum T9ViewMode { VIEW_FULL, VIEW_FULL_LINENO, VIEW_MIN_LINENO, VIEW_MIN };

    enum SettingItem {
      SETTING_SD_REMOUNT = 0,
      SETTING_BRIGHTNESS,
      SETTING_NEW_BLANK_LUA_APP,
      SETTING_RO_PAGE_SIZE,
      SETTING_KEY_TESTER,
      SETTING_LCD_TEST,
      SETTING_SD_TEST,
      SETTING_HELP,
      SETTING_COUNT
    };
    
    int selectedIndex;
    bool editMode;
    bool inKeyTester;
    bool inT9Editor;
    bool inLcdTest;
    int lcdTestStep;       // Current test pattern (0-based)
    bool inSdTest;
    bool sdTestRan;        // True after test has executed
    bool sdRemountPending;
    unsigned long sdRemountDeadline;
    
    // Editable values
    int tempBrightness;
    int tempContrast;
    bool tempSleepEnabled;
    int tempReadOnlyPageSizeIndex;

    // Key tester state
    char lastPressedKey;
    static const int HISTORY_SIZE = 14;
    char keyHistory[HISTORY_SIZE + 1];
    void addToHistory(char c);
    void resetKeyTesterSession();
    uint32_t keyTesterPollBaseline;
    uint32_t keyTesterRawHitBaseline;
    uint32_t keyTesterLatchedBaseline;
    uint32_t keyTesterDuplicateBaseline;
    uint32_t keyTesterDeliveredCount;

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
    T9ViewMode t9ViewMode;
    bool t9SavePromptActive;
    int t9SavePromptSelection;  // 0=No, 1=Yes
    int t9SingleKeyCycleIndex;  // single-digit T9 cycle: letters first, then dictionary
    bool t9ZeroPending;         // pending T9-mode 0 press: tap=space on release, hold=literal 0

    // T9 fallback-ABC mode: when prediction fails mid-word,
    // commit the known part and continue in ABC for the rest.
    bool t9Fallback;           // true = in fallback-ABC mode
    int t9FallbackStart;       // cursor pos where fallback region begins (for inverse render)

    void t9EditorReset();
    void t9HandleInput(char key);
    void t9CommitPrediction();  // Confirm predicted word into text
    void t9CommitMultiTap();    // Confirm multi-tap char into text
    String t9GetMultiTapChar() const;
    void t9MoveCursorVertically(int dir); // Vertical cursor movement
    void t9HandleSavePromptInput(char key);
    void renderT9SavePrompt();

    // Sub-renderers
    void renderSettingsList();
    void renderKeyTester();
    void renderT9Editor();
    void renderInfoHeader();
    void renderLcdTest();
    void renderSdTest();
    void runSdPinDiagnostic();

  public:
    SettingsApp();
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
    bool isInSubmenu();
    static const int LCD_TEST_COUNT = 7;
    
    // SD test result lines (filled by runSdPinDiagnostic)
    static const int SD_TEST_LINES = 12;
    char sdTestResults[SD_TEST_LINES][26];  // 25 chars + null per line
    int sdTestLineCount;
};

#endif
