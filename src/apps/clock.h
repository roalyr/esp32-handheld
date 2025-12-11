// [Revision: v1.0] [Path: src/apps/clock.h] [Date: 2025-12-11]
// Description: Clock app for viewing and setting system time.

#ifndef APP_CLOCK_H
#define APP_CLOCK_H

#include "../app_interface.h"

class ClockApp : public App {
  private:
    // Edit mode
    bool editMode;
    int editField;  // 0=hours, 1=minutes, 2=seconds
    
    // Temp values for editing
    int editHours;
    int editMinutes;
    int editSeconds;
    
    // Blink state for cursor
    unsigned long lastBlinkTime;
    bool blinkOn;

  public:
    ClockApp();
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
};

#endif
