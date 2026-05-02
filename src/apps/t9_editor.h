// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/apps/t9_editor.h
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TACTICAL_TODO TASK_2
// LOG_REF: 2026-05-02
// Description: Canonical T9 editor/viewer app with split RO paging and capped RW editing.

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

struct EditorUndoState {
  String documentBuffer;
  int cursorPos;
  bool selectionMode;
  int selectionAnchorPos;
  int selectionFocusPos;
  unsigned long activeHistorySnapshotId;
};

struct ClipboardPopupEntry {
  int slot;
  String preview;
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
  SOURCE_PAGED_FILE
};

enum T9InputMode {
  MODE_T9,
  MODE_ABC,
  MODE_123
};

static const size_t kT9EditorDefaultReadOnlyPageBytes = 512;
static const size_t kT9EditorReadWriteMaxBytes = 16 * 1024;

extern const size_t kT9EditorReadOnlyPageSizeOptions[];
extern const int kT9EditorReadOnlyPageSizeOptionCount;
extern const char* kT9EditorFontSizeOptionLabels[];
extern const int kT9EditorFontSizeOptionCount;

size_t getT9EditorReadOnlyPageBytes();
int getT9EditorReadOnlyPageSizeOptionIndex();
size_t getT9EditorReadOnlyPageSizeOption(int index);
bool setT9EditorReadOnlyPageSizeOptionIndex(int index);
int getT9EditorFontSizeOptionIndex();
const char* getT9EditorFontSizeOptionLabel(int index);
bool setT9EditorFontSizeOptionIndex(int index);

class T9EditorApp : public App {
  private:
    std::vector<VisualLine> visualLines;
  int scrollOffset;

  EditorOpenMode openMode;
  DocumentSourceKind sourceKind;
  int sourcePageSize;
  String documentLabel;
  String documentPath;
  String sourceBuffer;
  String documentBuffer;
  size_t pagedDocumentSize;
  int currentPageIndex;
  int totalPageCount;
  bool pageDirty;
  String historyDocumentId;
  unsigned long historyNextSnapshotId;
  unsigned long activeHistorySnapshotId;
  int nextClipboardSlot;

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
  bool selectionMode;
  bool shiftTapPending;
  int selectionAnchorPos;
  int selectionFocusPos;
  std::vector<EditorUndoState> undoStack;
  std::vector<EditorUndoState> redoStack;
  bool clipboardPopupActive;
  int clipboardPopupSelection;
  int clipboardPopupScroll;
  std::vector<ClipboardPopupEntry> clipboardPopupEntries;

  void resetEditorSession();
  void resetPagedSession();
  void clearSelectionState();
  void enterSelectionMode();
  void exitSelectionMode();
  void syncSelectionFocusToCursor();
  bool hasSelectionRange() const;
  int getSelectionStart() const;
  int getSelectionEnd() const;
  void clearTransientInputState();
  EditorUndoState captureUndoState() const;
  void restoreUndoState(const EditorUndoState& state);
  void pushUndoState(std::vector<EditorUndoState>& stack, const EditorUndoState& state);
  void recordUndoState();
  bool undoFromHistory();
  bool undoEdit();
  bool redoEdit();
  String getSelectedText() const;
  bool replaceDocumentRange(int start, int end, const String& replacement, int newCursorPos,
                            bool recordUndo = true);
  bool removeDocumentRange(int start, int end, bool recordUndo = true);
  String previewClipboardText(const String& value) const;
  bool readClipboardSlotPreview(int slot, String& preview, String& error) const;
  bool rebuildClipboardPopupEntries(String& error);
  bool openClipboardPopup();
  void closeClipboardPopup();
  bool applyClipboardEntry(int entryIndex);
  bool copySelectionToClipboard(bool cutSelection);
  void handleClipboardPopupInput(char key);
  int getClipboardPopupVisibleRows() const;
  int getClipboardPopupTopY() const;
  int getTextBottomY() const;
  void renderClipboardPopup() const;
  String readDocumentSlice(int start, int length) const;
  int getDocumentLength() const;
  void showReadWriteCapToast() const;
  bool tryInsertTextAtCursor(const String& text, int cursorAdvance);
  bool tryInsertCharWithAutoBracket(char c);
  bool statPagedDocument(size_t& fileSize, String& error) const;
  bool readFileRange(size_t start, size_t length, String& out, String& error) const;
  bool loadPagedDocument(String& error);
  bool loadPageByIndex(int pageIndex, String& error);
  void updatePagedDocumentMetrics(size_t fileSize);
  void markPageDirty();
  bool saveCurrentPage(String& error);
  bool ensureEditorStorage(String& error);
  bool ensureHistoryDocument(String& error);
  bool recordPageSnapshot(const char* reason, String& error);
  bool loadClipboardState(String& error);
  bool storeClipboardState(String& error);
  bool writeClipboardSlot(const String& content, int& writtenSlot, String& error);
  bool readClipboardSlot(int slot, String& content, String& error) const;
  String getEditorSystemRoot() const;
  String getHistoryRoot() const;
  String getClipboardRoot() const;
  String getDocumentHistoryRoot() const;
  String getDocumentManifestPath() const;
  String getHistorySnapshotPath(unsigned long snapshotId) const;
  String getClipboardManifestPath() const;
  String getClipboardSlotPath(int slot) const;
  String buildDefaultHistoryDocumentId() const;
  bool isReadOnlyPaged() const;
  bool hasPreviousPage() const;
  bool hasNextPage() const;
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
  bool isKeyActiveNow(char key) const;
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