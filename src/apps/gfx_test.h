// [Revision: v3.1] [Path: src/apps/gfx_test.h] [Date: 2025-12-11]
// Description: GFX Test with contrast cycling and moving object for ghosting test.
//              Supports high-speed mode for accurate ghosting tests.

#ifndef APP_GFX_TEST_H
#define APP_GFX_TEST_H

#include "../app_interface.h"

class GfxTestApp : public App {
  private:
    // Contrast cycling
    int currentContrast;
    int contrastDir;
    unsigned long lastContrastTime;
    int contrastInterval;
    
    // Moving object for ghosting test
    int ballX;
    int ballY;
    int ballDirX;
    int ballDirY;
    unsigned long lastBallTime;
    int ballSpeed;  // 0=slow, 1=medium, 2=fast
    
    // Test mode
    int testMode;  // 0=contrast, 1=ghosting

  public:
    GfxTestApp();
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
    
    // High-speed mode for ghosting test (bypasses normal FPS limit)
    bool needsHighFps() { return testMode == 1; }
};

#endif