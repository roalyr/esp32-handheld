// [Revision: v1.0] [Path: src/gui.cpp] [Date: 2025-12-11]
// Description: Implementation of unified GUI module.

#include "gui.h"
#include "hal.h"

namespace GUI {

// ==========================================================================
// FONT DEFINITIONS
// ==========================================================================

const uint8_t* FONT_TINY   = u8g2_font_4x6_tf;
const uint8_t* FONT_SMALL  = u8g2_font_5x7_tf;
const uint8_t* FONT_MEDIUM = u8g2_font_6x10_tf;
const uint8_t* FONT_LARGE  = u8g2_font_8x13_tf;
const uint8_t* FONT_TITLE  = u8g2_font_ncenB12_tr;

// ==========================================================================
// TOAST STATE
// ==========================================================================

static String toastMessage = "";
static unsigned long toastEndTime = 0;

// ==========================================================================
// HEADER COMPONENTS
// ==========================================================================

void drawHeader(const char* title, const char* rightText) {
    // Draw inverted header bar
    u8g2.drawBox(0, 0, SCREEN_WIDTH, HEADER_HEIGHT);
    u8g2.setDrawColor(0);
    u8g2.setFont(FONT_SMALL);
    
    // Left-aligned title
    u8g2.drawStr(2, 9, title);
    
    // Optional right-aligned text
    if (rightText != nullptr) {
        int textWidth = u8g2.getStrWidth(rightText);
        u8g2.drawStr(SCREEN_WIDTH - textWidth - 2, 9, rightText);
    }
    
    u8g2.setDrawColor(1);
}

void drawHeaderWithIcon(const char* title, char icon) {
    char rightText[4] = {icon, '\0'};
    drawHeader(title, rightText);
}

// ==========================================================================
// FOOTER COMPONENTS
// ==========================================================================

void drawFooter(const char* text) {
    u8g2.setFont(FONT_SMALL);
    u8g2.drawHLine(0, SCREEN_HEIGHT - FOOTER_HEIGHT - 1, SCREEN_WIDTH);
    
    // Center text
    int textWidth = u8g2.getStrWidth(text);
    int x = (SCREEN_WIDTH - textWidth) / 2;
    u8g2.drawStr(x, SCREEN_HEIGHT - 2, text);
}

void drawFooterHints(const char* leftHint, const char* rightHint) {
    u8g2.setFont(FONT_SMALL);
    u8g2.drawHLine(0, SCREEN_HEIGHT - FOOTER_HEIGHT - 1, SCREEN_WIDTH);
    
    // Left hint
    if (leftHint != nullptr) {
        u8g2.drawStr(2, SCREEN_HEIGHT - 2, leftHint);
    }
    
    // Right hint (offset from scrollbar area)
    if (rightHint != nullptr) {
        int textWidth = u8g2.getStrWidth(rightHint);
        u8g2.drawStr(SCREEN_WIDTH - textWidth - SCROLLBAR_WIDTH - 4, SCREEN_HEIGHT - 2, rightHint);
    }
}

// ==========================================================================
// LIST & SCROLLING COMPONENTS
// ==========================================================================

void drawList(const char* const* items, int itemCount, int selectedIndex,
              int scrollOffset, const ListConfig& config) {
    u8g2.setFont(FONT_SMALL);
    
    for (int i = 0; i < config.visibleItems && (scrollOffset + i) < itemCount; i++) {
        int idx = scrollOffset + i;
        int y = config.startY + (i * config.lineHeight);
        
        // Draw selector arrow
        if (config.showSelector && idx == selectedIndex) {
            u8g2.drawStr(0, y, ">");
        }
        
        // Highlight selected item
        if (idx == selectedIndex) {
            u8g2.drawBox(config.leftMargin - 2, y - 8, 
                        SCREEN_WIDTH - config.leftMargin - SCROLLBAR_WIDTH - 2, 
                        config.lineHeight);
            u8g2.setDrawColor(0);
        }
        
        u8g2.drawStr(config.leftMargin, y, items[idx]);
        u8g2.setDrawColor(1);
    }
    
    // Draw scrollbar if needed
    if (config.showScrollbar && itemCount > config.visibleItems) {
        int scrollHeight = config.visibleItems * config.lineHeight;
        drawScrollbar(SCREEN_WIDTH - SCROLLBAR_WIDTH - 1, 
                      config.startY - 8,
                      scrollHeight, 
                      itemCount, 
                      config.visibleItems, 
                      scrollOffset);
    }
}

void drawStringList(const String* items, int itemCount, int selectedIndex,
                    int scrollOffset, const ListConfig& config) {
    u8g2.setFont(FONT_SMALL);
    
    for (int i = 0; i < config.visibleItems && (scrollOffset + i) < itemCount; i++) {
        int idx = scrollOffset + i;
        int y = config.startY + (i * config.lineHeight);
        
        // Draw selector arrow
        if (config.showSelector && idx == selectedIndex) {
            u8g2.drawStr(0, y, ">");
        }
        
        // Highlight selected item
        if (idx == selectedIndex) {
            u8g2.drawBox(config.leftMargin - 2, y - 8, 
                        SCREEN_WIDTH - config.leftMargin - SCROLLBAR_WIDTH - 2, 
                        config.lineHeight);
            u8g2.setDrawColor(0);
        }
        
        // Truncate long strings
        String displayStr = truncateString(items[idx], 18);
        u8g2.drawStr(config.leftMargin, y, displayStr.c_str());
        u8g2.setDrawColor(1);
    }
    
    // Draw scrollbar if needed
    if (config.showScrollbar && itemCount > config.visibleItems) {
        int scrollHeight = config.visibleItems * config.lineHeight;
        drawScrollbar(SCREEN_WIDTH - SCROLLBAR_WIDTH - 1,
                      config.startY - 8,
                      scrollHeight,
                      itemCount,
                      config.visibleItems,
                      scrollOffset);
    }
}

void drawScrollbar(int x, int yStart, int height,
                   int totalItems, int visibleItems, int scrollOffset) {
    if (totalItems <= visibleItems) return;
    
    // Draw track
    u8g2.drawVLine(x + 1, yStart, height);
    
    // Calculate knob size and position
    int knobHeight = (height * visibleItems) / totalItems;
    if (knobHeight < 4) knobHeight = 4;  // Minimum knob size
    
    int knobY = yStart + (scrollOffset * (height - knobHeight)) / (totalItems - visibleItems);
    
    // Draw knob
    u8g2.drawBox(x, knobY, SCROLLBAR_WIDTH, knobHeight);
}

// ==========================================================================
// SELECTION & HIGHLIGHT
// ==========================================================================

void drawHighlight(int x, int y, int width, int height) {
    u8g2.drawBox(x, y, width, height);
}

void drawSelectableText(int x, int y, const char* text, bool selected, int width) {
    u8g2.setFont(FONT_SMALL);
    
    if (selected) {
        int w = (width > 0) ? width : u8g2.getStrWidth(text) + 4;
        u8g2.drawBox(x - 2, y - 8, w, LINE_HEIGHT);
        u8g2.setDrawColor(0);
    }
    
    u8g2.drawStr(x, y, text);
    u8g2.setDrawColor(1);
}

// ==========================================================================
// POPUP & DIALOG COMPONENTS
// ==========================================================================

void drawPopupFrame(int x, int y, int width, int height, bool clearBackground) {
    if (clearBackground) {
        u8g2.setDrawColor(0);
        u8g2.drawBox(x + 1, y + 1, width - 2, height - 2);
        u8g2.setDrawColor(1);
    }
    u8g2.drawFrame(x, y, width, height);
}

void drawCenteredPopup(int width, int height, bool clearBackground) {
    int x = (SCREEN_WIDTH - width) / 2;
    int y = (SCREEN_HEIGHT - height) / 2;
    drawPopupFrame(x, y, width, height, clearBackground);
}

void drawContextMenu(const char* const* items, int itemCount, int selectedIndex,
                     int x, int y) {
    // Calculate menu dimensions
    u8g2.setFont(FONT_SMALL);
    int maxWidth = 0;
    for (int i = 0; i < itemCount; i++) {
        int w = u8g2.getStrWidth(items[i]);
        if (w > maxWidth) maxWidth = w;
    }
    
    int menuWidth = maxWidth + 12;
    int menuHeight = itemCount * LINE_HEIGHT + 6;
    
    // Draw frame
    drawPopupFrame(x, y, menuWidth, menuHeight, true);
    
    // Draw items
    for (int i = 0; i < itemCount; i++) {
        int itemY = y + 10 + (i * LINE_HEIGHT);
        drawSelectableText(x + 4, itemY, items[i], i == selectedIndex, menuWidth - 8);
    }
}

void drawYesNoDialog(const char* message, bool yesSelected) {
    // Draw popup frame
    drawPopupFrame(10, 12, 108, 40, true);
    
    // Draw message
    u8g2.setFont(FONT_SMALL);
    int msgWidth = u8g2.getStrWidth(message);
    int msgX = (SCREEN_WIDTH - msgWidth) / 2;
    u8g2.drawStr(msgX, 28, message);
    
    // Draw YES / NO buttons
    const int buttonY = 44;
    const int buttonWidth = 30;
    const int yesX = 25;
    const int noX = 73;
    
    // YES button
    if (yesSelected) {
        u8g2.drawBox(yesX, buttonY - 8, buttonWidth, LINE_HEIGHT);
        u8g2.setDrawColor(0);
    }
    u8g2.drawStr(yesX + 6, buttonY, "YES");
    u8g2.setDrawColor(1);
    
    // NO button
    if (!yesSelected) {
        u8g2.drawBox(noX, buttonY - 8, buttonWidth, LINE_HEIGHT);
        u8g2.setDrawColor(0);
    }
    u8g2.drawStr(noX + 9, buttonY, "NO");
    u8g2.setDrawColor(1);
}

// ==========================================================================
// TOAST / TEMPORARY MESSAGES
// ==========================================================================

void showToast(const char* message, unsigned long durationMs) {
    toastMessage = message;
    toastEndTime = millis() + durationMs;
}

bool updateToast() {
    if (toastMessage.length() == 0 || millis() > toastEndTime) {
        toastMessage = "";
        return false;
    }
    
    // Draw toast at bottom
    u8g2.setFont(FONT_SMALL);
    int msgWidth = u8g2.getStrWidth(toastMessage.c_str());
    int boxWidth = msgWidth + 8;
    int boxX = (SCREEN_WIDTH - boxWidth) / 2;
    
    u8g2.drawFrame(boxX, 54, boxWidth, 10);
    u8g2.drawStr(boxX + 4, 62, toastMessage.c_str());
    
    return true;
}

void clearToast() {
    toastMessage = "";
    toastEndTime = 0;
}

// ==========================================================================
// GAME UI COMPONENTS
// ==========================================================================

void drawGameOver(const char* title, int score, const char* restartHint) {
    u8g2.setFont(FONT_TITLE);
    int titleWidth = u8g2.getStrWidth(title);
    u8g2.drawStr((SCREEN_WIDTH - titleWidth) / 2, 24, title);
    
    u8g2.setFont(FONT_SMALL);
    char scoreStr[20];
    snprintf(scoreStr, sizeof(scoreStr), "Score: %d", score);
    int scoreWidth = u8g2.getStrWidth(scoreStr);
    u8g2.drawStr((SCREEN_WIDTH - scoreWidth) / 2, 38, scoreStr);
    
    int hintWidth = u8g2.getStrWidth(restartHint);
    u8g2.drawStr((SCREEN_WIDTH - hintWidth) / 2, 55, restartHint);
}

void drawScore(int x, int y, const char* label, int value) {
    u8g2.setFont(FONT_SMALL);
    char str[32];
    snprintf(str, sizeof(str), "%s%d", label, value);
    u8g2.drawStr(x, y, str);
}

// ==========================================================================
// NAVIGATION HELPERS
// ==========================================================================

void navigateUp(ScrollState& state, bool wrap) {
    state.selectedIndex--;
    
    if (state.selectedIndex < 0) {
        if (wrap) {
            // Wrap to bottom
            state.selectedIndex = state.totalItems - 1;
            state.scrollOffset = state.totalItems - state.visibleItems;
            if (state.scrollOffset < 0) state.scrollOffset = 0;
        } else {
            state.selectedIndex = 0;
        }
    } else if (state.selectedIndex < state.scrollOffset) {
        // Scroll up
        state.scrollOffset = state.selectedIndex;
    }
}

void navigateDown(ScrollState& state, bool wrap) {
    state.selectedIndex++;
    
    if (state.selectedIndex >= state.totalItems) {
        if (wrap) {
            // Wrap to top
            state.selectedIndex = 0;
            state.scrollOffset = 0;
        } else {
            state.selectedIndex = state.totalItems - 1;
        }
    } else if (state.selectedIndex >= state.scrollOffset + state.visibleItems) {
        // Scroll down
        state.scrollOffset = state.selectedIndex - state.visibleItems + 1;
    }
}

bool handleListNavigation(ScrollState& state, char key) {
    if (key == '2') {
        navigateUp(state);
        return true;
    }
    if (key == '8') {
        navigateDown(state);
        return true;
    }
    return false;
}

// ==========================================================================
// UTILITY FUNCTIONS
// ==========================================================================

String truncateString(const String& str, int maxChars) {
    if ((int)str.length() <= maxChars) {
        return str;
    }
    return str.substring(0, maxChars - 3) + "...";
}

int centerTextX(const char* text) {
    int textWidth = u8g2.getStrWidth(text);
    return (SCREEN_WIDTH - textWidth) / 2;
}

int getTextWidth(const char* text) {
    return u8g2.getStrWidth(text);
}

void setFontSmall() {
    u8g2.setFont(FONT_SMALL);
}

void setFontMedium() {
    u8g2.setFont(FONT_MEDIUM);
}

void setFontLarge() {
    u8g2.setFont(FONT_TITLE);
}

} // namespace GUI
