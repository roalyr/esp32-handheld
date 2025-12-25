// [Revision: v4.0] [Path: src/apps/gfx_test.cpp] [Date: 2025-12-11]
// Description: GFX Test - contrast cycling and moving object for ghosting test.

#include "gfx_test.h"
#include "../gui.h"

// Ball movement intervals (ms) for different speeds
static const int BALL_SPEEDS[] = {80, 40, 15};  // slow, medium, fast

GfxTestApp::GfxTestApp() {
    currentContrast = 0;
    contrastDir = 1;
    lastContrastTime = 0;
    contrastInterval = 40;
    
    ballX = 64;
    ballY = 32;
    ballDirX = 1;
    ballDirY = 1;
    lastBallTime = 0;
    ballSpeed = 1;  // Start medium
    
    testMode = 0;  // Start with contrast test
}

void GfxTestApp::start() {
    currentContrast = systemContrast;
    contrastDir = 1;
    lastContrastTime = millis();
    
    // Reset ball position
    ballX = 64;
    ballY = 28;
    ballDirX = 1;
    ballDirY = 1;
    lastBallTime = millis();
}

void GfxTestApp::stop() {
    // Revert to global system preference when leaving
    u8g2.setContrast(systemContrast);
}

void GfxTestApp::handleInput(char key) {
    // Switch modes with Enter
    if (key == KEY_ENTER) {
        testMode = (testMode + 1) % 2;
    }
    
    // In ghosting mode, UP/DOWN changes speed
    if (testMode == 1) {
        if (key == KEY_UP && ballSpeed > 0) {
            ballSpeed--;
        }
        if (key == KEY_DOWN && ballSpeed < 2) {
            ballSpeed++;
        }
    }
}

void GfxTestApp::update() {
    unsigned long now = millis();
    
    // Contrast cycling (only in contrast mode)
    if (testMode == 0 && now - lastContrastTime >= (unsigned long)contrastInterval) {
        lastContrastTime = now;
        currentContrast += contrastDir;
        
        if (currentContrast >= 50) {
            currentContrast = 50;
            contrastDir = -1;
        } else if (currentContrast <= 0) {
            currentContrast = 0;
            contrastDir = 1;
        }
        u8g2.setContrast(currentContrast);
    }
    
    // Ball movement (only in ghosting mode)
    if (testMode == 1 && now - lastBallTime >= (unsigned long)BALL_SPEEDS[ballSpeed]) {
        lastBallTime = now;
        
        ballX += ballDirX;
        ballY += ballDirY;
        
        // Bounce off walls (content area: y=12 to y=52 for header/footer)
        if (ballX <= 4 || ballX >= GUI::SCREEN_WIDTH - 8) {
            ballDirX = -ballDirX;
        }
        if (ballY <= GUI::HEADER_HEIGHT + 4 || ballY >= GUI::SCREEN_HEIGHT - GUI::FOOTER_HEIGHT - 8) {
            ballDirY = -ballDirY;
        }
    }
}

void GfxTestApp::render() {
    if (testMode == 0) {
        // CONTRAST TEST MODE
        char headerInfo[12];
        snprintf(headerInfo, sizeof(headerInfo), "C:%d", currentContrast);
        GUI::drawHeader("CONTRAST", headerInfo);
        
        // Checkerboard pattern in content area
        int contentTop = GUI::HEADER_HEIGHT + 2;
        int contentBottom = GUI::SCREEN_HEIGHT - GUI::FOOTER_HEIGHT - 2;
        
        for (int y = contentTop; y < contentBottom; y += 2) {
            for (int x = 0; x < GUI::SCREEN_WIDTH; x += 2) {
                if ((x + y) % 4 == 0) u8g2.drawPixel(x, y);
            }
        }
        
        // Test shapes
        int shapeY = contentTop + 4;
        u8g2.drawBox(4, shapeY, 16, 16);           // Solid box
        u8g2.drawFrame(24, shapeY, 16, 16);        // Frame
        for (int i = 0; i < 16; i += 2) {          // Vertical lines
            u8g2.drawVLine(44 + i, shapeY, 16);
        }
        for (int i = 0; i < 16; i += 2) {          // Horizontal lines
            u8g2.drawHLine(64, shapeY + i, 16);
        }
        // Gradient bar
        for (int x = 84; x < 124; x++) {
            if ((x - 84) % 4 < (x - 84) / 10) {
                u8g2.drawVLine(x, shapeY, 16);
            }
        }
        
        GUI::drawFooterHints("Enter:Mode", "Esc:Back");
        
    } else {
        // GHOSTING TEST MODE
        const char* speedNames[] = {"SLOW", "MED", "FAST"};
        GUI::drawHeader("GHOSTING", speedNames[ballSpeed]);
        
        // Draw moving ball (8x8 filled square for visibility)
        u8g2.drawBox(ballX, ballY, 6, 6);
        
        // Draw reference frame to show bounds
        int contentTop = GUI::HEADER_HEIGHT;
        int contentHeight = GUI::SCREEN_HEIGHT - GUI::HEADER_HEIGHT - GUI::FOOTER_HEIGHT;
        u8g2.drawFrame(0, contentTop, GUI::SCREEN_WIDTH, contentHeight);
        
        GUI::drawFooterHints("^v:Spd Enter:Mode", "Esc");
    }
}