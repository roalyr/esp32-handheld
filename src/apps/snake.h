// [Revision: v2.1] [Path: src/apps/snake.h] [Date: 2025-12-10]
// Description: Updated for frame-counter timing.

#ifndef APP_SNAKE_H
#define APP_SNAKE_H

#include "../app_interface.h"

struct Point { 
    int x, y;
};

class SnakeApp : public App {
  private:
    static const int MAX_SNAKE_LEN = 100;
    static const int SNAKE_BLOCK_SIZE = 4;
    static const int BOARD_W = 128 / SNAKE_BLOCK_SIZE;
    static const int BOARD_H = 64 / SNAKE_BLOCK_SIZE;

    Point snake[MAX_SNAKE_LEN];
    int snakeLen;
    int dirX, dirY;
    Point food;
    bool gameOver;
    
    // New Timing Logic
    int moveInterval;    // Frames between moves
    int framesSinceMove; // Counter

    void spawnFood();
    void resetGame();

  public:
    SnakeApp();
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
};

#endif