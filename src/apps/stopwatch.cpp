// [Revision: v1.0] [Path: src/apps/stopwatch.cpp] [Date: 2025-12-10]
// Description: Stopwatch logic.
// Controls: [5] Toggle Start/Stop, [0] Reset (when stopped).

#include "stopwatch.h"

StopwatchApp::StopwatchApp() {
    isRunning = false;
    accumulatedTime = 0;
    startTime = 0;
}

void StopwatchApp::start() {
    u8g2.setContrast(systemContrast);
    // Note: We do NOT reset time here. This allows switching apps 
    // while the stopwatch continues running in the background.
}

void StopwatchApp::stop() {
    // No cleanup needed. State is preserved.
}

unsigned long StopwatchApp::getTotalTime() {
    if (isRunning) {
        // Current time = Previously stored time + (Now - Start timestamp)
        return accumulatedTime + (millis() - startTime);
    } else {
        return accumulatedTime;
    }
}

void StopwatchApp::update() {
    // No physics to update. Time is calculated dynamically in render().
}

void StopwatchApp::handleInput(char key) {
    // 5 = PAUSE / RESUME
    if (key == '5') {
        if (isRunning) {
            // PAUSE: Save the elapsed time to the accumulator
            accumulatedTime += (millis() - startTime);
            isRunning = false;
        } else {
            // RESUME: Set new start timestamp
            startTime = millis();
            isRunning = true;
        }
    }
    
    // 0 = RESET (Only allowed when paused)
    if (key == '0' && !isRunning) {
        accumulatedTime = 0;
    }
}

void StopwatchApp::render() {
    u8g2.setFont(FONT_SMALL);
    u8g2.drawStr(2, 8, "STOPWATCH");
    u8g2.drawHLine(0, 10, 128);
    
    // 1. Calculate Time Components
    unsigned long totalMillis = getTotalTime();
    unsigned long totalSeconds = totalMillis / 1000;
    
    unsigned long hours = totalSeconds / 3600;
    unsigned long minutes = (totalSeconds % 3600) / 60;
    unsigned long seconds = totalSeconds % 60;
    
    // 2. Format String (HH:MM:SS)
    char buf[16];
    sprintf(buf, "%02lu:%02lu:%02lu", hours, minutes, seconds);
    
    // 3. Draw Large Time
    // Using B14 font to ensure "00:00:00" fits on 128px screen
    u8g2.setFont(u8g2_font_ncenB14_tr); 
    int width = u8g2.getStrWidth(buf);
    u8g2.drawStr((128 - width) / 2, 40, buf); 
    
    // 4. Draw Controls
    u8g2.setFont(u8g2_font_micro_tr); // Tiny font for hints
    if (isRunning) {
        u8g2.drawStr(40, 60, "[5] PAUSE");
    } else {
        u8g2.drawStr(15, 60, "[5] RESUME   [0] RESET");
    }
}