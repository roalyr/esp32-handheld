// [Revision: v1.0] [Path: emulator_mocks/U8g2lib.cpp] [Date: 2026-06-09]
// Description: U8g2 graphics implementation including Sinclair 5x7 font and console renderer.

#include "U8g2lib.h"
#include <iostream>

// Real fonts are now linked from u8g2_fonts.cpp
#define U8G2_FONT_DATA_STRUCT_SIZE 23

// Sinclair 5x7 standard ASCII font table (ASCII 32 to 126)
static const uint8_t font5x7[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // space
    {0x00, 0x00, 0x5f, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7f, 0x14, 0x7f, 0x14}, // #
    {0x24, 0x2a, 0x7f, 0x2a, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1c, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1c, 0x00}, // )
    {0x14, 0x08, 0x3e, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3e, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, // 0
    {0x00, 0x42, 0x7f, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4b, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7f, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1e}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3e}, // @
    {0x7e, 0x11, 0x11, 0x11, 0x7e}, // A
    {0x7f, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3e, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7f, 0x41, 0x41, 0x22, 0x1c}, // D
    {0x7f, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7f, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3e, 0x41, 0x49, 0x49, 0x7a}, // G
    {0x7f, 0x08, 0x08, 0x08, 0x7f}, // H
    {0x00, 0x41, 0x7f, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3f, 0x01}, // J
    {0x7f, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7f, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7f, 0x02, 0x0c, 0x02, 0x7f}, // M
    {0x7f, 0x04, 0x08, 0x10, 0x7f}, // N
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, // O
    {0x7f, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3e, 0x41, 0x51, 0x21, 0x5e}, // Q
    {0x7f, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7f, 0x01, 0x01}, // T
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, // U
    {0x1f, 0x20, 0x40, 0x20, 0x1f}, // V
    {0x3f, 0x40, 0x38, 0x40, 0x3f}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x7f, 0x41, 0x41, 0x00}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // \.
    {0x00, 0x41, 0x41, 0x7f, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7f, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7f}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7e, 0x09, 0x01, 0x02}, // f
    {0x0c, 0x52, 0x52, 0x52, 0x3e}, // g
    {0x7f, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7d, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3d, 0x00}, // j
    {0x7f, 0x10, 0x28, 0x44, 0x00}, // k
    {0x00, 0x41, 0x7f, 0x40, 0x00}, // l
    {0x7c, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7c, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7c, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7c}, // q
    {0x7c, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3f, 0x44, 0x40, 0x20}, // t
    {0x3c, 0x40, 0x40, 0x20, 0x7c}, // u
    {0x1c, 0x20, 0x40, 0x20, 0x1c}, // v
    {0x3c, 0x40, 0x30, 0x40, 0x3c}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0c, 0x50, 0x50, 0x50, 0x3c}, // y
    {0x44, 0x64, 0x54, 0x4c, 0x44}, // z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // {
    {0x00, 0x00, 0x7f, 0x00, 0x00}, // |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // }
    {0x10, 0x08, 0x18, 0x10, 0x08}, // ~
};

U8G2::U8G2() {
    clearBuffer();
    currentFont = nullptr;
    drawColor = 1;
    contrast = 200;
    fontMode = 1; // Default to transparent
}

// U8g2 font decoder implementation
struct u8g2_font_info_t {
    uint8_t glyph_cnt;
    uint8_t bbx_mode;
    uint8_t bits_per_0;
    uint8_t bits_per_1;
    uint8_t bits_per_char_width;
    uint8_t bits_per_char_height;
    uint8_t bits_per_char_x;
    uint8_t bits_per_char_y;
    uint8_t bits_per_delta_x;
    int8_t max_char_width;
    int8_t max_char_height;
    int8_t x_offset;
    int8_t y_offset;
    int8_t ascent_A;
    int8_t descent_g;
    int8_t ascent_para;
    int8_t descent_para;
    uint16_t start_pos_upper_A;
    uint16_t start_pos_lower_a;
    uint16_t start_pos_unicode;
};

struct u8g2_font_decode_t {
    const uint8_t *decode_ptr;
    int target_x;
    int target_y;
    int x;
    int y;
    int glyph_width;
    int glyph_height;
    uint8_t decode_bit_pos;
    uint8_t is_transparent;
    uint8_t fg_color;
    uint8_t bg_color;
};

static uint8_t u8g2_font_get_byte(const uint8_t *font, uint8_t offset) {
    return font[offset];
}

static uint16_t u8g2_font_get_word(const uint8_t *font, uint8_t offset) {
    uint16_t pos = font[offset];
    pos <<= 8;
    pos += font[offset + 1];
    return pos;
}

