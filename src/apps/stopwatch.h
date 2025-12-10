// [Revision: v1.0] [Path: src/apps/stopwatch.h] [Date: 2025-12-10]
// Description: Stopwatch Application. 
// Features: Independent timekeeping (millis-based), Pause/Resume, Reset.

#ifndef APP_STOPWATCH_H
#define APP_STOPWATCH_H

#include "../app_interface.h"

class StopwatchApp : public App {
  private:
    bool isRunning;
    unsigned long startTime;       // Time stamp when "Start" was pressed
    unsigned long accumulatedTime; // Time stored from previous sessions (before pause)
    
    // Helper to calculate time to display
    unsigned long getTotalTime();

  public:
    StopwatchApp();
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
};

#endif