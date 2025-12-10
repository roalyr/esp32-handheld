// [Revision: v1.0] [Path: src/apps/gfx_test.cpp] [Date: 2025-12-09]
// Description: Implementation of the Graphics Test screen.
// Notes: automatically cycles contrast 0-255 to test display biasing.

#include "gfx_test.h"

// --------------------------------------------------------------------------
// LOCAL STATE
// --------------------------------------------------------------------------

// Keep these static so they persist between frames but don't pollute global namespace
static int currentContrast = 0;
static int contrastDir = 5;

// --------------------------------------------------------------------------
// RENDER LOGIC
// --------------------------------------------------------------------------

void renderGfxTest() {
    // 1. Update Contrast Physics
    currentContrast += contrastDir;
    
    // Bounce logic
    if (currentContrast > 255) {
        currentContrast = 255;
        contrastDir = -5;
    } else if (currentContrast < 0) {
        currentContrast = 0;
        contrastDir = 5;
    }
    
    // Apply contrast immediately
    u8g2.setContrast(currentContrast);

    // 2. Text Elements
    u8g2.setFont(FONT_SMALL);
    u8g2.drawStr(0, 8, "CONTRAST CYCLE");

    // 3. Checkerboard Pattern (Test pixel clarity)
    // Optimization: Draw pixels in a strided loop
    for(int y=10; y<50; y+=2) { 
        for(int x=0; x<128; x+=2) {
            if ((x+y)%4 == 0) u8g2.drawPixel(x, y);
        }
    }
    
    // 4. Geometry Tests
    u8g2.drawFrame(10, 20, 108, 20); // Outer box
    u8g2.drawBox(12, 22, 20, 16);    // Filled box
    u8g2.drawFrame(34, 22, 20, 16);  // Empty box
    
    // Vertical lines test
    for(int i=56; i<76; i+=2) u8g2.drawVLine(i, 22, 16); 
    
    // 5. Status Display
    char stateBuf[32];
    sprintf(stateBuf, "CONTRAST: %d", currentContrast);
    u8g2.drawStr(0, 56, stateBuf);
    
    // Navigation Hint
    u8g2.drawStr(0, 64, "PRESS X/Y TO EXIT");
}