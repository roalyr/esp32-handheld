// [Revision: v2.0] [Path: src/apps/key_tester.h] [Date: 2025-12-10]
// Description: Key Tester Application class definition.

#ifndef APP_KEY_TESTER_H
#define APP_KEY_TESTER_H

#include "../app_interface.h"

class KeyTesterApp : public App {
  private:
    char lastPressedKey;
    static const int HISTORY_SIZE = 14;
    char keyHistory[HISTORY_SIZE + 1];

    void addToHistory(char c);

  public:
    KeyTesterApp(); // Constructor to init buffers
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
};

#endif