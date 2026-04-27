// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/apps/t9_editor.h
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TACTICAL_TODO TASK_2
// LOG_REF: 2026-04-27
// Description: Canonical T9 editor/viewer app with read-only support and source seam.

#ifndef APP_T9_EDITOR_H
#define APP_T9_EDITOR_H

#include "../app_interface.h"
#include "../t9_predict.h"
#include <vector>

struct VisualLine {
  String content;
  int logicalLineNum;
  bool hasCursor;
  int byteStartIndex;
  int byteLength;
};

enum T9ViewMode {
  VIEW_FULL,
  VIEW_FULL_LINENO,
  VIEW_MIN_LINENO,
  VIEW_MIN
};

enum EditorOpenMode {
  OPEN_READ_WRITE,
  OPEN_READ_ONLY
};

enum DocumentSourceKind {
  SOURCE_BUFFER,
  SOURCE_PAGED_PLACEHOLDER
};

enum T9InputMode {
  MODE_T9,
  MODE_ABC,
  MODE_123
};

class T9EditorApp : public App {
  private:
    std::vector<VisualLine> visualLines;
  int scrollOffset;

  EditorOpenMode openMode;
  DocumentSourceKind sourceKind;
  int sourcePageSize;
  String documentLabel;
  String documentPath;
  String documentBuffer;

  T9Predict t9predict;
  int cursorPos;
  int shiftMode;
  T9InputMode inputMode;
  char tapKey;
  int tapIndex;
  unsigned long tapTime;
  unsigned long cursorMoveTime;
  T9ViewMode viewMode;
  bool savePromptActive;
  int savePromptSelection;
  int singleKeyCycleIndex;
  bool zeroPending;
  bool fallback;
  int fallbackStart;

  void resetEditorSession();
  String readDocumentSlice(int start, int length) const;
  int getDocumentLength() const;
  bool isReadOnly() const;
  bool showHeader() const;
  bool showFooter() const;
  bool showLineNumbers() const;
  int getVisibleLineCount() const;
  int countLogicalLines() const;
  int getGutterWidth(int logicalLineCount) const;
  int getTextLeft(int logicalLineCount) const;
  String getPreviewText() const;
  String getDisplayText(String* previewOut = nullptr) const;
  void requestExit(bool saveRequested);
  String getMultiTapChar() const;
  void commitMultiTap();
  void commitPrediction();
    void recalculateLayout();
  void moveCursorVertically(int dir);
  void handleSavePromptInput(char key);
    void renderHeader();
  void renderFooter() const;
  void renderSavePrompt();

  public:
    T9EditorApp();

  bool exitRequested;

    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
};

#endif