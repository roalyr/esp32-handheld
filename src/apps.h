#ifndef APPS_H
#define APPS_H

#include "hal.h"
#include "t9_engine.h"

// --- EXISTING APPS ---
void renderT9Editor();
void renderKeyTester(char lastKey, char* heldKeys, int count);
void setupTester();
void addToTesterHistory(char c);

// --- GFX TESTER ---
void renderGfxTest();

#endif