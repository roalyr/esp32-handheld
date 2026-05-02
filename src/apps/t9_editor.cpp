// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/apps/t9_editor.cpp
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TACTICAL_TODO TASK_1
// LOG_REF: 2026-04-30
// Description: Canonical T9 editor/viewer app with predictive input and paged-file startup.

#include "t9_editor.h"
#include "../app_transfer.h"
#include "../gui.h"
#include <cstdlib>
#include <cstring>

struct EditorRecordHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t kind;
    uint32_t index;
    uint32_t payloadLength;
    uint32_t documentLength;
    uint32_t flags;
    uint32_t checksum;
};

static const size_t kEditorRecordSize = 2048;
static const size_t kEditorRecordPayloadBytes = kEditorRecordSize - sizeof(EditorRecordHeader);
static const uint32_t kEditorRecordMagic = 0x54494348UL;
static const uint32_t kEditorRecordVersion = 1;
static const uint32_t kEditorRecordKindSessionChunk = 1;
static const uint32_t kEditorRecordKindHistorySnapshot = 2;
static const uint32_t kEditorRecordKindClipboardSlot = 3;
static const uint32_t kEditorRecordFlagDirtyExport = 1UL << 0;
static const int kClipboardSlotCount = 12;
static const char* kEditorSystemRoot = "/.t9sys";
static const char* kEditorWorkRoot = "/.t9sys/work";
static const char* kEditorHistoryRoot = "/.t9sys/history";
static const char* kEditorClipboardRoot = "/.t9sys/clipboard";
static const char* kEditorRewriteTempPath = "/.t9sys/rewrite.tmp";
static const char* kEditorRewriteBackupPath = "/.t9sys/rewrite.bak";
const size_t kT9EditorReadOnlyPageSizeOptions[] = {2048, 1024, 512, 256};
const int kT9EditorReadOnlyPageSizeOptionCount = 4;
const char* kT9EditorFontSizeOptionLabels[] = {"Medium", "Small", "Tiny"};
const int kT9EditorFontSizeOptionCount = 3;
static int gT9EditorReadOnlyPageSizeOptionIndex = 2;
static int gT9EditorFontSizeOptionIndex = 0;

struct T9EditorFontMetrics {
    const uint8_t* font;
    int lineHeight;
    int baselineOffset;
    int glyphTopOffset;
    int boxHeight;
    int cursorHeight;
    int underlineOffset;
};

static const T9EditorFontMetrics kT9EditorFontMetrics[] = {
    {u8g2_font_5x8_tr, 9, 8, 7, 9, 8, 2},
    {u8g2_font_5x7_tf, 8, 7, 6, 8, 7, 1},
    {u8g2_font_4x6_tf, 7, 6, 5, 7, 6, 1},
};

static_assert(sizeof(EditorRecordHeader) < kEditorRecordSize,
              "Editor record header must fit within the fixed record size");

class EditorSdSessionGuard {
  public:
    bool begin() {
        if (depth_ == 0) {
            active_ = sdBeginSession();
            if (!active_) return false;
        } else {
            active_ = true;
        }
        depth_++;
        return true;
    }

    ~EditorSdSessionGuard() {
        if (!active_) return;
        depth_--;
        if (depth_ == 0) {
            sdEndSession();
        }
    }

  private:
    static int depth_;
    bool active_ = false;
};

int EditorSdSessionGuard::depth_ = 0;

size_t getT9EditorReadOnlyPageBytes() {
    return kT9EditorReadOnlyPageSizeOptions[gT9EditorReadOnlyPageSizeOptionIndex];
}

int getT9EditorReadOnlyPageSizeOptionIndex() {
    return gT9EditorReadOnlyPageSizeOptionIndex;
}

size_t getT9EditorReadOnlyPageSizeOption(int index) {
    if (index < 0 || index >= kT9EditorReadOnlyPageSizeOptionCount) {
        return kT9EditorDefaultReadOnlyPageBytes;
    }
    return kT9EditorReadOnlyPageSizeOptions[index];
}

bool setT9EditorReadOnlyPageSizeOptionIndex(int index) {
    if (index < 0 || index >= kT9EditorReadOnlyPageSizeOptionCount) {
        return false;
    }
    gT9EditorReadOnlyPageSizeOptionIndex = index;
    return true;
}

int getT9EditorFontSizeOptionIndex() {
    return gT9EditorFontSizeOptionIndex;
}

const char* getT9EditorFontSizeOptionLabel(int index) {
    if (index < 0 || index >= kT9EditorFontSizeOptionCount) {
        return kT9EditorFontSizeOptionLabels[0];
    }
    return kT9EditorFontSizeOptionLabels[index];
}

bool setT9EditorFontSizeOptionIndex(int index) {
    if (index < 0 || index >= kT9EditorFontSizeOptionCount) {
        return false;
    }
    gT9EditorFontSizeOptionIndex = index;
    return true;
}

static const T9EditorFontMetrics& getCurrentFontMetrics() {
    int index = gT9EditorFontSizeOptionIndex;
    if (index < 0 || index >= kT9EditorFontSizeOptionCount) {
        index = 0;
    }
    return kT9EditorFontMetrics[index];
}

static uint32_t fnv1a32(const String& text) {
    uint32_t hash = 2166136261UL;
    for (int i = 0; i < text.length(); i++) {
        hash ^= static_cast<uint8_t>(text[i]);
        hash *= 16777619UL;
    }
    return hash;
}

static uint32_t fnv1aBytes(const uint8_t* data, size_t length) {
    uint32_t hash = 2166136261UL;
    for (size_t i = 0; i < length; i++) {
        hash ^= data[i];
        hash *= 16777619UL;
    }
    return hash;
}

static String getPathTail(const String& path) {
    int slash = path.lastIndexOf('/');
    if (slash < 0 || slash + 1 >= path.length()) return path;
    return path.substring(slash + 1);
}

static String sanitizePathComponent(const String& value) {
    String out = "";
    for (int i = 0; i < value.length(); i++) {
        char c = value[i];
        bool isAlphaNum = (c >= '0' && c <= '9') ||
                          (c >= 'A' && c <= 'Z') ||
                          (c >= 'a' && c <= 'z');
        out += isAlphaNum ? String(c) : String('_');
    }
    while (out.indexOf("__") >= 0) out.replace("__", "_");
    if (out.length() == 0) out = "doc";
    return out;
}

static String parentPathOf(const String& path) {
    int slash = path.lastIndexOf('/');
    if (slash <= 0) return "/";
    return path.substring(0, slash);
}

static bool ensureDirectoryChainUnlocked(const String& path, String& error) {
    if (path.length() == 0 || path == "/") {
        error = "";
        return true;
    }

    String current = "";
    int segmentStart = 1;
    while (segmentStart <= path.length()) {
        int slash = path.indexOf('/', segmentStart);
        String segment = slash >= 0 ? path.substring(segmentStart, slash) : path.substring(segmentStart);
        if (segment.length() > 0) {
            current += "/" + segment;
            if (!sdFat.exists(current.c_str()) && !sdFat.mkdir(current.c_str())) {
                error = String("Failed to create directory: ") + current;
                return false;
            }
        }
        if (slash < 0) break;
        segmentStart = slash + 1;
    }

    error = "";
    return true;
}

static bool readSmallFileUnlocked(const String& path, String& content, String& error) {
    content = "";
    FsFile file;
    if (!file.open(path.c_str(), O_RDONLY)) {
        error = String("Failed to open file: ") + path;
        return false;
    }
    if (file.isDir()) {
        error = String("Path is a directory: ") + path;
        return false;
    }

    size_t fileSize = static_cast<size_t>(file.size());
    if (fileSize > 0 && !content.reserve(static_cast<unsigned int>(fileSize))) {
        error = String("Not enough memory to read file: ") + path;
        return false;
    }

    char buffer[129];
    while (true) {
        int bytesRead = file.read(buffer, sizeof(buffer) - 1);
        if (bytesRead < 0) {
            error = String("Failed to read file: ") + path;
            return false;
        }
        if (bytesRead == 0) break;
        buffer[bytesRead] = '\0';
        if (!content.concat(buffer)) {
            error = String("Failed to append file data: ") + path;
            return false;
        }
    }

    error = "";
    return true;
}

static bool writeSmallFileUnlocked(const String& path, const String& content, String& error) {
    if (!ensureDirectoryChainUnlocked(parentPathOf(path), error)) {
        return false;
    }

    FsFile file;
    if (!file.open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC)) {
        error = String("Failed to open file for write: ") + path;
        return false;
    }
    if (file.isDir()) {
        error = String("Path is a directory: ") + path;
        return false;
    }

    size_t bytesWritten = file.write(reinterpret_cast<const uint8_t*>(content.c_str()), content.length());
    if (bytesWritten != content.length()) {
        error = String("Failed to write file: ") + path;
        return false;
    }
    if (!file.sync()) {
        error = String("Failed to sync file: ") + path;
        return false;
    }

    error = "";
    return true;
}

static String manifestValue(const String& manifest, const char* key) {
    String prefix = String(key) + "=";
    int start = 0;
    while (start <= manifest.length()) {
        int end = manifest.indexOf('\n', start);
        if (end < 0) end = manifest.length();
        String line = manifest.substring(start, end);
        if (line.startsWith(prefix)) {
            return line.substring(prefix.length());
        }
        if (end >= manifest.length()) break;
        start = end + 1;
    }
    return "";
}

static bool manifestContainsAlias(const String& manifest, const String& path) {
    const String prefix = "alias=";
    int start = 0;
    while (start <= manifest.length()) {
        int end = manifest.indexOf('\n', start);
        if (end < 0) end = manifest.length();
        String line = manifest.substring(start, end);
        if (line.startsWith(prefix) && line.substring(prefix.length()) == path) {
            return true;
        }
        if (end >= manifest.length()) break;
        start = end + 1;
    }
    return false;
}

static uint32_t snapshotReasonFlags(const char* reason) {
    if (!reason) return 0;
    if (strcmp(reason, "page_load") == 0) return 1;
    if (strcmp(reason, "page_leave") == 0) return 2;
    if (strcmp(reason, "page_save") == 0) return 3;
    if (strcmp(reason, "save") == 0) return 4;
    return 0;
}

static bool writeFixedRecordUnlocked(const String& path,
                                     uint32_t kind,
                                     uint32_t index,
                                     uint32_t documentLength,
                                     uint32_t flags,
                                     const String& content,
                                     String& error) {
    if (static_cast<size_t>(content.length()) > kEditorRecordPayloadBytes) {
        error = String("Payload exceeds fixed record size: ") + path;
        return false;
    }
    if (!ensureDirectoryChainUnlocked(parentPathOf(path), error)) {
        return false;
    }

    uint8_t record[kEditorRecordSize];
    memset(record, 0, sizeof(record));

    EditorRecordHeader header;
    header.magic = kEditorRecordMagic;
    header.version = kEditorRecordVersion;
    header.kind = kind;
    header.index = index;
    header.payloadLength = static_cast<uint32_t>(content.length());
    header.documentLength = documentLength;
    header.flags = flags;
    header.checksum = fnv1aBytes(reinterpret_cast<const uint8_t*>(content.c_str()),
                                 static_cast<size_t>(content.length()));

    memcpy(record, &header, sizeof(header));
    if (content.length() > 0) {
        memcpy(record + sizeof(header), content.c_str(), static_cast<size_t>(content.length()));
    }

    FsFile file;
    if (!file.open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC)) {
        error = String("Failed to open fixed record for write: ") + path;
        return false;
    }
    if (file.isDir()) {
        error = String("Path is a directory: ") + path;
        return false;
    }

    size_t bytesWritten = file.write(record, sizeof(record));
    if (bytesWritten != sizeof(record)) {
        error = String("Failed to write fixed record: ") + path;
        return false;
    }
    if (!file.sync()) {
        error = String("Failed to sync fixed record: ") + path;
        return false;
    }

    error = "";
    return true;
}

