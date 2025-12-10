// [Revision: v1.0] [Path: src/apps/file_browser.h] [Date: 2025-12-10]
// Description: File browser app with dual storage (SPIFFS/SD) and context menu support.

#ifndef APP_FILE_BROWSER_H
#define APP_FILE_BROWSER_H

#include "../app_interface.h"
#include <vector>

struct FileEntry {
  String name;      // Filename
  bool isDir;       // True if directory (we'll ignore dirs for SPIFFS-only)
  bool isParent;    // Reserved (not used)
  size_t size;      // File size in bytes (used for alternate sorting)
  unsigned long mtime; // Placeholder for modified time (SPIFFS may not provide)
};

enum MenuState {
  MENU_CLOSED = 0,
  MENU_FILE_CONTEXT = 1    // Context menu for selected file
};

class FileBrowserApp : public App {
  private:
    static const int VISIBLE_ITEMS = 4;
    static const int HEADER_HEIGHT = 12;
    static const int LINE_HEIGHT = 10;
    static const int START_Y = 24;
    static const int MAX_FILENAME_DISPLAY = 20;

    std::vector<FileEntry> fileList;
    String currentPath;

    int selectedIndex;
    int scrollOffset;
    
    MenuState menuState;
    int menuSelectedIndex;  // Selection within context menu
    
    bool spiffsAvailable;
    bool awaitingSaveConfirmation;
    // Temporary save feedback message (displayed for a short time)
    String saveMessage;
    unsigned long saveMessageUntil;
    
    enum SortMode { SORT_NAME = 0, SORT_MTIME = 1 };
    SortMode sortMode;

    // Private helper methods
    void scanDirectory(const String& path);
    String truncateFilename(const String& name, int maxLen);
    void openFileContextMenu();
    void closeMenu();
    void handleFileAction(int actionIndex);

  public:
    FileBrowserApp();
    
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
};

#endif
