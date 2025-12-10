// [Revision: v2.1] [Path: src/apps/gfx_test.h] [Date: 2025-12-10]
// Description: Header for GFX Test. Added timing variables for contrast control.

#ifndef APP_GFX_TEST_H
#define APP_GFX_TEST_H

#include "../app_interface.h"

class GfxTestApp : public App {
  private:
    int currentContrast;
    int contrastDir;
    
    // Timing Control
    unsigned long lastStepTime;
    int stepInterval; // ms per contrast step

  public:
    GfxTestApp();
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
};

#endif