static bool readFixedRecordUnlocked(const String& path,
                                    uint32_t expectedKind,
                                    uint32_t expectedIndex,
                                    String& content,
                                    uint32_t& documentLength,
                                    uint32_t& flags,
                                    String& error) {
    content = "";
    documentLength = 0;
    flags = 0;

    FsFile file;
    if (!file.open(path.c_str(), O_RDONLY)) {
        error = String("Failed to open fixed record: ") + path;
        return false;
    }
    if (file.isDir()) {
        error = String("Path is a directory: ") + path;
        return false;
    }
    if (static_cast<size_t>(file.size()) != kEditorRecordSize) {
        error = String("Unexpected fixed record size: ") + path;
        return false;
    }

    uint8_t record[kEditorRecordSize];
    int bytesRead = file.read(record, sizeof(record));
    if (bytesRead != static_cast<int>(sizeof(record))) {
        error = String("Failed to read fixed record: ") + path;
        return false;
    }

    EditorRecordHeader header;
    memcpy(&header, record, sizeof(header));
    if (header.magic != kEditorRecordMagic || header.version != kEditorRecordVersion) {
        error = String("Fixed record header mismatch: ") + path;
        return false;
    }
    if (header.kind != expectedKind || header.index != expectedIndex) {
        error = String("Fixed record kind/index mismatch: ") + path;
        return false;
    }
    if (header.payloadLength > kEditorRecordPayloadBytes) {
        error = String("Fixed record payload is out of range: ") + path;
        return false;
    }

    uint32_t checksum = fnv1aBytes(record + sizeof(header), header.payloadLength);
    if (checksum != header.checksum) {
        error = String("Fixed record checksum mismatch: ") + path;
        return false;
    }

    char text[kEditorRecordPayloadBytes + 1];
    memcpy(text, record + sizeof(header), header.payloadLength);
    text[header.payloadLength] = '\0';
    content = String(text);
    documentLength = header.documentLength;
    flags = header.flags;
    error = "";
    return true;
}

static bool appendFileBytes(FsFile& src, FsFile& dst, size_t length, String& error) {
    char buffer[256];
    size_t remaining = length;
    while (remaining > 0) {
        size_t chunk = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        int bytesRead = src.read(buffer, chunk);
        if (bytesRead < 0) {
            error = "Failed to read source file";
            return false;
        }
        if (bytesRead == 0) break;
        size_t bytesWritten = dst.write(reinterpret_cast<const uint8_t*>(buffer), static_cast<size_t>(bytesRead));
        if (bytesWritten != static_cast<size_t>(bytesRead)) {
            error = "Failed to write destination file";
            return false;
        }
        remaining -= static_cast<size_t>(bytesRead);
    }
    error = "";
    return true;
}

static bool appendFileRemainder(FsFile& src, FsFile& dst, String& error) {
    char buffer[256];
    while (true) {
        int bytesRead = src.read(buffer, sizeof(buffer));
        if (bytesRead < 0) {
            error = "Failed to read source file";
            return false;
        }
        if (bytesRead == 0) break;
        size_t bytesWritten = dst.write(reinterpret_cast<const uint8_t*>(buffer), static_cast<size_t>(bytesRead));
        if (bytesWritten != static_cast<size_t>(bytesRead)) {
            error = "Failed to write destination file";
            return false;
        }
    }
    error = "";
    return true;
}

static const char* multiTapMap[] = {
    " 0",
    ",.=:()[]{}+-*/_?!1<>;\"'`%#\\|&$",
    "abc2",
    "def3",
    "ghi4",
    "jkl5",
    "mno6",
    "pqrs7",
    "tuv8",
    "wxyz9"
};

static char getMatchingBracket(char left) {
    switch (left) {
        case '(': return ')';
        case '[': return ']';
        case '{': return '}';
        case '<': return '>';
        case '"': return '"';
        case '\'': return '\'';
        default:  return '\0';
    }
}

static void insertCharWithAutoBracket(String& text, int& cursor, char c) {
    char right = getMatchingBracket(c);
    if (right != '\0') {
        String pair = String(c) + String(right);
        text = text.substring(0, cursor) + pair + text.substring(cursor);
        cursor += 1;
        return;
    }
    text = text.substring(0, cursor) + String(c) + text.substring(cursor);
    cursor += 1;
}

static void drawCenteredStatusDialog(const char* message) {
    u8g2.clearBuffer();
    GUI::drawPopupFrame(18, 22, 92, 20, true);
    u8g2.setFont(u8g2_font_5x8_tr);
    int msgWidth = u8g2.getStrWidth(message);
    u8g2.drawStr((GUI::SCREEN_WIDTH - msgWidth) / 2, 35, message);
    u8g2.sendBuffer();
}

static void drawHighlightedChoiceBar(int xStart, int baselineY, const char* choices, int selectedIndex) {
    if (!choices) return;

    int count = strlen(choices);
    if (count <= 0) return;
    if (selectedIndex < 0) selectedIndex = 0;
    if (selectedIndex >= count) selectedIndex = count - 1;

    int widths[40];
    int safeCount = min(count, 40);
    for (int i = 0; i < safeCount; i++) {
        char item[2] = {choices[i], '\0'};
        widths[i] = u8g2.getStrWidth(item) + 2;
    }

    int startIndex = 0;
    int widthToSelected = 0;
    for (int i = 0; i < selectedIndex && i < safeCount; i++) widthToSelected += widths[i];
    while (widthToSelected > 56 && startIndex < selectedIndex) {
        widthToSelected -= widths[startIndex];
        startIndex++;
    }

    int x = xStart;
    if (startIndex > 0) {
        u8g2.drawStr(x, baselineY, "<");
        x += u8g2.getStrWidth("<") + 2;
    }

    bool clippedRight = false;
    for (int i = startIndex; i < safeCount; i++) {
        char item[2] = {choices[i], '\0'};
        int charWidth = u8g2.getStrWidth(item);
        int paddedWidth = charWidth + 2;
        if (x + paddedWidth > 122) {
            clippedRight = (i < safeCount);
            break;
        }

        if (i == selectedIndex) {
            u8g2.drawBox(x - 1, baselineY - 7, paddedWidth + 1, 9);
            u8g2.setDrawColor(0);
            u8g2.drawStr(x, baselineY, item);
            u8g2.setDrawColor(1);
        } else {
            u8g2.drawStr(x, baselineY, item);
        }
        x += paddedWidth;
    }

    if (clippedRight) {
        u8g2.drawStr(123, baselineY, ">");
    }
}

static bool isWrapBoundaryChar(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (uc <= ' ') return true;
    if ((uc >= '0' && uc <= '9') ||
        (uc >= 'A' && uc <= 'Z') ||
        (uc >= 'a' && uc <= 'z') ||
        uc == '_') {
        return false;
    }
    if (uc >= 0x80) return false;
    return true;
}

static int getWrappedFitLength(const String& segment, int segStart, int segmentLen, int textAreaWidth) {
    int fitLen = 0;
    int lastBoundaryLen = -1;

    while (segStart + fitLen < segmentLen) {
        String sub = segment.substring(segStart, segStart + fitLen + 1);
        if (u8g2.getUTF8Width(sub.c_str()) > textAreaWidth) break;
        fitLen++;

        if (isWrapBoundaryChar(segment[segStart + fitLen - 1])) {
            lastBoundaryLen = fitLen;
        }
    }

    if (fitLen == 0 && segStart < segmentLen) return 1;

    if (segStart + fitLen < segmentLen && lastBoundaryLen > 0) {
        return lastBoundaryLen;
    }

    return fitLen;
}

T9EditorApp::T9EditorApp() {
    scrollOffset = 0;
    openMode = OPEN_READ_WRITE;
    sourceKind = SOURCE_BUFFER;
    sourcePageSize = static_cast<int>(getT9EditorReadOnlyPageBytes());
    sessionExportDirty = false;
    sessionSourceSize = 0;
    documentLabel = "T9 EDITOR";
    documentPath = "";
    sourceBuffer = "";
    documentBuffer = "";
    resetPagedSession();
    exitRequested = false;
    resetEditorSession();
}

void T9EditorApp::start() {
    u8g2.setContrast(systemContrast);
    exitRequested = false;
    openMode = (appTransferEditorMode == APP_TRANSFER_EDITOR_READ_ONLY || appTransferAction == ACTION_VIEW_FILE)
               ? OPEN_READ_ONLY
               : OPEN_READ_WRITE;
    sourceKind = (appTransferSourceKind == APP_TRANSFER_SOURCE_PAGED_FILE)
                 ? SOURCE_PAGED_FILE
                 : SOURCE_BUFFER;
    documentPath = appTransferPath;
    sourceBuffer = appTransferString;
    documentBuffer = appTransferString;
    documentLabel = appTransferLabel;
    resetPagedSession();
    if (documentLabel.length() == 0) {
        if (appTransferAction == ACTION_CREATE_FILE) {
            documentLabel = "New File";
        } else if (appTransferAction == ACTION_REQUEST_FILENAME ||
                   appTransferAction == ACTION_CREATE_FOLDER ||
                   appTransferAction == ACTION_RENAME) {
            documentLabel = "Enter name";
        } else if (documentPath.length() > 0) {
            documentLabel = documentPath;
        } else if (isReadOnly()) {
            documentLabel = "VIEWER";
        } else {
            documentLabel = "T9 EDITOR";
        }
    }
    if (sourceKind == SOURCE_PAGED_FILE) {
        String error;
        if (!isReadOnly()) {
            size_t fileSize = 0;
            if (!statPagedDocument(fileSize, error)) {
                Serial.printf("[T9Editor] Paged open failed: %s\n", error.c_str());
                sourceKind = SOURCE_BUFFER;
                openMode = OPEN_READ_ONLY;
                documentBuffer = error;
                documentPath = "";
            } else if (fileSize > kT9EditorReadWriteMaxBytes) {
                Serial.printf("[T9Editor] RW open blocked by size cap: %u > %u\n",
                              static_cast<unsigned>(fileSize),
                              static_cast<unsigned>(kT9EditorReadWriteMaxBytes));
                sourceKind = SOURCE_BUFFER;
                openMode = OPEN_READ_ONLY;
                documentLabel = "RW blocked";
                documentBuffer = "RW cap exceeded.\nUse browser Enter for RO.";
                documentPath = "";
            }
        }
    }
    if (sourceKind == SOURCE_PAGED_FILE) {
        String error;
        if (!isReadOnly()) {
            drawCenteredStatusDialog("opening file");
        }
        sourcePageSize = isReadOnly()
                         ? static_cast<int>(getT9EditorReadOnlyPageBytes())
                         : static_cast<int>(kT9EditorReadWriteMaxBytes);
        if (!loadPagedDocument(error)) {
            Serial.printf("[T9Editor] Paged open failed: %s\n", error.c_str());
            sourceKind = SOURCE_BUFFER;
            openMode = OPEN_READ_ONLY;
            documentBuffer = error;
            documentPath = "";
        }
    }
    if (isReadOnly() && sourceKind == SOURCE_BUFFER) {
        String error;
        sourcePageSize = static_cast<int>(getT9EditorReadOnlyPageBytes());
        if (!loadPagedDocument(error)) {
            Serial.printf("[T9Editor] Buffer paging setup failed: %s\n", error.c_str());
            sourceBuffer = error;
            documentBuffer = error;
        }
    }
    Serial.printf("[T9Editor] Open mode: %s path=%s action=%d\n",
                  isReadOnly() ? "RO" : "RW",
                  documentPath.length() > 0 ? documentPath.c_str() : "(buffer)",
                  appTransferAction);
    resetEditorSession();
    recalculateLayout();
}

