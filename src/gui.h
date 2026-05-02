// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/gui.h
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TACTICAL_TODO TASK_2
// LOG_REF: 2026-05-02

#ifndef GUI_H
#define GUI_H

#include <Arduino.h>

// Forward declaration to avoid circular includes
class U8G2;

namespace GUI {

// ==========================================================================
// SCREEN & LAYOUT CONSTANTS
// ==========================================================================

constexpr int SCREEN_WIDTH  = 128;
constexpr int SCREEN_HEIGHT = 64;

constexpr int HEADER_HEIGHT   = 12;   // Standard header bar height
constexpr int FOOTER_HEIGHT   = 10;   // Standard footer height
constexpr int LINE_HEIGHT     = 10;   // Standard text line height
constexpr int CONTENT_START_Y = 24;   // Y position where content typically starts
constexpr int SCROLLBAR_WIDTH = 3;    // Width of scrollbar track/knob

// Content area (between header and footer)
constexpr int CONTENT_HEIGHT = SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT;
constexpr int VISIBLE_LINES  = CONTENT_HEIGHT / LINE_HEIGHT;  // ~4 lines

// ==========================================================================
// FONT DEFINITIONS
// ==========================================================================

constexpr int FONT_SIZE_TINY = 0;
constexpr int FONT_SIZE_SMALL = 1;
constexpr int FONT_SIZE_MEDIUM = 2;
constexpr int FONT_SIZE_COUNT = 3;

struct FontMetrics {
    const uint8_t* font;
    int lineHeight;
    int baselineOffset;
    int glyphTopOffset;
    int boxHeight;
    int cursorHeight;
    int underlineOffset;
};

// Standard font set - use these instead of hardcoding font names
extern const uint8_t* FONT_TINY;      // 4x6  - Minimal, for dense info
extern const uint8_t* FONT_SMALL;     // 5x7  - Default body text
extern const uint8_t* FONT_MEDIUM;    // 5x8  - Dense emphasized text
extern const uint8_t* FONT_LARGE;     // Legacy alias of medium
extern const uint8_t* FONT_TITLE;     // Legacy alias of medium

extern const char* kSystemFontOptionLabels[];
extern const int kSystemFontOptionCount;

const FontMetrics& getFontMetrics(int fontSize);
const FontMetrics& getSystemFontMetrics();
void setFontBySize(int fontSize);
void setFontSystem();
int getSystemFontSize();
int getSystemFontOptionIndex();
const char* getSystemFontOptionLabel(int index);
bool setSystemFontOptionIndex(int index);
int getSecondaryFontSize(int fontSize = -1);
const FontMetrics& getSecondaryFontMetrics();
void setFontSecondary();
int getHeaderHeight(int fontSize = -1);
int getHeaderBaselineY(int fontSize = -1);
int getFooterHeight(int fontSize = -1);
int getFooterSeparatorY(int fontSize = -1);
int getFooterBaselineY(int fontSize = -1);
int getContentAreaTop(int fontSize = -1);
int getContentBaselineStart(int fontSize = -1);
int getContentBottom(int fontSize = -1);
int getContentHeight(int fontSize = -1);
int getListLineHeight(int fontSize = -1);
int getVisibleListRows(int fontSize = -1);
int getHighlightTop(int baselineY, int fontSize = -1);
int getHighlightHeight(int fontSize = -1);
int getHighlightPaddingX(int fontSize = -1);
String truncateStringToWidth(const String& str, int maxWidth, const char* ellipsis = "...");

// ==========================================================================
// HEADER COMPONENTS
// ==========================================================================

/**
 * Draw standard inverted header bar with title.
 * @param title Main title text (left-aligned)
 * @param rightText Optional right-aligned text (status, count, etc.)
 */
void drawHeader(const char* title, const char* rightText = nullptr);

/**
 * Draw header with icon indicator.
 * @param title Main title text
 * @param icon Single character icon (e.g., '*' for modified)
 */
void drawHeaderWithIcon(const char* title, char icon);

// ==========================================================================
// FOOTER COMPONENTS
// ==========================================================================

/**
 * Draw footer with centered text.
 * @param text Footer message
 */
void drawFooter(const char* text);

/**
 * Draw footer with left and right hints (for button labels).
 * @param leftHint Left-side hint (e.g., "5:Select")
 * @param rightHint Right-side hint (e.g., "D:Back")
 */
void drawFooterHints(const char* leftHint, const char* rightHint = nullptr);

// ==========================================================================
// LIST & SCROLLING COMPONENTS
// ==========================================================================

/**
 * Configuration for list rendering.
 */
struct ListConfig {
    int startY        = -1;               // Y baseline of first item (-1 = auto)
    int visibleItems  = 0;                // Number of visible items (0 = auto)
    int lineHeight    = 0;                // Height per item (0 = auto)
    bool showScrollbar = true;            // Show scrollbar when needed
    bool showSelector  = true;            // Show ">" selector prefix
    int leftMargin     = -1;              // Text left margin (-1 = auto)
};

/**
 * Draw a scrollable list of string items.
 * @param items Array of C strings
 * @param itemCount Total number of items
 * @param selectedIndex Currently selected index
 * @param scrollOffset First visible item index
 * @param config List rendering configuration
 */
void drawList(const char* const* items, int itemCount, int selectedIndex, 
              int scrollOffset, const ListConfig& config = ListConfig());

/**
 * Draw a scrollable list of Arduino Strings.
 * @param items Array of String objects
 * @param itemCount Total number of items
 * @param selectedIndex Currently selected index
 * @param scrollOffset First visible item index
 * @param config List rendering configuration
 */
void drawStringList(const String* items, int itemCount, int selectedIndex,
                    int scrollOffset, const ListConfig& config = ListConfig());

/**
 * Draw vertical scrollbar.
 * @param x X position (typically SCREEN_WIDTH - SCROLLBAR_WIDTH - 1)
 * @param yStart Y start position
 * @param height Total scrollbar height
 * @param totalItems Total number of items
 * @param visibleItems Number of visible items
 * @param scrollOffset Current scroll position
 */
void drawScrollbar(int x, int yStart, int height,
                   int totalItems, int visibleItems, int scrollOffset);

// ==========================================================================
// SELECTION & HIGHLIGHT
// ==========================================================================

/**
 * Draw inverted highlight box.
 * @param x, y Position
 * @param width, height Dimensions
 */
void drawHighlight(int x, int y, int width, int height);

/**
 * Draw text with optional inverted highlight.
 * @param x, y Position
 * @param text Text to draw
 * @param selected If true, draws inverted (white on black)
 * @param width Highlight width (0 = auto from text)
 */
void drawSelectableText(int x, int y, const char* text, bool selected, int width = 0);

// ==========================================================================
// POPUP & DIALOG COMPONENTS
// ==========================================================================

/**
 * Draw a popup frame (box with border).
 * @param x, y Top-left position
 * @param width, height Dimensions
 * @param clearBackground If true, fills with white first
 */
void drawPopupFrame(int x, int y, int width, int height, bool clearBackground = true);

/**
 * Draw centered popup frame.
 * @param width, height Popup dimensions
 * @param clearBackground If true, fills with white first
 */
void drawCenteredPopup(int width, int height, bool clearBackground = true);

/**
 * Draw a context menu with items.
 * @param items Menu item strings
 * @param itemCount Number of items
 * @param selectedIndex Currently selected item
 * @param x, y Menu position
 */
void drawContextMenu(const char* const* items, int itemCount, int selectedIndex,
                     int x = 16, int y = 16);

/**
 * Draw Yes/No confirmation dialog.
 * @param message Prompt message
 * @param yesSelected If true, YES is highlighted
 */
void drawYesNoDialog(const char* message, bool yesSelected);

/**
 * Draw a centered informational dialog using the same popup chrome as confirmations.
 * @param message Message text
 * @param buttonLabel Label for the dismiss button
 * @param invertButton If true, draw the button with inverted colors like the default prompt style
 */
void drawMessageDialog(const char* message, const char* buttonLabel = "OK", bool invertButton = true);

/**
 * Draw a centered three-option dialog using the standard popup chrome.
 * @param message Prompt text
 * @param labels Three button labels
 * @param selectedIndex Selected button index [0..2]
 */
void drawThreeOptionDialog(const char* message, const char* const labels[3], int selectedIndex);

// ==========================================================================
// TOAST / TEMPORARY MESSAGES
// ==========================================================================

/**
 * Show a toast message at the bottom of screen.
 * @param message Message to display
 * @param durationMs How long to show (default 2000ms)
 */
void showToast(const char* message, unsigned long durationMs = 2000);

/**
 * Update and render active toast. Call in render loop.
 * @param footerTopY Top Y position of the current footer area, or SCREEN_HEIGHT when no footer is visible
 * @return true if a toast is currently visible
 */
bool updateToast(int footerTopY = -1);

/**
 * Clear any active toast immediately.
 */
void clearToast();

// ==========================================================================
// GAME UI COMPONENTS
// ==========================================================================

/**
 * Draw game over screen with score.
 * @param title Title text (e.g., "GAME OVER")
 * @param score Score to display
 * @param restartHint Hint text for restarting
 */
void drawGameOver(const char* title, int score, const char* restartHint = "5 TO RESTART");

/**
 * Draw a simple score display.
 * @param x, y Position
 * @param label Label text
 * @param value Score value
 */
void drawScore(int x, int y, const char* label, int value);

// ==========================================================================
// NAVIGATION HELPERS
// ==========================================================================

/**
 * State structure for scrollable lists.
 */
struct ScrollState {
    int selectedIndex;
    int scrollOffset;
    int totalItems;
    int visibleItems;
};

/**
 * Navigate up in a scrollable list.
 * @param state Scroll state to modify
 * @param wrap If true, wraps from top to bottom
 */
void navigateUp(ScrollState& state, bool wrap = true);

/**
 * Navigate down in a scrollable list.
 * @param state Scroll state to modify
 * @param wrap If true, wraps from bottom to top
 */
void navigateDown(ScrollState& state, bool wrap = true);

/**
 * Handle standard list navigation input.
 * @param state Scroll state to modify
 * @param key Input key ('2' = up, '8' = down)
 * @return true if navigation occurred
 */
bool handleListNavigation(ScrollState& state, char key);

// ==========================================================================
// UTILITY FUNCTIONS
// ==========================================================================

/**
 * Truncate string to fit width, adding "..." if needed.
 * @param str Input string
 * @param maxChars Maximum character count
 * @return Truncated string
 */
String truncateString(const String& str, int maxChars);

/**
 * Center text horizontally.
 * @param text Text to center
 * @return X position for centered text
 */
int centerTextX(const char* text);

/**
 * Get width of text string in current font.
 * @param text Text to measure
 * @return Width in pixels
 */
int getTextWidth(const char* text);

/**
 * Set the standard tiny font.
 */
void setFontTiny();

/**
 * Set the standard small font.
 */
void setFontSmall();

/**
 * Set the standard medium font.
 */
void setFontMedium();

/**
 * Set the standard large/title font.
 */
void setFontLarge();

} // namespace GUI

#endif // GUI_H
