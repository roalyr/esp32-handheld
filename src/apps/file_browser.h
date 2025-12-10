// [Revision: v1.0] [Path: src/apps/file_browser.h] [Date: 2025-12-10]
// Description: File browser app with dual storage (SPIFFS/SD) and context menu support.

#ifndef APP_FILE_BROWSER_H
#define APP_FILE_BROWSER_H

#include "../app_interface.h"
#include <vector>

struct FileEntry {
    String name;      // Filename
    bool isDir;       // True if directory, false if file
    bool isParent;    // True if ".." entry
};

enum StorageType {
    STORAGE_SPIFFS = 0,
    STORAGE_SDCARD = 1
};

enum MenuState {
    MENU_CLOSED = 0,
    MENU_FILE_CONTEXT = 1,    // Context menu for selected file
    MENU_FOLDER_OPS = 2       // Folder operations menu
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
    StorageType currentStorage;
    
    int selectedIndex;
    int scrollOffset;
    
    MenuState menuState;
    int menuSelectedIndex;  // Selection within context menu
    
    bool spiffsAvailable;
    bool sdAvailable;

    // Private helper methods
    void scanDirectory(const String& path);
    void switchStorage(StorageType newStorage);
    String truncateFilename(const String& name, int maxLen);
    void openFileContextMenu();
    void openFolderMenu();
    void closeMenu();
    void handleFileAction(int actionIndex);
    void handleFolderAction(int actionIndex);
    void navigateToParent();
    void navigateToDirectory(const String& dirName);

  public:
    FileBrowserApp();
    
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
};

#endif
