// [Revision: v1.0] [Path: emulator_mocks/U8g2lib.h] [Date: 2026-06-09]
// Description: U8g2 graphics library mock interface for the emulator.

#ifndef EMULATOR_U8G2LIB_H
#define EMULATOR_U8G2LIB_H

#include "Arduino.h"

#define U8G2_R0 0

#define U8G2_FONT_SECTION(name)

extern const uint8_t u8g2_font_4x6_tf[];
extern const uint8_t u8g2_font_5x7_tf[];
extern const uint8_t u8g2_font_5x8_tr[];
extern const uint8_t u8g2_font_5x7_t_cyrillic[];
extern const uint8_t u8g2_font_ncenB12_tr[];
extern const uint8_t u8g2_font_micro_tr[];
extern const uint8_t u8g2_font_inr24_t_cyrillic[];
extern const uint8_t u8g2_font_ncenB14_tr[];
extern const uint8_t u8g2_font_6x13_t_cyrillic[];

class U8G2 {
public:
    uint8_t buffer[128 * 64];
    const uint8_t* currentFont;
    int drawColor;
    int contrast;
    int fontMode; // 0: solid, 1: transparent

    U8G2();

    void begin() {}
    void setContrast(int c) { contrast = c; }
    void setFontMode(int mode) { fontMode = mode; }
    void setBitmapMode(int mode) {}
    void enableUTF8Print() {}
    void setFont(const uint8_t* font) { currentFont = font; }
    void setDrawColor(int color) { drawColor = color; }

    void clearBuffer() {
        memset(buffer, 0, sizeof(buffer));
    }

    void sendBuffer();

    void drawPixel(int x, int y) {
        if (x >= 0 && x < 128 && y >= 0 && y < 64) {
            buffer[y * 128 + x] = drawColor;
        }
    }

    void drawBox(int x, int y, int w, int h) {
        for (int currY = y; currY < y + h; ++currY) {
            for (int currX = x; currX < x + w; ++currX) {
                drawPixel(currX, currY);
            }
        }
    }

    void drawFrame(int x, int y, int w, int h) {
        for (int currX = x; currX < x + w; ++currX) {
            drawPixel(currX, y);
            drawPixel(currX, y + h - 1);
        }
        for (int currY = y; currY < y + h; ++currY) {
            drawPixel(x, currY);
            drawPixel(x + w - 1, currY);
        }
    }

    void drawVLine(int x, int y, int h) {
        for (int currY = y; currY < y + h; ++currY) {
            drawPixel(x, currY);
        }
    }

    void drawHLine(int x, int y, int w) {
        for (int currX = x; currX < x + w; ++currX) {
            drawPixel(currX, y);
        }
    }

    void drawLine(int x1, int y1, int x2, int y2) {
        int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
        int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
        int err = dx + dy, e2;
        while (true) {
            drawPixel(x1, y1);
            if (x1 == x2 && y1 == y2) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; x1 += sx; }
            if (e2 <= dx) { err += dx; y1 += sy; }
        }
    }

    void drawCircle(int x, int y, int r) {}
    void drawDisc(int x, int y, int r) {}
    int getDisplayWidth() { return 128; }
    int getDisplayHeight() { return 64; }

    void drawStr(int x, int y, const char* str);
    void drawUTF8(int x, int y, const char* str) { drawStr(x, y, str); }
    int getStrWidth(const char* str);
    int getUTF8Width(const char* str) { return getStrWidth(str); }
};

class U8G2_ST7920_128X64_F_SW_SPI : public U8G2 {
public:
    U8G2_ST7920_128X64_F_SW_SPI(int rotation, int clock, int data, int cs) : U8G2() {}
};

#endif // EMULATOR_U8G2LIB_H