static void u8g2_read_font_info(u8g2_font_info_t *font_info, const uint8_t *font) {
    font_info->glyph_cnt = u8g2_font_get_byte(font, 0);
    font_info->bbx_mode = u8g2_font_get_byte(font, 1);
    font_info->bits_per_0 = u8g2_font_get_byte(font, 2);
    font_info->bits_per_1 = u8g2_font_get_byte(font, 3);
    font_info->bits_per_char_width = u8g2_font_get_byte(font, 4);
    font_info->bits_per_char_height = u8g2_font_get_byte(font, 5);
    font_info->bits_per_char_x = u8g2_font_get_byte(font, 6);
    font_info->bits_per_char_y = u8g2_font_get_byte(font, 7);
    font_info->bits_per_delta_x = u8g2_font_get_byte(font, 8);
    font_info->max_char_width = u8g2_font_get_byte(font, 9);
    font_info->max_char_height = u8g2_font_get_byte(font, 10);
    font_info->x_offset = u8g2_font_get_byte(font, 11);
    font_info->y_offset = u8g2_font_get_byte(font, 12);
    font_info->ascent_A = u8g2_font_get_byte(font, 13);
    font_info->descent_g = u8g2_font_get_byte(font, 14);
    font_info->ascent_para = u8g2_font_get_byte(font, 15);
    font_info->descent_para = u8g2_font_get_byte(font, 16);
    font_info->start_pos_upper_A = u8g2_font_get_word(font, 17);
    font_info->start_pos_lower_a = u8g2_font_get_word(font, 19);
    font_info->start_pos_unicode = u8g2_font_get_word(font, 21);
}

static uint8_t u8g2_font_decode_get_unsigned_bits(u8g2_font_decode_t *f, uint8_t cnt) {
    uint8_t val = *(f->decode_ptr);
    val >>= f->decode_bit_pos;
    uint8_t bit_pos_plus_cnt = f->decode_bit_pos + cnt;
    if (bit_pos_plus_cnt >= 8) {
        uint8_t s = 8 - f->decode_bit_pos;
        f->decode_ptr++;
        val |= *(f->decode_ptr) << s;
        bit_pos_plus_cnt -= 8;
    }
    val &= (1U << cnt) - 1;
    f->decode_bit_pos = bit_pos_plus_cnt;
    return val;
}

static int8_t u8g2_font_decode_get_signed_bits(u8g2_font_decode_t *f, uint8_t cnt) {
    int8_t v = (int8_t)u8g2_font_decode_get_unsigned_bits(f, cnt);
    int8_t d = 1;
    cnt--;
    d <<= cnt;
    v -= d;
    return v;
}

static const uint8_t *u8g2_font_get_glyph_data(const uint8_t *font, uint16_t encoding, const u8g2_font_info_t &font_info) {
    const uint8_t *ptr = font + U8G2_FONT_DATA_STRUCT_SIZE;
    if (encoding <= 255) {
        if (encoding >= 'a') {
            ptr += font_info.start_pos_lower_a;
        } else if (encoding >= 'A') {
            ptr += font_info.start_pos_upper_A;
        }
        for (;;) {
            if (ptr[1] == 0) break;
            if (ptr[0] == encoding) {
                return ptr + 2; // skip encoding and glyph size
            }
            ptr += ptr[1];
        }
    } else {
        ptr += font_info.start_pos_unicode;
        const uint8_t *unicode_lookup_table = ptr;
        do {
            ptr += u8g2_font_get_word(unicode_lookup_table, 0);
            uint16_t e = u8g2_font_get_word(unicode_lookup_table, 2);
            unicode_lookup_table += 4;
            if (e >= encoding) break;
        } while (true);

        for (;;) {
            uint16_t e = ptr[0];
            e <<= 8;
            e |= ptr[1];
            if (e == 0) break;
            if (e == encoding) {
                return ptr + 3; // skip encoding and glyph size
            }
            ptr += ptr[2];
        }
    }
    return nullptr;
}

static void u8g2_font_setup_decode(u8g2_font_decode_t *decode, const uint8_t *glyph_data, const u8g2_font_info_t &font_info, uint8_t draw_color) {
    decode->decode_ptr = glyph_data;
    decode->decode_bit_pos = 0;
    decode->glyph_width = u8g2_font_decode_get_unsigned_bits(decode, font_info.bits_per_char_width);
    decode->glyph_height = u8g2_font_decode_get_unsigned_bits(decode, font_info.bits_per_char_height);
    decode->fg_color = draw_color;
    decode->bg_color = (draw_color == 0 ? 1 : 0);
}

static void u8g2_font_decode_len(U8G2 *u8g2, u8g2_font_decode_t *decode, uint8_t len, uint8_t is_foreground) {
    uint8_t cnt = len;
    uint8_t lx = decode->x;
    uint8_t ly = decode->y;
    for (;;) {
        uint8_t rem = decode->glyph_width - lx;
        uint8_t current = cnt < rem ? cnt : rem;
        
        if (is_foreground) {
            for (int i = 0; i < current; ++i) {
                u8g2->drawPixel(decode->target_x + lx + i, decode->target_y + ly);
            }
        } else if (decode->is_transparent == 0) {
            int saved_color = u8g2->drawColor;
            u8g2->drawColor = decode->bg_color;
            for (int i = 0; i < current; ++i) {
                u8g2->drawPixel(decode->target_x + lx + i, decode->target_y + ly);
            }
            u8g2->drawColor = saved_color;
        }
        
        if (cnt < rem) break;
        cnt -= rem;
        lx = 0;
        ly++;
    }
    decode->x = lx + cnt;
    decode->y = ly;
}

