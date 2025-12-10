// [Revision: v2.0] [Path: src/apps/t9_editor.h] [Date: 2025-12-10]
// Description: T9 Editor Application class definition.

#ifndef APP_T9_EDITOR_H
#define APP_T9_EDITOR_H

#include "../app_interface.h"
#include "../t9_engine.h"

class T9EditorApp : public App {
  public:
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
};

#endif