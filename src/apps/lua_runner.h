// [Revision: v1.1] [Path: src/apps/lua_runner.h] [Date: 2025-12-11]
// Description: App for browsing and running Lua scripts from SPIFFS.

#ifndef LUA_RUNNER_H
#define LUA_RUNNER_H

#include "../app_interface.h"

class LuaRunnerApp : public App {
public:
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;

private:
    enum Mode { BROWSE, RUNNING, ERROR };
    Mode mode;
    
    // File browsing state
    static constexpr int MAX_FILES = 32;
    String files[MAX_FILES];
    int fileCount;
    int selectedIndex;
    int scrollOffset;
    
    // Script execution state
    String currentScript;
    String errorMessage;
    
    void scanForLuaFiles();
    void drawBrowser();
    void drawRunning();
    void drawError();
    void runSelectedScript();
};

#endif
