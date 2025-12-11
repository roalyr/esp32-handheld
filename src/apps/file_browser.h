// [Revision: v2.0] [Path: src/apps/file_browser.h] [Date: 2025-12-11]
// Description: File browser app - refactored with clear separation of concerns.
//              Supports SPIFFS browsing, context menus, file operations, Lua execution.

#ifndef APP_FILE_BROWSER_H
#define APP_FILE_BROWSER_H

#include "../app_interface.h"
#include <vector>

struct FileEntry {
  String name;
  bool isDir;
  bool isParent;
  size_t size;
  unsigned long mtime;
};

enum MenuState {
  MENU_CLOSED = 0,
  MENU_FILE_CONTEXT = 1
};

enum BrowserMode {
  BROWSE_MODE = 0,
  LUA_RUNNING = 1,
  LUA_ERROR = 2
};

class FileBrowserApp : public App {
  private:
    // Layout constants
    static const int VISIBLE_ITEMS = 3;
    static const int MAX_FILENAME_DISPLAY = 20;

    // State
    std::vector<FileEntry> fileList;
    String currentPath;
    int selectedIndex;
    int scrollOffset;
    MenuState menuState;
    int menuSelectedIndex;
    bool spiffsAvailable;
    bool awaitingSaveConfirmation;
    String saveMessage;
    unsigned long saveMessageUntil;
    enum SortMode { SORT_NAME = 0, SORT_MTIME = 1 };
    SortMode sortMode;
    BrowserMode browserMode;
    String currentLuaScript;
    String luaErrorMessage;

    // Initialization
    void resetState();
    
    // App transfer handling
    bool processPendingActions();
    bool handleDeleteReturn();
    bool handleFilenameReturn();
    bool handleEditorReturn();
    void clearTransferState();
    void showMessage(const char* msg, unsigned long duration);

    // File operations
    void scanDirectory(const String& path);
    void sortFileList();
    bool isLuaFile(const String& name);
    String loadFileContent(const String& path);
    bool saveFileContent(const String& path, const String& content);

    // File actions
    void runLuaScript(const String& path);
    void viewFile(const String& path);
    void editFile(const String& path);
    void deleteFile(const String& path, const String& name);
    void createNewFile();
    void handleFileAction(int actionIndex);

    // Menu management
    void openFileContextMenu();
    void closeMenu();
    int getMenuItemCount();

    // Input handling
    void handleLuaRunningInput(char key);
    void handleLuaErrorInput(char key);
    void handleBrowseInput(char key);
    void handleMenuInput(char key);

    // Rendering
    void renderLuaRunning();
    void renderLuaError();
    void renderBrowser();
    void renderHeader();
    void renderFileList();
    void renderScrollbar();
    void renderContextMenu();
    void renderFooter();
    void renderToast();

  public:
    FileBrowserApp();
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
};

#endif
