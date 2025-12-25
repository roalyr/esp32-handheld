// [Revision: v2.0] [Path: src/apps/stopwatch.cpp] [Date: 2025-12-11]
// Description: Stopwatch app - refactored to use unified GUI module.
// Controls: [ENTER] Toggle Start/Stop, [0] Reset (when stopped).

#include "stopwatch.h"
#include "../gui.h"

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
        return accumulatedTime + (millis() - startTime);
    } else {
        return accumulatedTime;
    }
}

void StopwatchApp::update() {
    // No physics to update. Time is calculated dynamically in render().
}

void StopwatchApp::handleInput(char key) {
    if (key == KEY_ENTER) {
        if (isRunning) {
            // PAUSE: Save the elapsed time
            accumulatedTime += (millis() - startTime);
            isRunning = false;
        } else {
            // RESUME: Set new start timestamp
            startTime = millis();
            isRunning = true;
        }
    }
    
    // RESET (Only allowed when paused)
    if (key == '0' && !isRunning) {
        accumulatedTime = 0;
    }
}

void StopwatchApp::render() {
    GUI::drawHeader("STOPWATCH");
    
    // Calculate Time Components
    unsigned long totalMillis = getTotalTime();
    unsigned long totalSeconds = totalMillis / 1000;
    
    unsigned long hours = totalSeconds / 3600;
    unsigned long minutes = (totalSeconds % 3600) / 60;
    unsigned long seconds = totalSeconds % 60;
    
    // Format String (HH:MM:SS)
    char buf[16];
    sprintf(buf, "%02lu:%02lu:%02lu", hours, minutes, seconds);
    
    // Draw Large Time (centered)
    u8g2.setFont(u8g2_font_ncenB14_tr); 
    int width = u8g2.getStrWidth(buf);
    u8g2.drawStr((GUI::SCREEN_WIDTH - width) / 2, 40, buf); 
    
    // Draw Controls
    if (isRunning) {
        GUI::drawFooter("[Enter] PAUSE");
    } else {
        GUI::drawFooterHints("[Enter] GO", "[0] RESET");
    }
}