void T9EditorApp::stop() {
    visualLines.clear();
    sourceBuffer = "";
    documentBuffer = "";
    documentLabel = "T9 EDITOR";
    documentPath = "";
    resetPagedSession();
    exitRequested = false;
}

void T9EditorApp::resetEditorSession() {
    t9predict.reset();
    cursorPos = 0;
    shiftMode = 0;
    inputMode = MODE_T9;
    tapKey = '\0';
    tapIndex = 0;
    tapTime = 0;
    cursorMoveTime = 0;
    viewMode = VIEW_FULL;
    savePromptActive = false;
    savePromptSelection = 0;
    singleKeyCycleIndex = 0;
    zeroPending = false;
    fallback = false;
    fallbackStart = 0;
    scrollOffset = 0;
    selectionMode = false;
    shiftTapPending = false;
}

void T9EditorApp::resetPagedSession() {
    sourcePageSize = static_cast<int>(getT9EditorReadOnlyPageBytes());
    sessionExportDirty = false;
    sessionSourceSize = 0;
    pageStartOffset = 0;
    pageOriginalLength = 0;
    pagedDocumentSize = 0;
    currentPageIndex = 0;
    totalPageCount = 0;
    pageDirty = false;
    historyDocumentId = "";
    historyNextSnapshotId = 1;
    nextClipboardSlot = 1;
    pagePromptActive = false;
    pagePromptSelection = 0;
    pendingPageAction = PAGED_ACTION_NONE;
}

String T9EditorApp::readDocumentSlice(int start, int length) const {
    if (length <= 0 || start < 0) return "";
    int docLen = getDocumentLength();
    if (start >= docLen) return "";
    int end = start + length;
    if (end > docLen) end = docLen;
    return documentBuffer.substring(start, end);
}

int T9EditorApp::getDocumentLength() const {
    return documentBuffer.length();
}

bool T9EditorApp::isKeyActiveNow(char key) const {
    for (int i = 0; i < activeKeyCount; i++) {
        if (activeKeys[i] == key) {
            return true;
        }
    }
    return false;
}

void T9EditorApp::showReadWriteCapToast() const {
    GUI::showToast("RW cap exceeded", 1500);
}

bool T9EditorApp::tryInsertTextAtCursor(const String& text, int cursorAdvance) {
    if (sourceKind == SOURCE_PAGED_FILE && !isReadOnly() &&
        static_cast<size_t>(documentBuffer.length() + text.length()) > kT9EditorReadWriteMaxBytes) {
        showReadWriteCapToast();
        return false;
    }

    documentBuffer = documentBuffer.substring(0, cursorPos) + text + documentBuffer.substring(cursorPos);
    cursorPos += cursorAdvance;
    return true;
}

bool T9EditorApp::tryInsertCharWithAutoBracket(char c) {
    char right = getMatchingBracket(c);
    size_t addedBytes = right != '\0' ? 2U : 1U;
    if (sourceKind == SOURCE_PAGED_FILE && !isReadOnly() &&
        static_cast<size_t>(documentBuffer.length()) + addedBytes > kT9EditorReadWriteMaxBytes) {
        showReadWriteCapToast();
        return false;
    }

    insertCharWithAutoBracket(documentBuffer, cursorPos, c);
    return true;
}

bool T9EditorApp::statPagedDocument(size_t& fileSize, String& error) const {
    fileSize = 0;
    if (documentPath.length() == 0) {
        error = "Missing paged document path";
        return false;
    }

    EditorSdSessionGuard session;
    if (!session.begin()) {
        error = "Failed to open SD session";
        return false;
    }

    FsFile file;
    if (!file.open(documentPath.c_str(), O_RDONLY)) {
        error = String("Failed to open file: ") + documentPath;
        return false;
    }
    if (file.isDir()) {
        error = String("Path is a directory: ") + documentPath;
        return false;
    }

    fileSize = static_cast<size_t>(file.size());
    error = "";
    return true;
}

bool T9EditorApp::readFileRange(size_t start, size_t length, String& out, String& error) const {
    out = "";
    if (documentPath.length() == 0) {
        error = "Missing paged document path";
        return false;
    }

    EditorSdSessionGuard session;
    if (!session.begin()) {
        error = "Failed to open SD session";
        return false;
    }

    FsFile file;
    if (!file.open(documentPath.c_str(), O_RDONLY)) {
        error = String("Failed to open file: ") + documentPath;
        return false;
    }
    if (file.isDir()) {
        error = String("Path is a directory: ") + documentPath;
        return false;
    }

    size_t fileSize = static_cast<size_t>(file.size());
    if (start > fileSize) start = fileSize;
    size_t available = fileSize - start;
    if (length > available) length = available;

    if (!file.seekSet(start)) {
        error = String("Failed to seek file: ") + documentPath;
        return false;
    }

    if (length > 0 && !out.reserve(static_cast<unsigned int>(length))) {
        error = String("Not enough memory to load page: ") + documentPath;
        return false;
    }

    char buffer[129];
    size_t remaining = length;
    while (remaining > 0) {
        size_t chunk = remaining < (sizeof(buffer) - 1) ? remaining : (sizeof(buffer) - 1);
        int bytesRead = file.read(buffer, chunk);
        if (bytesRead < 0) {
            error = String("Failed to read file: ") + documentPath;
            return false;
        }
        if (bytesRead == 0) break;
        buffer[bytesRead] = '\0';
        if (!out.concat(buffer)) {
            error = String("Failed to append page data: ") + documentPath;
            return false;
        }
        remaining -= static_cast<size_t>(bytesRead);
    }

    error = "";
    return true;
}

void T9EditorApp::updatePagedDocumentMetrics(size_t fileSize) {
    pagedDocumentSize = fileSize;
    if (isReadOnly()) {
        sourcePageSize = static_cast<int>(getT9EditorReadOnlyPageBytes());
        totalPageCount = max(1, static_cast<int>((fileSize + static_cast<size_t>(sourcePageSize) - 1) /
                                                 static_cast<size_t>(sourcePageSize)));
        if (currentPageIndex >= totalPageCount) currentPageIndex = totalPageCount - 1;
        return;
    }

    sourcePageSize = static_cast<int>(kT9EditorReadWriteMaxBytes);
    totalPageCount = 1;
    currentPageIndex = 0;
}

void T9EditorApp::markPageDirty() {
    if (sourceKind == SOURCE_PAGED_FILE && !isReadOnly()) {
        pageDirty = true;
    }
}

String T9EditorApp::getEditorSystemRoot() const {
    return String(kEditorSystemRoot);
}

String T9EditorApp::getWorkRoot() const {
    return String(kEditorWorkRoot);
}

String T9EditorApp::getHistoryRoot() const {
    return String(kEditorHistoryRoot);
}

String T9EditorApp::getClipboardRoot() const {
    return String(kEditorClipboardRoot);
}

String T9EditorApp::getSessionRoot() const {
    String docId = historyDocumentId.length() > 0 ? historyDocumentId : buildDefaultHistoryDocumentId();
    return getWorkRoot() + "/" + docId;
}

String T9EditorApp::getSessionManifestPath() const {
    return getSessionRoot() + "/manifest.txt";
}

String T9EditorApp::getSessionChunkRoot() const {
    return getSessionRoot() + "/chunks";
}

String T9EditorApp::getSessionChunkPath(int pageIndex) const {
    char name[20];
    snprintf(name, sizeof(name), "%04d.bin", pageIndex + 1);
    return getSessionChunkRoot() + "/" + String(name);
}

String T9EditorApp::getDocumentHistoryRoot() const {
    String docId = historyDocumentId.length() > 0 ? historyDocumentId : buildDefaultHistoryDocumentId();
    return getHistoryRoot() + "/" + docId;
}

String T9EditorApp::getDocumentManifestPath() const {
    return getDocumentHistoryRoot() + "/manifest.txt";
}

String T9EditorApp::getClipboardManifestPath() const {
    return getClipboardRoot() + "/manifest.txt";
}

String T9EditorApp::getClipboardSlotPath(int slot) const {
    char name[16];
    snprintf(name, sizeof(name), "%02d.bin", slot);
    return getClipboardRoot() + "/" + String(name);
}

String T9EditorApp::buildDefaultHistoryDocumentId() const {
    String base = sanitizePathComponent(getPathTail(documentPath));
    String hash = String(fnv1a32(documentPath), HEX);
    hash.toUpperCase();
    return base + "_" + hash;
}

bool T9EditorApp::loadSessionManifest(String& error) {
    size_t sourceFileSize = 0;
    if (!statPagedDocument(sourceFileSize, error)) {
        return false;
    }

    sourcePageSize = static_cast<int>(kEditorRecordPayloadBytes);
    EditorSdSessionGuard session;
    if (!session.begin()) {
        error = "Failed to open SD session";
        return false;
    }

    if (!sdFat.exists(getSessionManifestPath().c_str())) {
        sessionSourceSize = sourceFileSize;
        sessionExportDirty = false;
        updatePagedDocumentMetrics(sourceFileSize);
        return storeSessionManifest(error);
    }

    String manifest;
    if (!readSmallFileUnlocked(getSessionManifestPath(), manifest, error)) {
        return false;
    }

    const String logicalValue = manifestValue(manifest, "logical_size");
    const String sourceValue = manifestValue(manifest, "source_size");
    const String chunkValue = manifestValue(manifest, "chunk_count");
    pagedDocumentSize = logicalValue.length() > 0
                        ? static_cast<size_t>(strtoul(logicalValue.c_str(), nullptr, 10))
                        : sourceFileSize;
    sessionSourceSize = sourceValue.length() > 0
                        ? static_cast<size_t>(strtoul(sourceValue.c_str(), nullptr, 10))
                        : sourceFileSize;
    totalPageCount = chunkValue.length() > 0 ? chunkValue.toInt() : 0;
    if (totalPageCount < 1) {
        updatePagedDocumentMetrics(sessionSourceSize);
    } else if (currentPageIndex >= totalPageCount) {
        currentPageIndex = totalPageCount - 1;
    }
    if (pagedDocumentSize == 0 && sessionSourceSize > 0) {
        pagedDocumentSize = sessionSourceSize;
    }
    sessionExportDirty = manifestValue(manifest, "dirty_export") == "1";
    error = "";
    return true;
}

bool T9EditorApp::storeSessionManifest(String& error) const {
    String manifest = "path=" + documentPath + "\n";
    manifest += "label=" + documentLabel + "\n";
    manifest += "logical_size=" + String(static_cast<unsigned long>(pagedDocumentSize)) + "\n";
    manifest += "source_size=" + String(static_cast<unsigned long>(sessionSourceSize)) + "\n";
    manifest += "chunk_count=" + String(max(1, totalPageCount)) + "\n";
    manifest += "payload_bytes=" + String(sourcePageSize) + "\n";
    manifest += "record_bytes=" + String(static_cast<unsigned long>(kEditorRecordSize)) + "\n";
    manifest += "dirty_export=" + String(sessionExportDirty ? 1 : 0) + "\n";
    return writeSmallFileUnlocked(getSessionManifestPath(), manifest, error);
}

