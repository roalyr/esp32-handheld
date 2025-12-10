// [Revision: v2.0] [Path: src/apps/snake.h] [Date: 2025-12-10]
// Description: Snake Game Application class definition.

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

    // Game State
    Point snake[MAX_SNAKE_LEN];
    int snakeLen;
    int dirX, dirY;
    Point food;
    bool gameOver;
    unsigned long lastMoveTime;
    int gameSpeed;

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