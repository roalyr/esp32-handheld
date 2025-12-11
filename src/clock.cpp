// [Revision: v1.0] [Path: src/clock.cpp] [Date: 2025-12-11]
// Description: Global system clock implementation.

#include "clock.h"

namespace SystemClock {

// Internal state
static unsigned long lastUpdateMillis = 0;
static int currentHours = 0;
static int currentMinutes = 0;
static int currentSeconds = 0;

void init() {
    lastUpdateMillis = millis();
    currentHours = 12;    // Default to 12:00:00
    currentMinutes = 0;
    currentSeconds = 0;
}

void setTime(int hours, int minutes, int seconds) {
    currentHours = hours % 24;
    currentMinutes = minutes % 60;
    currentSeconds = seconds % 60;
    lastUpdateMillis = millis();
}

// Internal helper to update time based on elapsed millis
static void updateTime() {
    unsigned long now = millis();
    unsigned long elapsed = now - lastUpdateMillis;
    
    // Add elapsed seconds
    if (elapsed >= 1000) {
        unsigned long elapsedSeconds = elapsed / 1000;
        lastUpdateMillis = now - (elapsed % 1000);  // Keep remainder
        
        currentSeconds += elapsedSeconds;
        
        // Handle overflow
        while (currentSeconds >= 60) {
            currentSeconds -= 60;
            currentMinutes++;
        }
        while (currentMinutes >= 60) {
            currentMinutes -= 60;
            currentHours++;
        }
        while (currentHours >= 24) {
            currentHours -= 24;
        }
    }
}

int getHours() {
    updateTime();
    return currentHours;
}

int getMinutes() {
    updateTime();
    return currentMinutes;
}

int getSeconds() {
    updateTime();
    return currentSeconds;
}

void getTimeString(char* buf, int bufSize) {
    updateTime();
    snprintf(buf, bufSize, "%02d:%02d", currentHours, currentMinutes);
}

void getFullTimeString(char* buf, int bufSize) {
    updateTime();
    snprintf(buf, bufSize, "%02d:%02d:%02d", currentHours, currentMinutes, currentSeconds);
}

} // namespace SystemClock