bool T9EditorApp::ensureSessionWorkspace(String& error) {
    if (sourceKind != SOURCE_PAGED_FILE) {
        error = "";
        return true;
    }
    if (!ensureEditorStorage(error)) {
        return false;
    }

    EditorSdSessionGuard session;
    if (!session.begin()) {
        error = "Failed to open SD session";
        return false;
    }

    if (!ensureDirectoryChainUnlocked(getSessionRoot(), error)) return false;
    if (!ensureDirectoryChainUnlocked(getSessionChunkRoot(), error)) return false;
    return loadSessionManifest(error);
}

bool T9EditorApp::writeSessionChunk(int pageIndex, const String& content, size_t documentLength, String& error) {
    if (static_cast<size_t>(content.length()) > kEditorRecordPayloadBytes) {
        error = "Page content exceeds fixed chunk payload size";
        return false;
    }
    if (!ensureSessionWorkspace(error)) {
        return false;
    }

    const uint32_t flags = sessionExportDirty ? kEditorRecordFlagDirtyExport : 0;
    return writeFixedRecordUnlocked(getSessionChunkPath(pageIndex),
                                    kEditorRecordKindSessionChunk,
                                    static_cast<uint32_t>(pageIndex),
                                    static_cast<uint32_t>(documentLength),
                                    flags,
                                    content,
                                    error);
}

bool T9EditorApp::loadSessionChunk(int pageIndex, String& error) {
    if (!ensureSessionWorkspace(error)) {
        return false;
    }

    EditorSdSessionGuard session;
    if (!session.begin()) {
        error = "Failed to open SD session";
        return false;
    }

    String chunkPath = getSessionChunkPath(pageIndex);
    if (!sdFat.exists(chunkPath.c_str())) {
        const size_t start = static_cast<size_t>(pageIndex) * static_cast<size_t>(sourcePageSize);
        size_t length = 0;
        if (sessionSourceSize > start) {
            const size_t remaining = sessionSourceSize - start;
            length = remaining < static_cast<size_t>(sourcePageSize) ? remaining : static_cast<size_t>(sourcePageSize);
        }

        String chunkText;
        if (!readFileRange(start, length, chunkText, error)) {
            return false;
        }
        if (!writeSessionChunk(pageIndex, chunkText, pagedDocumentSize, error)) {
            return false;
        }
    }

    String chunkText;
    uint32_t recordDocumentLength = 0;
    uint32_t flags = 0;
    if (!readFixedRecordUnlocked(chunkPath,
                                 kEditorRecordKindSessionChunk,
                                 static_cast<uint32_t>(pageIndex),
                                 chunkText,
                                 recordDocumentLength,
                                 flags,
                                 error)) {
        return false;
    }

    documentBuffer = chunkText;
    pageStartOffset = static_cast<size_t>(pageIndex) * static_cast<size_t>(sourcePageSize);
    pageOriginalLength = static_cast<size_t>(chunkText.length());
    currentPageIndex = pageIndex;
    pageDirty = false;
    if (recordDocumentLength > 0) {
        pagedDocumentSize = static_cast<size_t>(recordDocumentLength);
    }
    sessionExportDirty = ((flags & kEditorRecordFlagDirtyExport) != 0) || sessionExportDirty;
    error = "";
    return true;
}

bool T9EditorApp::exportPagedDocument(String& error) {
    if (sourceKind != SOURCE_PAGED_FILE) {
        error = "";
        return true;
    }
    if (isReadOnly()) {
        error = "Document is read-only";
        return false;
    }
    if (!ensureSessionWorkspace(error)) {
        return false;
    }

    EditorSdSessionGuard session;
    if (!session.begin()) {
        error = "Failed to open SD session";
        return false;
    }
    if (!ensureDirectoryChainUnlocked(getEditorSystemRoot(), error)) {
        return false;
    }

    sdFat.remove(kEditorRewriteTempPath);
    sdFat.remove(kEditorRewriteBackupPath);

    FsFile dst;
    if (!dst.open(kEditorRewriteTempPath, O_WRONLY | O_CREAT | O_TRUNC)) {
        error = String("Failed to open temp file for write: ") + kEditorRewriteTempPath;
        return false;
    }

    size_t exportedLength = 0;
    for (int pageIndex = 0; pageIndex < max(1, totalPageCount); pageIndex++) {
        String chunkText;
        String chunkPath = getSessionChunkPath(pageIndex);
        if (sdFat.exists(chunkPath.c_str())) {
            uint32_t recordDocumentLength = 0;
            uint32_t flags = 0;
            if (!readFixedRecordUnlocked(chunkPath,
                                         kEditorRecordKindSessionChunk,
                                         static_cast<uint32_t>(pageIndex),
                                         chunkText,
                                         recordDocumentLength,
                                         flags,
                                         error)) {
                return false;
            }
        } else {
            const size_t start = static_cast<size_t>(pageIndex) * static_cast<size_t>(sourcePageSize);
            size_t length = 0;
            if (sessionSourceSize > start) {
                const size_t remaining = sessionSourceSize - start;
                length = remaining < static_cast<size_t>(sourcePageSize) ? remaining : static_cast<size_t>(sourcePageSize);
            }
            if (!readFileRange(start, length, chunkText, error)) {
                return false;
            }
        }

        if (chunkText.length() > 0) {
            size_t bytesWritten = dst.write(reinterpret_cast<const uint8_t*>(chunkText.c_str()),
                                           static_cast<size_t>(chunkText.length()));
            if (bytesWritten != static_cast<size_t>(chunkText.length())) {
                error = "Failed to write exported document";
                return false;
            }
            exportedLength += static_cast<size_t>(chunkText.length());
        }
    }

    if (!dst.sync()) {
        error = String("Failed to sync temp file: ") + kEditorRewriteTempPath;
        return false;
    }
    dst.close();

    if (sdFat.exists(documentPath.c_str()) && !sdFat.rename(documentPath.c_str(), kEditorRewriteBackupPath)) {
        error = String("Failed to stage backup for: ") + documentPath;
        return false;
    }
    if (!sdFat.rename(kEditorRewriteTempPath, documentPath.c_str())) {
        sdFat.rename(kEditorRewriteBackupPath, documentPath.c_str());
        error = String("Failed to replace file: ") + documentPath;
        return false;
    }
    if (sdFat.exists(kEditorRewriteBackupPath)) {
        sdFat.remove(kEditorRewriteBackupPath);
    }

    pagedDocumentSize = exportedLength;
    sessionSourceSize = exportedLength;
    sessionExportDirty = false;
    if (currentPageIndex >= totalPageCount) {
        currentPageIndex = max(0, totalPageCount - 1);
    }
    if (!storeSessionManifest(error)) {
        return false;
    }

    error = "";
    return true;
}

bool T9EditorApp::ensureEditorStorage(String& error) {
    EditorSdSessionGuard session;
    if (!session.begin()) {
        error = "Failed to open SD session";
        return false;
    }

    if (!ensureDirectoryChainUnlocked(getEditorSystemRoot(), error)) return false;
    if (!ensureDirectoryChainUnlocked(getWorkRoot(), error)) return false;
    if (!ensureDirectoryChainUnlocked(getHistoryRoot(), error)) return false;
    if (!ensureDirectoryChainUnlocked(getClipboardRoot(), error)) return false;
    error = "";
    return true;
}

bool T9EditorApp::ensureHistoryDocument(String& error) {
    if (sourceKind != SOURCE_PAGED_FILE || isReadOnly()) {
        error = "";
        return true;
    }
    if (!ensureEditorStorage(error)) {
        return false;
    }

    EditorSdSessionGuard session;
    if (!session.begin()) {
        error = "Failed to open SD session";
        return false;
    }

    if (historyDocumentId.length() == 0) {
        FsFile root;
        FsFile entry;
        if (root.open(getHistoryRoot().c_str(), O_RDONLY) && root.isDir()) {
            char entryName[64];
            while (entry.openNext(&root, O_RDONLY)) {
                size_t nameLen = entry.getName(entryName, sizeof(entryName));
                bool isDir = entry.isDir();
                entry.close();
                if (!isDir || nameLen == 0) continue;

                String manifestPath = getHistoryRoot() + "/" + String(entryName) + "/manifest.txt";
                String manifest;
                String readError;
                if (!readSmallFileUnlocked(manifestPath, manifest, readError)) {
                    continue;
                }
                if (manifestValue(manifest, "path") == documentPath || manifestContainsAlias(manifest, documentPath)) {
                    historyDocumentId = entryName;
                    String nextValue = manifestValue(manifest, "next_snapshot");
                    historyNextSnapshotId = nextValue.length() > 0 ? static_cast<unsigned long>(nextValue.toInt()) : 1UL;
                    break;
                }
            }
            root.close();
        }

        if (historyDocumentId.length() == 0) {
            historyDocumentId = buildDefaultHistoryDocumentId();
            historyNextSnapshotId = 1;
        }
    }

    if (!ensureDirectoryChainUnlocked(getDocumentHistoryRoot(), error)) {
        return false;
    }

    String manifest = "path=" + documentPath + "\n";
    manifest += "label=" + documentLabel + "\n";
    manifest += "alias=" + documentPath + "\n";
    manifest += "next_snapshot=" + String(historyNextSnapshotId) + "\n";
    manifest += "size=" + String(static_cast<unsigned long>(pagedDocumentSize)) + "\n";
    manifest += "mode=rw_whole\n";
    if (!writeSmallFileUnlocked(getDocumentManifestPath(), manifest, error)) {
        return false;
    }

    error = "";
    return true;
}

bool T9EditorApp::recordPageSnapshot(const char* reason, String& error) {
    if (sourceKind != SOURCE_PAGED_FILE || isReadOnly()) {
        error = "";
        return true;
    }
    if (!ensureHistoryDocument(error)) {
        return false;
    }

    EditorSdSessionGuard session;
    if (!session.begin()) {
        error = "Failed to open SD session";
        return false;
    }

    char name[40];
    snprintf(name, sizeof(name), "%06lu.txt", historyNextSnapshotId);
    String snapshotPath = getDocumentHistoryRoot() + "/" + String(name);
    if (!writeSmallFileUnlocked(snapshotPath, documentBuffer, error)) {
        return false;
    }

    historyNextSnapshotId++;
    String manifest = "path=" + documentPath + "\n";
    manifest += "label=" + documentLabel + "\n";
    manifest += "alias=" + documentPath + "\n";
    manifest += "next_snapshot=" + String(historyNextSnapshotId) + "\n";
    manifest += "size=" + String(static_cast<unsigned long>(documentBuffer.length())) + "\n";
    manifest += "mode=rw_whole\n";
    if (!writeSmallFileUnlocked(getDocumentManifestPath(), manifest, error)) {
        return false;
    }

    error = "";
    return true;
}

bool T9EditorApp::loadClipboardState(String& error) {
    if (sourceKind != SOURCE_PAGED_FILE) {
        error = "";
        return true;
    }
    if (!ensureEditorStorage(error)) {
        return false;
    }

    nextClipboardSlot = 1;
    EditorSdSessionGuard session;
    if (!session.begin()) {
        error = "Failed to open SD session";
        return false;
    }

    if (sdFat.exists(getClipboardManifestPath().c_str())) {
        String manifest;
        if (!readSmallFileUnlocked(getClipboardManifestPath(), manifest, error)) {
            return false;
        }
        String nextValue = manifestValue(manifest, "next_slot");
        if (nextValue.length() > 0) {
            nextClipboardSlot = nextValue.toInt();
        }
        if (nextClipboardSlot < 1 || nextClipboardSlot > kClipboardSlotCount) {
            nextClipboardSlot = 1;
        }
    }

    for (int slot = 1; slot <= kClipboardSlotCount; slot++) {
        String slotPath = getClipboardSlotPath(slot);
        if (!sdFat.exists(slotPath.c_str()) &&
            !writeFixedRecordUnlocked(slotPath,
                                      kEditorRecordKindClipboardSlot,
                                      static_cast<uint32_t>(slot),
                                      0,
                                      0,
                                      "",
                                      error)) {
            return false;
        }
    }

    return storeClipboardState(error);
}

