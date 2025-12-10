// [Revision: v1.0] [Path: src/apps/key_tester.h] [Date: 2025-12-09]
// Description: Header for the Key Matrix Tester application.

#ifndef APP_KEY_TESTER_H
#define APP_KEY_TESTER_H

#include "../hal.h"

// Initialize tester state (clear history)
void setupTester();

// Add a key to the scrolling history buffer
void addToTesterHistory(char c);

// Render the debug interface showing history, last pressed key, and currently held keys
void renderKeyTester(char lastKey, char* heldKeys, int count);

#endif