#ifndef APPS_H
#define APPS_H

#include "hal.h"
#include "t9_engine.h"

// --- EXISTING APPS ---
void renderT9Editor();
void renderKeyTester(char lastKey, char* heldKeys, int count);
void setupTester();
void addToTesterHistory(char c);

// --- SNAKE GAME ---
void setupSnake();
void handleSnakeInput(char key);
void updateSnake();
void renderSnake();

// --- GFX TESTER ---
void renderGfxTest();

#endif