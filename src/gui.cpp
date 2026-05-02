// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/gui.cpp
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TACTICAL_TODO TASK_2
// LOG_REF: 2026-05-02

#include "gui.h"
#include "hal.h"

namespace GUI {

// ==========================================================================
// FONT DEFINITIONS
// ==========================================================================

static const FontMetrics kSystemFontMetrics[FONT_SIZE_COUNT] = {
    {u8g2_font_4x6_tf, 7, 6, 5, 7, 6, 1},
    {u8g2_font_5x7_tf, 8, 7, 6, 8, 7, 1},
    {u8g2_font_5x8_tr, 9, 8, 7, 9, 8, 2},
};

const char* kSystemFontOptionLabels[] = {"Medium", "Small", "Tiny"};
const int kSystemFontOptionCount = 3;
static const int kSystemFontOptionToSize[kSystemFontOptionCount] = {
    FONT_SIZE_MEDIUM,
    FONT_SIZE_SMALL,
    FONT_SIZE_TINY,
};
static int gSystemFontOptionIndex = 0;

const uint8_t* FONT_TINY   = kSystemFontMetrics[FONT_SIZE_TINY].font;
const uint8_t* FONT_SMALL  = kSystemFontMetrics[FONT_SIZE_SMALL].font;
const uint8_t* FONT_MEDIUM = kSystemFontMetrics[FONT_SIZE_MEDIUM].font;
const uint8_t* FONT_LARGE  = kSystemFontMetrics[FONT_SIZE_MEDIUM].font;
const uint8_t* FONT_TITLE  = kSystemFontMetrics[FONT_SIZE_MEDIUM].font;

int getSystemFontSize() {
    int optionIndex = gSystemFontOptionIndex;
    if (optionIndex < 0 || optionIndex >= kSystemFontOptionCount) {
        optionIndex = 0;
    }
    return kSystemFontOptionToSize[optionIndex];
}

static int normalizeExplicitFontSize(int fontSize) {
    if (fontSize < 0 || fontSize >= FONT_SIZE_COUNT) {
        return FONT_SIZE_SMALL;
    }
    return fontSize;
}

static int resolveFontSize(int fontSize) {
    if (fontSize < 0) {
        return getSystemFontSize();
    }
    return normalizeExplicitFontSize(fontSize);
}

const FontMetrics& getFontMetrics(int fontSize) {
    return kSystemFontMetrics[resolveFontSize(fontSize)];
}

const FontMetrics& getSystemFontMetrics() {
    return kSystemFontMetrics[getSystemFontSize()];
}

int getSystemFontOptionIndex() {
    return gSystemFontOptionIndex;
}

const char* getSystemFontOptionLabel(int index) {
    if (index < 0 || index >= kSystemFontOptionCount) {
        return kSystemFontOptionLabels[0];
    }
    return kSystemFontOptionLabels[index];
}

bool setSystemFontOptionIndex(int index) {
    if (index < 0 || index >= kSystemFontOptionCount) {
        return false;
    }
    gSystemFontOptionIndex = index;
    return true;
}

int getSecondaryFontSize(int fontSize) {
    int resolved = resolveFontSize(fontSize);
    if (resolved == FONT_SIZE_MEDIUM) {
        return FONT_SIZE_SMALL;
    }
    return FONT_SIZE_TINY;
}

const FontMetrics& getSecondaryFontMetrics() {
    return getFontMetrics(getSecondaryFontSize());
}

void setFontBySize(int fontSize) {
    u8g2.setFont(getFontMetrics(fontSize).font);
}

void setFontSystem() {
    setFontBySize(getSystemFontSize());
}

void setFontSecondary() {
    setFontBySize(getSecondaryFontSize());
}

static int getHorizontalPaddingForFont(int fontSize) {
    return resolveFontSize(fontSize) == FONT_SIZE_TINY ? 1 : 2;
}

int getHeaderHeight(int fontSize) {
    return getFontMetrics(fontSize).lineHeight + 3;
}

int getHeaderBaselineY(int fontSize) {
    return getFontMetrics(fontSize).baselineOffset + 1;
}

int getFooterHeight(int fontSize) {
    return getFontMetrics(fontSize).lineHeight + 2;
}

int getFooterSeparatorY(int fontSize) {
    return SCREEN_HEIGHT - getFooterHeight(fontSize);
}

int getFooterBaselineY(int fontSize) {
    return SCREEN_HEIGHT - getHorizontalPaddingForFont(fontSize);
}

int getContentAreaTop(int fontSize) {
    return getHeaderHeight(fontSize) + 2;
}

int getContentBaselineStart(int fontSize) {
    return getContentAreaTop(fontSize) + getFontMetrics(fontSize).baselineOffset;
}

int getContentBottom(int fontSize) {
    return getFooterSeparatorY(fontSize) - 1;
}

int getContentHeight(int fontSize) {
    return getContentBottom(fontSize) - getContentAreaTop(fontSize) + 1;
}

int getListLineHeight(int fontSize) {
    return getFontMetrics(fontSize).lineHeight;
}

int getVisibleListRows(int fontSize) {
    return max(1, getContentHeight(fontSize) / getListLineHeight(fontSize));
}

int getHighlightTop(int baselineY, int fontSize) {
    return baselineY - getFontMetrics(fontSize).glyphTopOffset;
}

int getHighlightHeight(int fontSize) {
    return getFontMetrics(fontSize).boxHeight;
}

int getHighlightPaddingX(int fontSize) {
    return getHorizontalPaddingForFont(fontSize);
}

// ==========================================================================
// TOAST STATE
// ==========================================================================

static String toastMessage = "";
static unsigned long toastEndTime = 0;

// ==========================================================================
// HEADER COMPONENTS
// ==========================================================================

void drawHeader(const char* title, const char* rightText) {
    const int fontSize = getSystemFontSize();
    const int headerHeight = getHeaderHeight(fontSize);
    const int baselineY = getHeaderBaselineY(fontSize);
    const int padX = getHorizontalPaddingForFont(fontSize);

    u8g2.drawBox(0, 0, SCREEN_WIDTH, headerHeight);
    u8g2.setDrawColor(0);
    setFontSystem();

    String right = rightText ? truncateStringToWidth(String(rightText), (SCREEN_WIDTH / 2) - (padX * 2)) : String("");
    int rightWidth = right.length() > 0 ? u8g2.getUTF8Width(right.c_str()) : 0;
    int titleMaxWidth = SCREEN_WIDTH - (padX * 2) - (rightWidth > 0 ? rightWidth + padX + 1 : 0);
    String titleText = truncateStringToWidth(String(title ? title : ""), max(1, titleMaxWidth));
    u8g2.drawUTF8(padX, baselineY, titleText.c_str());

    if (rightWidth > 0) {
        u8g2.drawUTF8(SCREEN_WIDTH - rightWidth - padX, baselineY, right.c_str());
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
    const int fontSize = getSystemFontSize();
    const int baselineY = getFooterBaselineY(fontSize);
    const int separatorY = getFooterSeparatorY(fontSize);
    const int maxWidth = SCREEN_WIDTH - (getHorizontalPaddingForFont(fontSize) * 2);

    setFontSystem();
    u8g2.drawHLine(0, separatorY, SCREEN_WIDTH);

    String footerText = truncateStringToWidth(String(text ? text : ""), maxWidth);
    int textWidth = u8g2.getUTF8Width(footerText.c_str());
    int x = (SCREEN_WIDTH - textWidth) / 2;
    u8g2.drawUTF8(x, baselineY, footerText.c_str());
}

void drawFooterHints(const char* leftHint, const char* rightHint) {
    const int fontSize = getSystemFontSize();
    const int baselineY = getFooterBaselineY(fontSize);
    const int separatorY = getFooterSeparatorY(fontSize);
    const int padX = getHorizontalPaddingForFont(fontSize);
    const int halfWidth = (SCREEN_WIDTH - SCROLLBAR_WIDTH - (padX * 3)) / 2;

    setFontSystem();
    u8g2.drawHLine(0, separatorY, SCREEN_WIDTH);

    if (leftHint != nullptr && leftHint[0] != '\0') {
        String left = truncateStringToWidth(String(leftHint), halfWidth);
        u8g2.drawUTF8(padX, baselineY, left.c_str());
    }

    if (rightHint != nullptr && rightHint[0] != '\0') {
        String right = truncateStringToWidth(String(rightHint), halfWidth);
        int textWidth = u8g2.getUTF8Width(right.c_str());
        u8g2.drawUTF8(SCREEN_WIDTH - textWidth - SCROLLBAR_WIDTH - padX, baselineY, right.c_str());
    }
}

static void resolveListConfig(const ListConfig& config,
                              int& startY,
                              int& visibleItems,
                              int& lineHeight,
                              int& leftMargin) {
    const int fontSize = getSystemFontSize();
    const int padX = getHorizontalPaddingForFont(fontSize);
    const int selectorWidth = u8g2.getUTF8Width(">") + padX + 1;

    startY = config.startY >= 0 ? config.startY : getContentBaselineStart(fontSize);
    lineHeight = config.lineHeight > 0 ? config.lineHeight : getListLineHeight(fontSize);
    visibleItems = config.visibleItems > 0 ? config.visibleItems : max(1, getContentHeight(fontSize) / lineHeight);
    leftMargin = config.leftMargin >= 0 ? config.leftMargin : (config.showSelector ? selectorWidth : padX);
}

// ==========================================================================
// LIST & SCROLLING COMPONENTS
// ==========================================================================

void drawList(const char* const* items, int itemCount, int selectedIndex,
              int scrollOffset, const ListConfig& config) {
    setFontSystem();
    int startY = 0;
    int visibleItems = 0;
    int lineHeight = 0;
    int leftMargin = 0;
    resolveListConfig(config, startY, visibleItems, lineHeight, leftMargin);
    const int fontSize = getSystemFontSize();
    const int textMaxWidth = SCREEN_WIDTH - leftMargin - (config.showScrollbar ? SCROLLBAR_WIDTH + 4 : 2);
    
    for (int i = 0; i < visibleItems && (scrollOffset + i) < itemCount; i++) {
        int idx = scrollOffset + i;
        int y = startY + (i * lineHeight);
        
        if (config.showSelector && idx == selectedIndex) {
            u8g2.drawUTF8(0, y, ">");
        }
        
        if (idx == selectedIndex) {
            u8g2.drawBox(leftMargin - getHighlightPaddingX(fontSize),
                         getHighlightTop(y, fontSize),
                         SCREEN_WIDTH - leftMargin - (config.showScrollbar ? SCROLLBAR_WIDTH + 2 : 1),
                         getHighlightHeight(fontSize));
            u8g2.setDrawColor(0);
        }
        
        String itemText = truncateStringToWidth(String(items[idx] ? items[idx] : ""), max(1, textMaxWidth));
        u8g2.drawUTF8(leftMargin, y, itemText.c_str());
        u8g2.setDrawColor(1);
    }
    
    if (config.showScrollbar && itemCount > visibleItems) {
        int scrollHeight = visibleItems * lineHeight;
        drawScrollbar(SCREEN_WIDTH - SCROLLBAR_WIDTH - 1, 
                      getHighlightTop(startY, fontSize),
                      scrollHeight, 
                      itemCount, 
                      visibleItems, 
                      scrollOffset);
    }
}

void drawStringList(const String* items, int itemCount, int selectedIndex,
                    int scrollOffset, const ListConfig& config) {
    setFontSystem();
    int startY = 0;
    int visibleItems = 0;
    int lineHeight = 0;
    int leftMargin = 0;
    resolveListConfig(config, startY, visibleItems, lineHeight, leftMargin);
    const int fontSize = getSystemFontSize();
    const int textMaxWidth = SCREEN_WIDTH - leftMargin - (config.showScrollbar ? SCROLLBAR_WIDTH + 4 : 2);
    
    for (int i = 0; i < visibleItems && (scrollOffset + i) < itemCount; i++) {
        int idx = scrollOffset + i;
        int y = startY + (i * lineHeight);
        
        if (config.showSelector && idx == selectedIndex) {
            u8g2.drawUTF8(0, y, ">");
        }
        
        if (idx == selectedIndex) {
            u8g2.drawBox(leftMargin - getHighlightPaddingX(fontSize),
                         getHighlightTop(y, fontSize),
                         SCREEN_WIDTH - leftMargin - (config.showScrollbar ? SCROLLBAR_WIDTH + 2 : 1),
                         getHighlightHeight(fontSize));
            u8g2.setDrawColor(0);
        }
        
        String displayStr = truncateStringToWidth(items[idx], max(1, textMaxWidth));
        u8g2.drawUTF8(leftMargin, y, displayStr.c_str());
        u8g2.setDrawColor(1);
    }
    
    if (config.showScrollbar && itemCount > visibleItems) {
        int scrollHeight = visibleItems * lineHeight;
        drawScrollbar(SCREEN_WIDTH - SCROLLBAR_WIDTH - 1,
                      getHighlightTop(startY, fontSize),
                      scrollHeight,
                      itemCount,
                      visibleItems,
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
    const int fontSize = getSystemFontSize();
    setFontSystem();
    
    if (selected) {
        int w = (width > 0) ? width : u8g2.getUTF8Width(text) + (getHighlightPaddingX(fontSize) * 2);
        u8g2.drawBox(x - getHighlightPaddingX(fontSize),
                     getHighlightTop(y, fontSize),
                     w,
                     getHighlightHeight(fontSize));
        u8g2.setDrawColor(0);
    }
    
    u8g2.drawUTF8(x, y, text);
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
    const FontMetrics& metrics = getSystemFontMetrics();
    const int padX = getHighlightPaddingX() + 3;
    const int padY = 3;

    setFontSystem();
    int maxWidth = 0;
    for (int i = 0; i < itemCount; i++) {
        int w = u8g2.getUTF8Width(items[i]);
        if (w > maxWidth) maxWidth = w;
    }
    
    int menuWidth = maxWidth + (padX * 2);
    int menuHeight = itemCount * metrics.lineHeight + (padY * 2);

    drawPopupFrame(x, y, menuWidth, menuHeight, true);
    
    for (int i = 0; i < itemCount; i++) {
        int itemY = y + padY + metrics.baselineOffset + (i * metrics.lineHeight);
        drawSelectableText(x + padX, itemY, items[i], i == selectedIndex, menuWidth - (padX * 2));
    }
}

void drawYesNoDialog(const char* message, bool yesSelected) {
    const FontMetrics& metrics = getSystemFontMetrics();
    drawPopupFrame(10, 12, 108, 40, true);

    setFontSystem();
    String msg = truncateStringToWidth(String(message ? message : ""), 96);
    int msgWidth = u8g2.getUTF8Width(msg.c_str());
    int msgX = (SCREEN_WIDTH - msgWidth) / 2;
    int msgY = 12 + 6 + metrics.baselineOffset;
    u8g2.drawUTF8(msgX, msgY, msg.c_str());

    const int buttonY = 44;
    const int buttonWidth = 30;
    const int yesX = 25;
    const int noX = 73;

    if (yesSelected) {
        u8g2.drawBox(yesX, getHighlightTop(buttonY), buttonWidth, getHighlightHeight());
        u8g2.setDrawColor(0);
    }
    u8g2.drawUTF8(yesX + 6, buttonY, "YES");
    u8g2.setDrawColor(1);

    if (!yesSelected) {
        u8g2.drawBox(noX, getHighlightTop(buttonY), buttonWidth, getHighlightHeight());
        u8g2.setDrawColor(0);
    }
    u8g2.drawUTF8(noX + 9, buttonY, "NO");
    u8g2.setDrawColor(1);
}

void drawMessageDialog(const char* message, const char* buttonLabel, bool invertButton) {
    drawPopupFrame(10, 12, 108, 40, true);

    setFontSystem();
    String msg = truncateStringToWidth(String(message ? message : ""), 96);
    int msgWidth = u8g2.getUTF8Width(msg.c_str());
    int msgX = (SCREEN_WIDTH - msgWidth) / 2;
    int msgY = 12 + 6 + getSystemFontMetrics().baselineOffset;
    u8g2.drawUTF8(msgX, msgY, msg.c_str());

    const char* label = (buttonLabel != nullptr && buttonLabel[0] != '\0') ? buttonLabel : "OK";
    const int buttonY = 44;
    int buttonWidth = u8g2.getUTF8Width(label) + 12;
    if (buttonWidth < 30) {
        buttonWidth = 30;
    }
    const int buttonX = (SCREEN_WIDTH - buttonWidth) / 2;

    if (invertButton) {
        u8g2.drawBox(buttonX, getHighlightTop(buttonY), buttonWidth, getHighlightHeight());
        u8g2.setDrawColor(0);
    } else {
        u8g2.drawFrame(buttonX, getHighlightTop(buttonY), buttonWidth, getHighlightHeight());
        u8g2.setDrawColor(1);
    }
    int labelWidth = u8g2.getUTF8Width(label);
    int labelX = buttonX + (buttonWidth - labelWidth) / 2;
    u8g2.drawUTF8(labelX, buttonY, label);
    u8g2.setDrawColor(1);
}

void drawThreeOptionDialog(const char* message, const char* const labels[3], int selectedIndex) {
    drawPopupFrame(6, 10, 116, 44, true);

    setFontSystem();
    String msg = truncateStringToWidth(String(message ? message : ""), 104);
    int msgWidth = u8g2.getUTF8Width(msg.c_str());
    int msgX = (SCREEN_WIDTH - msgWidth) / 2;
    int msgY = 10 + 6 + getSystemFontMetrics().baselineOffset;
    u8g2.drawUTF8(msgX, msgY, msg.c_str());

    const int buttonY = 41;
    const int buttonWidth = 30;
    const int buttonXs[3] = {10, 48, 86};
    for (int i = 0; i < 3; i++) {
        const char* label = labels[i] ? labels[i] : "";
        if (selectedIndex == i) {
            u8g2.drawBox(buttonXs[i], getHighlightTop(buttonY), buttonWidth, getHighlightHeight());
            u8g2.setDrawColor(0);
        }
        int labelWidth = u8g2.getUTF8Width(label);
        int labelX = buttonXs[i] + (buttonWidth - labelWidth) / 2;
        u8g2.drawUTF8(labelX, buttonY, label);
        u8g2.setDrawColor(1);
    }
}

// ==========================================================================
// TOAST / TEMPORARY MESSAGES
// ==========================================================================

void showToast(const char* message, unsigned long durationMs) {
    toastMessage = message;
    toastEndTime = millis() + durationMs;
}

bool updateToast(int footerTopY) {
    if (toastMessage.length() == 0 || millis() > toastEndTime) {
        toastMessage = "";
        return false;
    }

    if (footerTopY < 0 || footerTopY > SCREEN_HEIGHT) {
        footerTopY = getFooterSeparatorY();
    }

    setFontSystem();
    const FontMetrics& metrics = getSystemFontMetrics();
    String message = truncateStringToWidth(toastMessage, SCREEN_WIDTH - 12);
    int msgWidth = u8g2.getUTF8Width(message.c_str());
    int boxWidth = msgWidth + 8;
    int boxX = (SCREEN_WIDTH - boxWidth) / 2;
    int boxHeight = metrics.boxHeight + 4;
    int boxY = footerTopY - boxHeight - 1;
    if (boxY < 0) {
        boxY = 0;
    }

    u8g2.setDrawColor(0);
    u8g2.drawBox(boxX, boxY, boxWidth, boxHeight);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(boxX, boxY, boxWidth, boxHeight);
    u8g2.drawUTF8(boxX + 4, boxY + metrics.baselineOffset + 2, message.c_str());
    
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
    setFontSystem();
    String titleText = truncateStringToWidth(String(title ? title : ""), SCREEN_WIDTH - 4);
    int titleWidth = u8g2.getUTF8Width(titleText.c_str());
    int titleY = getContentBaselineStart();
    u8g2.drawUTF8((SCREEN_WIDTH - titleWidth) / 2, titleY, titleText.c_str());
    
    setFontSecondary();
    char scoreStr[20];
    snprintf(scoreStr, sizeof(scoreStr), "Score: %d", score);
    int scoreWidth = u8g2.getUTF8Width(scoreStr);
    int scoreY = titleY + getSystemFontMetrics().lineHeight + getSecondaryFontMetrics().baselineOffset;
    u8g2.drawUTF8((SCREEN_WIDTH - scoreWidth) / 2, scoreY, scoreStr);
    
    String hint = truncateStringToWidth(String(restartHint ? restartHint : ""), SCREEN_WIDTH - 4);
    int hintWidth = u8g2.getUTF8Width(hint.c_str());
    u8g2.drawUTF8((SCREEN_WIDTH - hintWidth) / 2, getFooterBaselineY(), hint.c_str());
}

void drawScore(int x, int y, const char* label, int value) {
    setFontSystem();
    char str[32];
    snprintf(str, sizeof(str), "%s%d", label, value);
    u8g2.drawUTF8(x, y, str);
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
    if (key == KEY_UP) {
        navigateUp(state);
        return true;
    }
    if (key == KEY_DOWN) {
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

String truncateStringToWidth(const String& str, int maxWidth, const char* ellipsis) {
    if (maxWidth <= 0) {
        return "";
    }

    if (u8g2.getUTF8Width(str.c_str()) <= maxWidth) {
        return str;
    }

    String suffix = (ellipsis != nullptr) ? String(ellipsis) : String("...");
    if (u8g2.getUTF8Width(suffix.c_str()) > maxWidth) {
        return "";
    }

    String candidate = str;
    while (candidate.length() > 0) {
        candidate.remove(candidate.length() - 1);
        String withSuffix = candidate + suffix;
        if (u8g2.getUTF8Width(withSuffix.c_str()) <= maxWidth) {
            return withSuffix;
        }
    }

    return suffix;
}

int centerTextX(const char* text) {
    int textWidth = u8g2.getStrWidth(text);
    return (SCREEN_WIDTH - textWidth) / 2;
}

int getTextWidth(const char* text) {
    return u8g2.getStrWidth(text);
}

void setFontTiny() {
    setFontBySize(FONT_SIZE_TINY);
}

void setFontSmall() {
    setFontBySize(FONT_SIZE_SMALL);
}

void setFontMedium() {
    setFontBySize(FONT_SIZE_MEDIUM);
}

void setFontLarge() {
    setFontMedium();
}

} // namespace GUI