bool T9EditorApp::storeClipboardState(String& error) {
    if (sourceKind != SOURCE_PAGED_FILE) {
        error = "";
        return true;
    }

    EditorSdSessionGuard session;
    if (!session.begin()) {
        error = "Failed to open SD session";
        return false;
    }

    String manifest = "next_slot=" + String(nextClipboardSlot) + "\n";
    manifest += "slot_count=" + String(kClipboardSlotCount) + "\n";
    return writeSmallFileUnlocked(getClipboardManifestPath(), manifest, error);
}

bool T9EditorApp::writeClipboardSlot(const String& content, int& writtenSlot, String& error) {
    writtenSlot = 0;
    if (!loadClipboardState(error)) {
        return false;
    }
    if (static_cast<size_t>(content.length()) > kEditorRecordPayloadBytes) {
        error = "Clipboard content exceeds page size";
        return false;
    }

    EditorSdSessionGuard session;
    if (!session.begin()) {
        error = "Failed to open SD session";
        return false;
    }

    writtenSlot = nextClipboardSlot;
    if (!writeFixedRecordUnlocked(getClipboardSlotPath(writtenSlot),
                                  kEditorRecordKindClipboardSlot,
                                  static_cast<uint32_t>(writtenSlot),
                                  static_cast<uint32_t>(content.length()),
                                  0,
                                  content,
                                  error)) {
        writtenSlot = 0;
        return false;
    }

    nextClipboardSlot++;
    if (nextClipboardSlot > kClipboardSlotCount) {
        nextClipboardSlot = 1;
    }
    return storeClipboardState(error);
}

bool T9EditorApp::readClipboardSlot(int slot, String& content, String& error) const {
    content = "";
    if (sourceKind != SOURCE_PAGED_FILE) {
        error = "Clipboard unavailable for buffer sessions";
        return false;
    }
    if (slot < 1 || slot > kClipboardSlotCount) {
        error = "Clipboard slot is out of range";
        return false;
    }

    EditorSdSessionGuard session;
    if (!session.begin()) {
        error = "Failed to open SD session";
        return false;
    }

    uint32_t documentLength = 0;
    uint32_t flags = 0;
    if (!readFixedRecordUnlocked(getClipboardSlotPath(slot),
                                 kEditorRecordKindClipboardSlot,
                                 static_cast<uint32_t>(slot),
                                 content,
                                 documentLength,
                                 flags,
                                 error)) {
        return false;
    }
    if (static_cast<size_t>(content.length()) > kEditorRecordPayloadBytes) {
        error = "Clipboard slot exceeds page size";
        content = "";
        return false;
    }

    error = "";
    return true;
}

bool T9EditorApp::saveCurrentPage(String& error) {
    if (sourceKind != SOURCE_PAGED_FILE) {
        error = "";
        return true;
    }
    if (isReadOnly()) {
        error = "Document is read-only";
        return false;
    }
    if (static_cast<size_t>(documentBuffer.length()) > kT9EditorReadWriteMaxBytes) {
        error = "Document exceeds RW size cap";
        return false;
    }

    EditorSdSessionGuard session;
    if (!session.begin()) {
        error = "Failed to open SD session";
        return false;
    }

    if (!writeSmallFileUnlocked(documentPath, documentBuffer, error)) {
        return false;
    }

    pagedDocumentSize = static_cast<size_t>(documentBuffer.length());
    sessionSourceSize = pagedDocumentSize;
    updatePagedDocumentMetrics(pagedDocumentSize);
    pageDirty = false;
    pageOriginalLength = pagedDocumentSize;
    pageStartOffset = 0;

    if (!ensureHistoryDocument(error)) {
        return false;
    }

    String snapshotError;
    if (!recordPageSnapshot("save", snapshotError)) {
        Serial.printf("[T9Editor] Snapshot warning after save: %s\n", snapshotError.c_str());
    }

    error = "";
    return true;
}

bool T9EditorApp::loadPagedDocument(String& error) {
    if (isReadOnly()) {
        if (sourceKind == SOURCE_BUFFER) {
            updatePagedDocumentMetrics(static_cast<size_t>(sourceBuffer.length()));
            currentPageIndex = 0;
            return loadPageByIndex(0, error);
        }
        size_t fileSize = 0;
        if (!statPagedDocument(fileSize, error)) return false;
        updatePagedDocumentMetrics(fileSize);
        currentPageIndex = 0;
        return loadPageByIndex(0, error);
    }
    if (!loadClipboardState(error)) return false;
    currentPageIndex = 0;
    if (!loadPageByIndex(0, error)) return false;
    if (!ensureHistoryDocument(error)) return false;
    String snapshotError;
    if (!recordPageSnapshot("load", snapshotError)) {
        Serial.printf("[T9Editor] Snapshot warning on load: %s\n", snapshotError.c_str());
    }
    return true;
}

bool T9EditorApp::loadPageByIndex(int pageIndex, String& error) {
    if (isReadOnly()) {
        if (sourceKind == SOURCE_BUFFER) {
            const size_t bufferSize = static_cast<size_t>(sourceBuffer.length());
            updatePagedDocumentMetrics(bufferSize);
            if (pageIndex < 0 || pageIndex >= totalPageCount) {
                error = "Requested page is out of range";
                return false;
            }

            const size_t start = static_cast<size_t>(pageIndex) * static_cast<size_t>(sourcePageSize);
            size_t length = 0;
            if (bufferSize > start) {
                const size_t remaining = bufferSize - start;
                length = remaining < static_cast<size_t>(sourcePageSize) ? remaining : static_cast<size_t>(sourcePageSize);
            }

            documentBuffer = sourceBuffer.substring(static_cast<unsigned int>(start),
                                                    static_cast<unsigned int>(start + length));
            pageStartOffset = start;
            pageOriginalLength = length;
            currentPageIndex = pageIndex;
            pageDirty = false;
            cursorPos = 0;
            scrollOffset = 0;
            error = "";
            return true;
        }
        size_t fileSize = 0;
        if (!statPagedDocument(fileSize, error)) {
            return false;
        }
        updatePagedDocumentMetrics(fileSize);
        if (pageIndex < 0 || pageIndex >= totalPageCount) {
            error = "Requested page is out of range";
            return false;
        }

        const size_t start = static_cast<size_t>(pageIndex) * static_cast<size_t>(sourcePageSize);
        size_t length = 0;
        if (fileSize > start) {
            const size_t remaining = fileSize - start;
            length = remaining < static_cast<size_t>(sourcePageSize) ? remaining : static_cast<size_t>(sourcePageSize);
        }

        String pageText;
        if (!readFileRange(start, length, pageText, error)) {
            return false;
        }

        documentBuffer = pageText;
        pageStartOffset = start;
        pageOriginalLength = length;
        currentPageIndex = pageIndex;
        pageDirty = false;
        cursorPos = 0;
        scrollOffset = 0;
        error = "";
        return true;
    }
    size_t fileSize = 0;
    if (!statPagedDocument(fileSize, error)) {
        return false;
    }
    if (fileSize > kT9EditorReadWriteMaxBytes) {
        error = "File too large for RW";
        return false;
    }
    updatePagedDocumentMetrics(fileSize);
    if (pageIndex != 0) {
        error = "Requested page is out of range";
        return false;
    }

    String fullText;
    if (!readFileRange(0, fileSize, fullText, error)) {
        return false;
    }

    documentBuffer = fullText;
    pageStartOffset = 0;
    pageOriginalLength = fileSize;
    currentPageIndex = 0;
    pageDirty = false;
    cursorPos = 0;
    scrollOffset = 0;
    error = "";
    return true;
}

bool T9EditorApp::isReadOnlyPaged() const {
    return isReadOnly() && totalPageCount > 0;
}

bool T9EditorApp::hasPreviousPage() const {
    if (isReadOnlyPaged()) return currentPageIndex > 0;
    return sourceKind == SOURCE_PAGED_FILE && currentPageIndex > 0;
}

bool T9EditorApp::hasNextPage() const {
    if (isReadOnlyPaged()) return currentPageIndex + 1 < totalPageCount;
    return sourceKind == SOURCE_PAGED_FILE && currentPageIndex + 1 < totalPageCount;
}

void T9EditorApp::beginPendingPageAction(PendingPagedAction action) {
    if (sourceKind != SOURCE_PAGED_FILE) return;
    pendingPageAction = action;
    pagePromptSelection = 0;
    pagePromptActive = true;
}

void T9EditorApp::handlePagePromptInput(char key) {
    if (key == KEY_ESC) {
        pagePromptActive = false;
        pendingPageAction = PAGED_ACTION_NONE;
        pagePromptSelection = 0;
        return;
    }

    if (key == KEY_LEFT || key == KEY_UP) {
        if (pagePromptSelection > 0) pagePromptSelection--;
        return;
    }
    if (key == KEY_RIGHT || key == KEY_DOWN || key == KEY_TAB) {
        if (pagePromptSelection < 2) pagePromptSelection++;
        return;
    }

    if (key != KEY_ENTER) return;

    pagePromptActive = false;
    if (pagePromptSelection == 2) {
        pendingPageAction = PAGED_ACTION_NONE;
        pagePromptSelection = 0;
        return;
    }

    if (pagePromptSelection == 1) {
        String leaveError;
        if (!recordPageSnapshot("page_leave", leaveError)) {
            Serial.printf("[T9Editor] Snapshot warning on leave: %s\n", leaveError.c_str());
        }
        String error;
        int nextIndex = pendingPageAction == PAGED_ACTION_PREV_PAGE ? currentPageIndex - 1 : currentPageIndex + 1;
        if (!loadPageByIndex(nextIndex, error)) {
            GUI::showToast(error.c_str(), 2000);
        } else {
            cursorPos = pendingPageAction == PAGED_ACTION_PREV_PAGE ? getDocumentLength() : 0;
            recalculateLayout();
        }
        pendingPageAction = PAGED_ACTION_NONE;
        pagePromptSelection = 0;
        return;
    }

    String saveError;
    if (!recordPageSnapshot("page_leave", saveError)) {
        Serial.printf("[T9Editor] Snapshot warning on leave: %s\n", saveError.c_str());
    }
    if (!saveCurrentPage(saveError)) {
        GUI::showToast(saveError.c_str(), 2000);
        pendingPageAction = PAGED_ACTION_NONE;
        pagePromptSelection = 0;
        return;
    }

    int nextIndex = pendingPageAction == PAGED_ACTION_PREV_PAGE ? currentPageIndex - 1 : currentPageIndex + 1;
    if (!loadPageByIndex(nextIndex, saveError)) {
        GUI::showToast(saveError.c_str(), 2000);
    } else {
        cursorPos = pendingPageAction == PAGED_ACTION_PREV_PAGE ? getDocumentLength() : 0;
        recalculateLayout();
    }
    pendingPageAction = PAGED_ACTION_NONE;
    pagePromptSelection = 0;
}

bool T9EditorApp::isReadOnly() const {
    return openMode == OPEN_READ_ONLY;
}

bool T9EditorApp::showHeader() const {
    return viewMode == VIEW_FULL || viewMode == VIEW_FULL_LINENO;
}

bool T9EditorApp::showFooter() const {
    return viewMode == VIEW_FULL || viewMode == VIEW_FULL_LINENO;
}

bool T9EditorApp::showLineNumbers() const {
    return viewMode == VIEW_FULL_LINENO || viewMode == VIEW_MIN_LINENO;
}

