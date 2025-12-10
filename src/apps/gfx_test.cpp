// [Revision: v2.2] [Path: src/apps/gfx_test.cpp] [Date: 2025-12-10]
// Description: GFX Test implementation. 
// Changes: Converted contrast cycle to non-blocking millis() timer.
//          Target speed: 0-255 sweep in ~1 second (Step delay ~4ms).

#include "gfx_test.h"

GfxTestApp::GfxTestApp() {
    currentContrast = 0;
    contrastDir = 1; // Step by 1 to cover all values
    lastStepTime = 0;
    
    // Calculate interval: 1000ms / 255 steps ~= 4ms
    stepInterval = 40; 
}

void GfxTestApp::start() {
    // Start test from the current system value
    currentContrast = systemContrast;
    contrastDir = 1;
    lastStepTime = millis();
}

void GfxTestApp::stop() {
    // Revert to global system preference when leaving the test
    u8g2.setContrast(systemContrast);
}

void GfxTestApp::handleInput(char key) {
    // No input handling
}

void GfxTestApp::update() {
    // Timer Check
    if (millis() - lastStepTime >= stepInterval) {
        lastStepTime = millis();

        // Update Contrast
        currentContrast += contrastDir;
        
        // Bounce logic
        if (currentContrast >= 50) {
            currentContrast = 50;
            contrastDir = -1; // Reverse direction
        } else if (currentContrast <= 0) {
            currentContrast = 0;
            contrastDir = 1;  // Reverse direction
        }
        
        // Apply immediately
        u8g2.setContrast(currentContrast);
    }
}

void GfxTestApp::render() {
    u8g2.setFont(FONT_SMALL);
    u8g2.drawStr(0, 8, "CONTRAST SWEEP");

    // Checkerboard
    for(int y=10; y<50; y+=2) { 
        for(int x=0; x<128; x+=2) {
            if ((x+y)%4 == 0) u8g2.drawPixel(x, y);
        }
    }
    
    // Geometry
    u8g2.drawFrame(10, 20, 108, 20);
    u8g2.drawBox(12, 22, 20, 16);
    u8g2.drawFrame(34, 22, 20, 16);
    for(int i=56; i<76; i+=2) u8g2.drawVLine(i, 22, 16); 
    
    // Status
    char stateBuf[32];
    sprintf(stateBuf, "VALUE: %d", currentContrast);
    u8g2.drawStr(0, 56, stateBuf);
    
    u8g2.drawStr(0, 64, "PRESS D TO EXIT");
}