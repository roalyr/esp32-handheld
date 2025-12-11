// [Revision: v1.0] [Path: src/clock.h] [Date: 2025-12-11]
// Description: Global system clock - tracks time using millis() with manual set capability.

#ifndef CLOCK_H
#define CLOCK_H

#include <Arduino.h>

namespace SystemClock {

// Initialize the clock (call in setup)
void init();

// Set the time (hours 0-23, minutes 0-59, seconds 0-59)
void setTime(int hours, int minutes, int seconds);

// Get current time components
int getHours();
int getMinutes();
int getSeconds();

// Get formatted time string (HH:MM format for header)
void getTimeString(char* buf, int bufSize);

// Get formatted time string with seconds (HH:MM:SS)
void getFullTimeString(char* buf, int bufSize);

} // namespace SystemClock

#endif