int T9EditorApp::getVisibleLineCount() const {
    int textTop = showHeader() ? 13 : 1;
    int textBottom = showFooter() ? 53 : 63;
    const T9EditorFontMetrics& metrics = getCurrentFontMetrics();
    int visible = (textBottom - textTop + 1) / metrics.lineHeight;
    return max(1, visible);
}

int T9EditorApp::countLogicalLines() const {
    int count = 1;
    for (int i = 0; i < getDocumentLength(); i++) {
        if (documentBuffer[i] == '\n') count++;
    }
    return count;
}

int T9EditorApp::getGutterWidth(int logicalLineCount) const {
    if (!showLineNumbers()) return 0;
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", max(1, logicalLineCount));
    u8g2.setFont(getCurrentFontMetrics().font);
    return u8g2.getStrWidth(buf) + 4;
}

int T9EditorApp::getTextLeft(int logicalLineCount) const {
    return showLineNumbers() ? getGutterWidth(logicalLineCount) + 2 : 1;
}

void T9EditorApp::requestExit(bool saveRequested) {
    if (saveRequested && sourceKind == SOURCE_PAGED_FILE && !isReadOnly()) {
        String error;
        if (!saveCurrentPage(error)) {
            GUI::showToast(error.c_str(), 2000);
            savePromptActive = false;
            return;
        }
        Serial.printf("[T9Editor] Saved RW document for %s\n",
                      documentPath.length() > 0 ? documentPath.c_str() : "(buffer)");
        appTransferString = "";
    } else {
        if (saveRequested) {
            Serial.printf("[T9Editor] Save requested; handing off %u bytes for %s\n",
                          static_cast<unsigned>(documentBuffer.length()),
                          documentPath.length() > 0 ? documentPath.c_str() : "(buffer)");
        }
        appTransferString = (sourceKind == SOURCE_PAGED_FILE) ? String("") : documentBuffer;
    }
    appTransferBool = saveRequested;
    appTransferResultReady = true;
    exitRequested = true;
    savePromptActive = false;
}

String T9EditorApp::getMultiTapChar() const {
    if (tapKey < '0' || tapKey > '9') return "";
    const char* map = multiTapMap[tapKey - '0'];
    int len = strlen(map);
    if (tapIndex >= len) return "";
    char c = map[tapIndex];
    if (shiftMode >= 1 && c >= 'a' && c <= 'z') c -= 32;
    return String(c);
}

String T9EditorApp::getPreviewText() const {
    String preview = "";

    if (zeroPending) {
        preview = " ";
    } else if (inputMode == MODE_T9 && tapKey == '1') {
        preview = getMultiTapChar();
    } else if ((inputMode == MODE_ABC || fallback) && tapKey != '\0') {
        preview = getMultiTapChar();
    } else if (inputMode == MODE_T9 && t9predict.hasInput()) {
        int dc = t9predict.getDigitCount();
        if (dc == 1) {
            const char* digs = t9predict.getDigits();
            char digit = (digs && digs[0]) ? digs[0] : '\0';
            int letterCount = t9predict.getSingleKeyLetterCount(digit);
            int dictCount = t9predict.getPrefixCandidateCount();
            int total = letterCount + dictCount;
            if (total > 0) {
                int idx = singleKeyCycleIndex % total;
                if (idx < letterCount) {
                    const char* c = t9predict.getSingleKeyLetter(digit, idx);
                    if (c) preview = c;
                } else {
                    const char* word = t9predict.getPrefixCandidate(idx - letterCount);
                    if (word) {
                        preview = word;
                        if ((int)preview.length() > dc) preview = preview.substring(0, dc);
                    }
                }
            }
        } else {
            const char* word = t9predict.getSelectedPrefixWord();
            if (word) {
                preview = word;
                if ((int)preview.length() > dc) preview = preview.substring(0, dc);
            }
        }
        if (preview.length() == 0) preview = t9predict.getDigits();
    }

    if (shiftMode == 1 && preview.length() > 0) {
        preview[0] = toupper(preview[0]);
    } else if (shiftMode == 2) {
        for (int i = 0; i < (int)preview.length(); i++) {
            if (preview[i] >= 'a' && preview[i] <= 'z') preview[i] -= 32;
        }
    }

    return preview;
}

String T9EditorApp::getDisplayText(String* previewOut) const {
    String preview = getPreviewText();
    if (previewOut) *previewOut = preview;
    if (preview.length() == 0) return documentBuffer;
    return documentBuffer.substring(0, cursorPos) + preview + documentBuffer.substring(cursorPos);
}

void T9EditorApp::commitMultiTap() {
    if (tapKey == '\0') return;
    String c = getMultiTapChar();
    if (c.length() > 0 && tryInsertCharWithAutoBracket(c[0])) {
        markPageDirty();
    }
    tapKey = '\0';
    tapIndex = 0;
    tapTime = 0;
}

void T9EditorApp::commitPrediction() {
    int dc = t9predict.getDigitCount();
    String commitText = "";

    if (dc == 1) {
        const char* digs = t9predict.getDigits();
        char digit = (digs && digs[0]) ? digs[0] : '\0';
        int letterCount = t9predict.getSingleKeyLetterCount(digit);
        int dictCount = t9predict.getPrefixCandidateCount();
        int total = letterCount + dictCount;
        if (total > 0) {
            int idx = singleKeyCycleIndex % total;
            if (idx < letterCount) {
                const char* c = t9predict.getSingleKeyLetter(digit, idx);
                if (c) commitText = c;
            } else {
                const char* word = t9predict.getPrefixCandidate(idx - letterCount);
                if (word) {
                    commitText = word;
                    if ((int)commitText.length() > dc) commitText = commitText.substring(0, dc);
                }
            }
        }
    } else {
        const char* word = t9predict.getSelectedPrefixWord();
        if (word) {
            commitText = word;
            if ((int)commitText.length() > dc) commitText = commitText.substring(0, dc);
        }
    }

    if (commitText.length() == 0) {
        const char* digs = t9predict.getDigits();
        if (digs && dc > 0) {
            String d = digs;
            if (tryInsertTextAtCursor(d, d.length())) {
                markPageDirty();
            }
        }
        t9predict.reset();
        singleKeyCycleIndex = 0;
        return;
    }

    if (shiftMode == 1 && commitText.length() > 0) {
        commitText[0] = toupper(commitText[0]);
    } else if (shiftMode == 2) {
        for (int i = 0; i < (int)commitText.length(); i++) {
            if (commitText[i] >= 'a' && commitText[i] <= 'z') commitText[i] -= 32;
        }
    }

    if (commitText.length() == 1) {
        if (!tryInsertCharWithAutoBracket(commitText[0])) {
            t9predict.reset();
            singleKeyCycleIndex = 0;
            return;
        }
    } else if (!tryInsertTextAtCursor(commitText, commitText.length())) {
        t9predict.reset();
        singleKeyCycleIndex = 0;
        return;
    }
    markPageDirty();
    t9predict.reset();
    singleKeyCycleIndex = 0;
    if (shiftMode == 1) shiftMode = 0;
}

void T9EditorApp::moveCursorVertically(int dir) {
    if (visualLines.empty()) {
        return;
    }

    int currentVisualLine = -1;
    for (int i = 0; i < static_cast<int>(visualLines.size()); i++) {
        const VisualLine& vl = visualLines[i];
        int absStart = vl.byteStartIndex;
        int absEnd = absStart + vl.byteLength;
        bool segmentEnd = (i + 1 >= static_cast<int>(visualLines.size())) ||
                          (visualLines[i + 1].byteStartIndex != absEnd);
        if ((cursorPos >= absStart && cursorPos < absEnd) ||
            (vl.byteLength == 0 && cursorPos == absStart) ||
            (segmentEnd && cursorPos == absEnd)) {
            currentVisualLine = i;
            break;
        }
    }

    if (currentVisualLine < 0) {
        return;
    }

    int targetVisualLine = currentVisualLine + dir;
    if (targetVisualLine < 0 || targetVisualLine >= static_cast<int>(visualLines.size())) {
        return;
    }

    const VisualLine& current = visualLines[currentVisualLine];
    const VisualLine& target = visualLines[targetVisualLine];
    int localOffset = cursorPos - current.byteStartIndex;
    if (localOffset < 0) localOffset = 0;
    if (localOffset > current.byteLength) localOffset = current.byteLength;

    if (target.byteLength <= 0) {
        cursorPos = target.byteStartIndex;
        return;
    }

    cursorPos = target.byteStartIndex + min(localOffset, target.byteLength);
}

void T9EditorApp::handleSavePromptInput(char key) {
    if (key == KEY_ESC || key == KEY_BKSP) {
        savePromptActive = false;
        savePromptSelection = 0;
        return;
    }

    if (key == KEY_LEFT || key == KEY_UP) {
        savePromptSelection = (savePromptSelection + 2) % 3;
        return;
    }

    if (key == KEY_RIGHT || key == KEY_DOWN || key == KEY_TAB) {
        savePromptSelection = (savePromptSelection + 1) % 3;
        return;
    }

    if (key == KEY_ENTER) {
        if (savePromptSelection == 0) {
            requestExit(false);
        } else if (savePromptSelection == 1) {
            requestExit(true);
        } else {
            savePromptActive = false;
        }
    }
}

