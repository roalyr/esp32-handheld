// [Revision: v4.1] [Path: src/apps/t9_editor.h] [Date: 2025-12-10]
// Description: Added exitRequested flag and refined cursor logic.

#ifndef APP_T9_EDITOR_H
#define APP_T9_EDITOR_H

#include "../app_interface.h"
#include "../t9_engine.h"
#include <vector>

struct VisualLine {
    String content;     
    int logicalLineNum; 
    bool hasCursor;     
    int byteStartIndex; // Index in textBuffer where this line starts
    int byteLength;     // Length of this line in bytes
};

enum EditorState {
    STATE_EDITING,
    STATE_HELP,
    STATE_EXIT_CONFIRM
};

class T9EditorApp : public App {
  private:
    // Layout State
    std::vector<VisualLine> visualLines;
    int scrollOffset;   
    
    // Caching
    String lastProcessedText;
    bool lastPendingState;
    String lastCandidate;
    int lastCursorPos;

    // Config
    const int LINE_HEIGHT = 13; 
    const int HEADER_HEIGHT = 12;
    const int GUTTER_WIDTH = 18;
    const int TEXT_AREA_WIDTH = 128 - GUTTER_WIDTH - 2; 
    const int VISIBLE_LINES = (64 - HEADER_HEIGHT) / LINE_HEIGHT;

    // New UI State
    EditorState currentState;
    String fileName;
    int helpScrollY;
    bool exitSelection; // true=YES, false=NO

    // Internal Helpers
    void recalculateLayout();
    void moveCursorVertically(int dir); // -1 Up, +1 Down
    void renderHeader();
    void renderHelpPopup();
    void renderExitPopup();

  public:
    T9EditorApp();
    
    // Communication flag for Main loop
    bool exitRequested; 

    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
};

#endif