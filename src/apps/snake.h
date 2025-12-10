// [Revision: v1.0] [Path: src/apps/snake.h] [Date: 2025-12-09]
// Description: Header for the Snake Game application.

#ifndef APP_SNAKE_H
#define APP_SNAKE_H

#include "../hal.h"

// Initialize game state, reset score, spawn first food
void setupSnake();

// Handle directional input (2/4/6/8) and restart (5)
void handleSnakeInput(char key);

// Main game loop logic (movement, collision, eating)
void updateSnake();

// Render game board and game over screen
void renderSnake();

#endif