void T9EditorApp::handleInput(char key) {
    if (pagePromptActive) {
        handlePagePromptInput(key);
        return;
    }

    if (savePromptActive) {
        handleSavePromptInput(key);
        return;
    }

    bool layoutChanged = false;

    if (!isReadOnly() && zeroPending && key != '0') {
        if (tryInsertTextAtCursor(" ", 1)) {
            cursorMoveTime = millis();
            layoutChanged = true;
            markPageDirty();
        }
        zeroPending = false;
    }

    if (!isReadOnly() && !zeroPending && key != '0') {
        cursorMoveTime = millis();
    }

    if (key == KEY_ESC) {
        if (isReadOnly()) {
            requestExit(false);
        } else {
            savePromptActive = true;
            savePromptSelection = 0;
        }
        return;
    }

    if (key == KEY_TAB) {
        if (!isReadOnly() && isKeyActiveNow(KEY_ALT)) {
            if ((inputMode == MODE_ABC || fallback) && tapKey != '\0') commitMultiTap();
            if (inputMode == MODE_T9 && t9predict.hasInput()) commitPrediction();
            fallback = false;
            singleKeyCycleIndex = 0;
            if (inputMode == MODE_T9) inputMode = MODE_ABC;
            else if (inputMode == MODE_ABC) inputMode = MODE_123;
            else inputMode = MODE_T9;
            t9predict.reset();
            tapKey = '\0';
        } else {
            if (viewMode == VIEW_FULL) viewMode = VIEW_FULL_LINENO;
            else if (viewMode == VIEW_FULL_LINENO) viewMode = VIEW_MIN_LINENO;
            else if (viewMode == VIEW_MIN_LINENO) viewMode = VIEW_MIN;
            else viewMode = VIEW_FULL;
        }
        recalculateLayout();
        return;
    }

    if (isReadOnlyPaged()) {
        if (key == KEY_UP) {
            if (scrollOffset > 0) {
                scrollOffset--;
                recalculateLayout();
            }
            return;
        }
        if (key == KEY_DOWN) {
            scrollOffset++;
            recalculateLayout();
            return;
        }
        if (key == KEY_LEFT && hasPreviousPage()) {
            String error;
            if (!loadPageByIndex(currentPageIndex - 1, error)) {
                GUI::showToast(error.c_str(), 2000);
            } else {
                recalculateLayout();
            }
            return;
        }
        if (key == KEY_RIGHT && hasNextPage()) {
            String error;
            if (!loadPageByIndex(currentPageIndex + 1, error)) {
                GUI::showToast(error.c_str(), 2000);
            } else {
                recalculateLayout();
            }
            return;
        }
    }

    if (!isReadOnly() && inputMode == MODE_T9 && tapKey == '1' &&
        (key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT || key == KEY_RIGHT)) {
        const char* map = multiTapMap[1];
        int count = strlen(map);
        if (count > 0) {
            if (key == KEY_UP || key == KEY_LEFT) tapIndex = (tapIndex - 1 + count) % count;
            else tapIndex = (tapIndex + 1) % count;
        }
        tapTime = millis();
        recalculateLayout();
        return;
    }

    if (key == KEY_UP || key == KEY_DOWN) {
        cursorMoveTime = millis();
        if (!isReadOnly() && isKeyActiveNow(KEY_ALT)) {
            if ((inputMode == MODE_ABC || fallback) && tapKey != '\0') commitMultiTap();
            if (inputMode == MODE_T9 && t9predict.hasInput()) commitPrediction();
            fallback = false;
            for (int step = 0; step < 5; step++) {
                moveCursorVertically(key == KEY_UP ? -1 : 1);
            }
            recalculateLayout();
            return;
        }

        if (!isReadOnly() && inputMode == MODE_T9 && t9predict.hasInput() && !fallback) {
            if (t9predict.getDigitCount() == 1) {
                const char* digs = t9predict.getDigits();
                char digit = (digs && digs[0]) ? digs[0] : '\0';
                int letterCount = t9predict.getSingleKeyLetterCount(digit);
                int dictCount = t9predict.getPrefixCandidateCount();
                int total = letterCount + dictCount;
                if (total > 0) {
                    if (key == KEY_UP) singleKeyCycleIndex = (singleKeyCycleIndex - 1 + total) % total;
                    else singleKeyCycleIndex = (singleKeyCycleIndex + 1) % total;
                }
            } else {
                if (key == KEY_UP) t9predict.prevPrefixCandidate();
                else t9predict.nextPrefixCandidate();
            }
            recalculateLayout();
            return;
        }

        if (!isReadOnly()) {
            if ((inputMode == MODE_ABC || fallback) && tapKey != '\0') commitMultiTap();
            if (inputMode == MODE_T9 && t9predict.hasInput()) commitPrediction();
            fallback = false;
        }
        moveCursorVertically(key == KEY_UP ? -1 : 1);
        recalculateLayout();
        return;
    }

    if (key == KEY_LEFT || key == KEY_RIGHT) {
        cursorMoveTime = millis();
        if (!isReadOnly()) {
            if ((inputMode == MODE_ABC || fallback) && tapKey != '\0') commitMultiTap();
            if (inputMode == MODE_T9 && t9predict.hasInput()) commitPrediction();
            fallback = false;

            if (isKeyActiveNow(KEY_ALT)) {
                int delta = key == KEY_LEFT ? -5 : 5;
                cursorPos += delta;
                if (cursorPos < 0) cursorPos = 0;
                if (cursorPos > getDocumentLength()) cursorPos = getDocumentLength();
                recalculateLayout();
                return;
            }
        }
        if (sourceKind == SOURCE_PAGED_FILE) {
            if (key == KEY_LEFT && cursorPos == 0 && hasPreviousPage()) {
                if (!isReadOnly() && pageDirty) {
                    beginPendingPageAction(PAGED_ACTION_PREV_PAGE);
                } else {
                    String error;
                    if (!loadPageByIndex(currentPageIndex - 1, error)) {
                        GUI::showToast(error.c_str(), 2000);
                    } else {
                        cursorPos = getDocumentLength();
                        recalculateLayout();
                    }
                }
                return;
            }
            if (key == KEY_RIGHT && cursorPos == getDocumentLength() && hasNextPage()) {
                if (!isReadOnly() && pageDirty) {
                    beginPendingPageAction(PAGED_ACTION_NEXT_PAGE);
                } else {
                    String error;
                    if (!loadPageByIndex(currentPageIndex + 1, error)) {
                        GUI::showToast(error.c_str(), 2000);
                    } else {
                        cursorPos = 0;
                        recalculateLayout();
                    }
                }
                return;
            }
        }
        if (key == KEY_LEFT && cursorPos > 0) cursorPos--;
        if (key == KEY_RIGHT && cursorPos < getDocumentLength()) cursorPos++;
        recalculateLayout();
        return;
    }

    if (isReadOnly()) {
        return;
    }

    if (key == KEY_ALT) {
        return;
    }

    if (key == KEY_SHIFT) {
        if (isKeyHeld(KEY_SHIFT)) {
            shiftTapPending = false;
            selectionMode = !selectionMode;
            recalculateLayout();
        } else {
            shiftTapPending = true;
        }
        return;
    }

    if (key == KEY_BKSP) {
        cursorMoveTime = millis();
        if (inputMode == MODE_ABC || (inputMode == MODE_T9 && fallback)) {
            if (tapKey != '\0') {
                tapKey = '\0';
                tapIndex = 0;
            } else if (cursorPos > 0) {
                documentBuffer.remove(cursorPos - 1, 1);
                cursorPos--;
                markPageDirty();
                if (fallback && cursorPos <= fallbackStart) {
                    fallback = false;
                }
            }
        } else if (inputMode == MODE_T9) {
            if (zeroPending) {
                zeroPending = false;
            } else if (tapKey == '1') {
                tapKey = '\0';
                tapIndex = 0;
                tapTime = 0;
            } else if (t9predict.hasInput()) {
                t9predict.popDigit();
                singleKeyCycleIndex = 0;
            } else if (cursorPos > 0) {
                documentBuffer.remove(cursorPos - 1, 1);
                cursorPos--;
                markPageDirty();
            }
        } else if (cursorPos > 0) {
            documentBuffer.remove(cursorPos - 1, 1);
            cursorPos--;
            markPageDirty();
        }
        recalculateLayout();
        return;
    }

    if (key == KEY_ENTER) {
        cursorMoveTime = millis();
        if (((inputMode == MODE_ABC || fallback) && tapKey != '\0') ||
            (inputMode == MODE_T9 && tapKey == '1')) {
            commitMultiTap();
        } else if (inputMode == MODE_T9 && t9predict.hasInput()) {
            commitPrediction();
        } else {
            if (tryInsertTextAtCursor("\n", 1)) {
                markPageDirty();
            }
        }
        fallback = false;
        recalculateLayout();
        return;
    }

    if (key < '0' || key > '9') {
        if (layoutChanged) recalculateLayout();
        return;
    }

    if (key == '0' && inputMode != MODE_123) {
        if (inputMode == MODE_T9 && tapKey == '1') {
            commitMultiTap();
        }
        if (inputMode == MODE_T9 && t9predict.hasInput()) {
            commitPrediction();
        }
        if ((inputMode == MODE_ABC || fallback) && tapKey != '\0') {
            tapKey = '\0';
            tapIndex = 0;
            tapTime = 0;
        }

        if (zeroPending && isKeyHeld('0')) {
            if (tryInsertTextAtCursor("0", 1)) {
                cursorMoveTime = millis();
                markPageDirty();
            }
            zeroPending = false;
            fallback = false;
        } else if (!zeroPending) {
            zeroPending = true;
        }
        recalculateLayout();
        return;
    }

    if ((inputMode == MODE_T9 || inputMode == MODE_ABC) && isKeyHeld(key)) {
        if (inputMode == MODE_T9 && key == '0') {
            return;
        }
        if (inputMode == MODE_T9 && tapKey == '1') {
            if (key == '1') {
                tapKey = '\0';
                tapIndex = 0;
                tapTime = 0;
            } else {
                commitMultiTap();
            }
        } else if ((inputMode == MODE_ABC || fallback) && tapKey != '\0') {
            tapKey = '\0';
            tapIndex = 0;
        }
        if (inputMode == MODE_T9 && !fallback) {
            if (t9predict.hasInput()) {
                t9predict.popDigit();
                if (t9predict.hasInput()) commitPrediction();
            }
        }
        if (tryInsertTextAtCursor(String(key), 1)) {
            cursorMoveTime = millis();
            markPageDirty();
        }
        recalculateLayout();
        return;
    }

    if (inputMode == MODE_123 && isKeyHeld(key)) {
        return;
    }

    if (inputMode == MODE_123) {
        if (tryInsertTextAtCursor(String(key), 1)) {
            markPageDirty();
        }
        recalculateLayout();
        return;
    }

    if (inputMode == MODE_ABC || (inputMode == MODE_T9 && fallback)) {
        if (fallback && key == '0') {
            if (tapKey != '\0') commitMultiTap();
            fallback = false;
            if (tryInsertTextAtCursor(" ", 1)) {
                markPageDirty();
            }
            recalculateLayout();
            return;
        }
        unsigned long now = millis();
        if (key == tapKey && (now - tapTime < MULTITAP_TIMEOUT)) {
            const char* map = multiTapMap[key - '0'];
            tapIndex = (tapIndex + 1) % strlen(map);
        } else {
            if (tapKey != '\0') commitMultiTap();
            tapKey = key;
            tapIndex = 0;
        }
        tapTime = now;
        recalculateLayout();
        return;
    }

    if (key == '1') {
        if (t9predict.hasInput()) commitPrediction();
        const char* map = multiTapMap[1];
        if (tapKey == '1') {
            tapIndex = (tapIndex + 1) % strlen(map);
        } else {
            tapKey = '1';
            tapIndex = 0;
        }
        tapTime = millis();
        recalculateLayout();
        return;
    }

    if (tapKey == '1') commitMultiTap();
    t9predict.pushDigit(key);
    singleKeyCycleIndex = 0;
    if (t9predict.getPrefixCandidateCount() == 0) {
        t9predict.popDigit();
        if (t9predict.hasInput()) commitPrediction();
        fallback = true;
        fallbackStart = cursorPos;
        tapKey = key;
        tapIndex = 0;
        tapTime = millis();
    }
    recalculateLayout();
}

void T9EditorApp::update() {
    bool layoutChanged = false;

    if (!isReadOnly() && shiftTapPending && !isKeyActiveNow(KEY_SHIFT)) {
        shiftTapPending = false;
        shiftMode = (shiftMode + 1) % 3;
        layoutChanged = true;
    }

    if (!isReadOnly() && !savePromptActive && (inputMode == MODE_ABC || fallback) && tapKey != '\0') {
        if (millis() - tapTime > MULTITAP_TIMEOUT) {
            bool isSeparator = false;
            if (fallback) {
                String c = getMultiTapChar();
                if (c == "." || c == "?" || c == "!" || c == ",") isSeparator = true;
            }
            commitMultiTap();
            if (isSeparator) fallback = false;
            layoutChanged = true;
        }
    }

    if (!isReadOnly() && !savePromptActive && inputMode != MODE_123 && zeroPending) {
        bool zeroStillPressed = false;
        for (int i = 0; i < activeKeyCount; i++) {
            if (activeKeys[i] == '0') {
                zeroStillPressed = true;
                break;
            }
        }

        if (!zeroStillPressed) {
            if (tryInsertTextAtCursor(" ", 1)) {
                cursorMoveTime = millis();
                layoutChanged = true;
                markPageDirty();
            }
            zeroPending = false;
        }
    }

    if (layoutChanged) {
        recalculateLayout();
    }
}