static int8_t u8g2_font_decode_glyph(U8G2 *u8g2, u8g2_font_decode_t *decode, const uint8_t *glyph_data, const u8g2_font_info_t &font_info) {
    u8g2_font_setup_decode(decode, glyph_data, font_info, u8g2->drawColor);
    int h = decode->glyph_height;
    int8_t x = u8g2_font_decode_get_signed_bits(decode, font_info.bits_per_char_x);
    int8_t y = u8g2_font_decode_get_signed_bits(decode, font_info.bits_per_char_y);
    int8_t d = u8g2_font_decode_get_signed_bits(decode, font_info.bits_per_delta_x);
    
    if (decode->glyph_width > 0) {
        decode->target_x += x;
        decode->target_y -= h + y;
        decode->x = 0;
        decode->y = 0;
        for (;;) {
            uint8_t a = u8g2_font_decode_get_unsigned_bits(decode, font_info.bits_per_0);
            uint8_t b = u8g2_font_decode_get_unsigned_bits(decode, font_info.bits_per_1);
            do {
                u8g2_font_decode_len(u8g2, decode, a, 0);
                u8g2_font_decode_len(u8g2, decode, b, 1);
            } while (u8g2_font_decode_get_unsigned_bits(decode, 1) != 0);
            
            if (decode->y >= h) break;
        }
    }
    return d;
}

static uint16_t get_next_utf8_char(const char*& str) {
    uint8_t c = (uint8_t)*str++;
    if (c < 0x80) return c;
    if ((c & 0xe0) == 0xc0) {
        uint8_t c2 = (uint8_t)*str++;
        return ((c & 0x1f) << 6) | (c2 & 0x3f);
    }
    if ((c & 0xf0) == 0xe0) {
        uint8_t c2 = (uint8_t)*str++;
        uint8_t c3 = (uint8_t)*str++;
        return ((c & 0x0f) << 12) | ((c2 & 0x3f) << 6) | (c3 & 0x3f);
    }
    return c;
}

void U8G2::drawStr(int x, int y, const char* str) {
    if (!str) return;
    if (!currentFont) {
        currentFont = u8g2_font_5x7_tf; // fallback
    }
    
    u8g2_font_info_t font_info;
    u8g2_read_font_info(&font_info, currentFont);
    
    int curX = x;
    while (*str) {
        uint16_t encoding = get_next_utf8_char(str);
        const uint8_t *glyph_data = u8g2_font_get_glyph_data(currentFont, encoding, font_info);
        if (glyph_data) {
            u8g2_font_decode_t decode;
            decode.target_x = curX;
            decode.target_y = y;
            decode.is_transparent = fontMode;
            
            int8_t d = u8g2_font_decode_glyph(this, &decode, glyph_data, font_info);
            curX += d;
        } else {
            // Draw placeholder box
            drawFrame(curX, y - font_info.ascent_A, font_info.max_char_width, font_info.max_char_height);
            curX += font_info.max_char_width;
        }
    }
}

int U8G2::getStrWidth(const char* str) {
    if (!str) return 0;
    if (!currentFont) {
        currentFont = u8g2_font_5x7_tf; // fallback
    }
    
    u8g2_font_info_t font_info;
    u8g2_read_font_info(&font_info, currentFont);
    
    int total_width = 0;
    while (*str) {
        uint16_t encoding = get_next_utf8_char(str);
        const uint8_t *glyph_data = u8g2_font_get_glyph_data(currentFont, encoding, font_info);
        if (glyph_data) {
            u8g2_font_decode_t decode;
            u8g2_font_setup_decode(&decode, glyph_data, font_info, drawColor);
            u8g2_font_decode_get_signed_bits(&decode, font_info.bits_per_char_x);
            u8g2_font_decode_get_signed_bits(&decode, font_info.bits_per_char_y);
            int8_t d = u8g2_font_decode_get_signed_bits(&decode, font_info.bits_per_delta_x);
            total_width += d;
        }
    }
    return total_width;
}

void U8G2::sendBuffer() {
    // Print the frame buffer in-place on the terminal using Unicode half block character pairs
    std::cout << "\033[H"; // Cursor to home position
    std::cout << "+--------------------------------------------------------------------------------------------------------------------------------+\n";
    for (int y = 0; y < 64; y += 2) {
        std::cout << "|";
        for (int x = 0; x < 128; ++x) {
            int top = buffer[y * 128 + x];
            int bottom = buffer[(y + 1) * 128 + x];
            if (top && bottom) {
                std::cout << "█";
            } else if (top) {
                std::cout << "▀";
            } else if (bottom) {
                std::cout << "▄";
            } else {
                std::cout << " ";
            }
        }
        std::cout << "|\n";
    }
    std::cout << "+--------------------------------------------------------------------------------------------------------------------------------+\n";
    std::cout.flush();
}