void T9EditorApp::recalculateLayout() {
    visualLines.clear();

    String preview;
    String displayText = getDisplayText(&preview);
    u8g2.setFont(getCurrentFontMetrics().font);

    int logicalLineCount = countLogicalLines();
    int textLeft = getTextLeft(logicalLineCount);
    int textAreaWidth = GUI::SCREEN_WIDTH - textLeft - 1;
    if (textAreaWidth < 1) textAreaWidth = 1;

    int currentLogicalLine = 1;
    int start = 0;
    while (start <= displayText.length()) {
        int end = displayText.indexOf('\n', start);
        if (end == -1) end = displayText.length();

        String segment = displayText.substring(start, end);
        int segmentLen = segment.length();
        if (segmentLen == 0) {
            VisualLine vl;
            vl.content = "";
            vl.logicalLineNum = currentLogicalLine;
            vl.byteStartIndex = start;
            vl.byteLength = 0;
            vl.hasCursor = (cursorPos == start);
            visualLines.push_back(vl);
        } else {
            int segStart = 0;
            while (segStart < segmentLen) {
                int fitLen = getWrappedFitLength(segment, segStart, segmentLen, textAreaWidth);

                VisualLine vl;
                vl.content = segment.substring(segStart, segStart + fitLen);
                vl.logicalLineNum = (segStart == 0) ? currentLogicalLine : -1;
                vl.byteStartIndex = start + segStart;
                vl.byteLength = fitLen;

                int absStart = start + segStart;
                int absEnd = absStart + fitLen;
                bool isSegmentEnd = (segStart + fitLen == segmentLen);
                if (cursorPos >= absStart && cursorPos < absEnd) {
                    vl.hasCursor = true;
                } else if (isSegmentEnd && cursorPos == absEnd) {
                    vl.hasCursor = true;
                } else {
                    vl.hasCursor = false;
                }

                visualLines.push_back(vl);
                segStart += fitLen;
            }
        }
        start = end + 1;
        currentLogicalLine++;
    }

    int cursorLineIndex = 0;
    for (int i = 0; i < (int)visualLines.size(); i++) {
        if (visualLines[i].hasCursor) {
            cursorLineIndex = i;
            break;
        }
    }

    int visibleLines = getVisibleLineCount();
    if (isReadOnlyPaged()) {
        int maxScroll = max(0, static_cast<int>(visualLines.size()) - visibleLines);
        if (scrollOffset < 0) scrollOffset = 0;
        if (scrollOffset > maxScroll) scrollOffset = maxScroll;
        return;
    }
    if (cursorLineIndex < scrollOffset) scrollOffset = cursorLineIndex;
    else if (cursorLineIndex >= scrollOffset + visibleLines) scrollOffset = cursorLineIndex - visibleLines + 1;
    if (scrollOffset < 0) scrollOffset = 0;
}

void T9EditorApp::renderHeader() {
    if (!showHeader()) return;

    char rightText[24];
    if (isReadOnly()) {
        if (totalPageCount > 0) {
            snprintf(rightText, sizeof(rightText), "RO %d/%d", currentPageIndex + 1, totalPageCount);
        } else {
            snprintf(rightText, sizeof(rightText), "RO");
        }
    } else {
        const char* modeStr = fallback ? "?ABC" :
                              (inputMode == MODE_T9) ? "T9" :
                              (inputMode == MODE_ABC) ? "ABC" : "123";
        const char* shiftStr = (shiftMode == 2) ? "^^" :
                               (shiftMode == 1) ? "^" : "";
        snprintf(rightText, sizeof(rightText), "RW %s%s", shiftStr, modeStr);
    }

    String title = documentLabel.length() > 0 ? documentLabel : String("T9 EDITOR");
    if (title.length() > 14) title = title.substring(0, 11) + "...";
    GUI::drawHeader(title.c_str(), rightText);
}

void T9EditorApp::renderFooter() const {
    if (!showFooter()) return;

    u8g2.drawHLine(0, 54, 128);
    u8g2.setFont(u8g2_font_5x8_tr);
    if (isReadOnly()) {
        u8g2.drawStr(1, 62, "RO U/D:scroll L/R:pg");
        return;
    }

    if (fallback && tapKey != '\0') {
        const char* map = multiTapMap[tapKey - '0'];
        char bar[32];
        snprintf(bar, sizeof(bar), "?[%s] %d/%d", map, tapIndex + 1, (int)strlen(map));
        u8g2.drawStr(1, 62, bar);
    } else if (fallback) {
        u8g2.drawStr(1, 62, "?ABC 0:sp exits");
    } else if (inputMode == MODE_T9 && t9predict.hasInput()) {
        char bar[40];
        int dc = t9predict.getDigitCount();
        if (dc == 1) {
            const char* digs = t9predict.getDigits();
            char digit = (digs && digs[0]) ? digs[0] : '\0';
            int letterCount = t9predict.getSingleKeyLetterCount(digit);
            int dictCount = t9predict.getPrefixCandidateCount();
            int total = letterCount + dictCount;
            if (total > 0) {
                int idx = singleKeyCycleIndex % total;
                if (idx < letterCount) {
                    const char* c = t9predict.getSingleKeyLetter(digit, idx);
                    snprintf(bar, sizeof(bar), "%s %d/%d", c ? c : "?", idx + 1, total);
                } else {
                    const char* w = t9predict.getPrefixCandidate(idx - letterCount);
                    snprintf(bar, sizeof(bar), "%s %d/%d", w ? w : "?", idx + 1, total);
                }
            } else {
                snprintf(bar, sizeof(bar), "? [%s]", t9predict.getDigits());
            }
        } else {
            const char* word = t9predict.getSelectedPrefixWord();
            if (word) {
                snprintf(bar, sizeof(bar), "%s %d/%d", word,
                         t9predict.getSelectedPrefixIndex() + 1,
                         t9predict.getPrefixCandidateCount());
            } else {
                snprintf(bar, sizeof(bar), "? [%s]", t9predict.getDigits());
            }
        }
        u8g2.drawStr(1, 62, bar);
    } else if (inputMode == MODE_T9 && tapKey == '1') {
        drawHighlightedChoiceBar(1, 62, multiTapMap[1], tapIndex);
    } else if (inputMode == MODE_ABC && tapKey != '\0') {
        const char* map = multiTapMap[tapKey - '0'];
        char bar[32];
        snprintf(bar, sizeof(bar), "[%s] %d/%d", map, tapIndex + 1, (int)strlen(map));
        u8g2.drawStr(1, 62, bar);
    } else {
        const char* hint = (inputMode == MODE_T9)  ? "A+TAB:ABC SH*:SEL" :
                           (inputMode == MODE_ABC) ? "A+TAB:123 SH*:SEL" :
                                                     "A+TAB:T9 SH*:SEL";
        u8g2.drawStr(1, 62, hint);
    }

    if (selectionMode) {
        const char* tag = "SEL";
        int tagWidth = u8g2.getStrWidth(tag);
        u8g2.drawStr(GUI::SCREEN_WIDTH - tagWidth - 1, 62, tag);
    }
}

void T9EditorApp::renderSavePrompt() {
    const char* labels[3] = {"No", "Save", "Cancel"};
    GUI::drawThreeOptionDialog("Save changes?", labels, savePromptSelection);
}

void T9EditorApp::renderPagePrompt() {
    GUI::drawPopupFrame(6, 10, 116, 44, true);
    u8g2.setFont(u8g2_font_5x8_tr);

    const char* message = "Dirty page";
    int msgWidth = u8g2.getStrWidth(message);
    u8g2.drawStr((GUI::SCREEN_WIDTH - msgWidth) / 2, 24, message);

    const char* labels[3] = {"Save", "Discard", "Cancel"};
    const int buttonY = 41;
    const int buttonWidth = 30;
    const int buttonXs[3] = {10, 48, 86};
    for (int i = 0; i < 3; i++) {
        if (pagePromptSelection == i) {
            u8g2.drawBox(buttonXs[i], buttonY - 8, buttonWidth, 10);
            u8g2.setDrawColor(0);
        }
        int textWidth = u8g2.getStrWidth(labels[i]);
        u8g2.drawStr(buttonXs[i] + (buttonWidth - textWidth) / 2, buttonY, labels[i]);
        u8g2.setDrawColor(1);
    }
}

void T9EditorApp::render() {
    renderHeader();

    String preview;
    getDisplayText(&preview);
    int logicalLineCount = countLogicalLines();
    int gutterWidth = getGutterWidth(logicalLineCount);
    int textLeft = getTextLeft(logicalLineCount);
    int textTop = showHeader() ? 13 : 1;
    int textBottom = showFooter() ? 53 : 63;
    int visibleLines = getVisibleLineCount();
    const T9EditorFontMetrics& metrics = getCurrentFontMetrics();

    u8g2.setFont(metrics.font);

    if (showLineNumbers()) {
        u8g2.drawVLine(gutterWidth, textTop, textBottom - textTop + 1);
    }

    int fbDispStart = -1;
    int fbDispEnd = -1;
    if (fallback) {
        fbDispStart = fallbackStart;
        fbDispEnd = cursorPos + (int)preview.length();
    }

    int y = textTop + metrics.baselineOffset;
    for (int i = 0; i < visibleLines; i++) {
        int idx = scrollOffset + i;
        if (idx >= (int)visualLines.size()) break;

        const VisualLine& vl = visualLines[idx];
        if (showLineNumbers() && vl.logicalLineNum != -1) {
            char ln[12];
            snprintf(ln, sizeof(ln), "%d", vl.logicalLineNum);
            u8g2.drawStr(1, y, ln);
        }

        if (fbDispStart >= 0 && fbDispStart < vl.byteStartIndex + vl.byteLength && fbDispEnd > vl.byteStartIndex) {
            int regionS = max(fbDispStart, vl.byteStartIndex) - vl.byteStartIndex;
            int regionE = min(fbDispEnd, vl.byteStartIndex + vl.byteLength) - vl.byteStartIndex;
            if (regionS > 0) {
                String before = vl.content.substring(0, regionS);
                u8g2.drawUTF8(textLeft, y, before.c_str());
            }

            String fbPart = vl.content.substring(regionS, regionE);
            int xFb = textLeft + u8g2.getUTF8Width(vl.content.substring(0, regionS).c_str());
            int wFb = u8g2.getUTF8Width(fbPart.c_str());
            u8g2.drawBox(xFb, y - metrics.glyphTopOffset, wFb, metrics.boxHeight);
            u8g2.setDrawColor(0);
            u8g2.drawUTF8(xFb, y, fbPart.c_str());
            u8g2.setDrawColor(1);

            if (regionE < (int)vl.content.length()) {
                String after = vl.content.substring(regionE);
                u8g2.drawUTF8(xFb + wFb, y, after.c_str());
            }
        } else {
            u8g2.drawUTF8(textLeft, y, vl.content.c_str());
        }

        if (!isReadOnly() && vl.hasCursor) {
            int localCursorIdx = cursorPos - vl.byteStartIndex;
            if (localCursorIdx < 0) localCursorIdx = 0;
            if (localCursorIdx > vl.byteLength) localCursorIdx = vl.byteLength;

            String before = vl.content.substring(0, localCursorIdx);
            int cursorX = textLeft + u8g2.getUTF8Width(before.c_str());
            if (preview.length() > 0) {
                int pw = u8g2.getUTF8Width(preview.c_str());
                u8g2.drawHLine(cursorX, y + metrics.underlineOffset, pw);
            } else {
                bool recentMove = (millis() - cursorMoveTime < CURSOR_BLINK_RATE);
                if (recentMove || (millis() / CURSOR_BLINK_RATE) % 2) {
                    u8g2.drawVLine(cursorX, y - metrics.glyphTopOffset, metrics.cursorHeight);
                }
            }
        }
        y += metrics.lineHeight;
    }

    renderFooter();
    if (GUI::updateToast()) return;
    if (pagePromptActive) renderPagePrompt();
    if (savePromptActive) renderSavePrompt();
}