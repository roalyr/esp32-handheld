// [Revision: v2.1] [Path: src/apps/snake.cpp] [Date: 2025-12-10]
// Description: Snake Game. 
// Changes: Converted to frame-counter timing (Fixed FPS) and standardized Game Over font.

#include "snake.h"

SnakeApp::SnakeApp() {
    snakeLen = 3;
}

void SnakeApp::start() {
    u8g2.setContrast(systemContrast);
    randomSeed(esp_random());
    resetGame();
}

void SnakeApp::stop() {}

void SnakeApp::resetGame() {
    snakeLen = 3;
    snake[0] = {10, 10};
    snake[1] = {9, 10};
    snake[2] = {8, 10};
  
    dirX = 1; 
    dirY = 0;
  
    gameOver = false;
    spawnFood();
    
    moveInterval = 2; 
    framesSinceMove = 0;
}

void SnakeApp::spawnFood() {
    food.x = random(1, BOARD_W - 1);
    food.y = random(1, BOARD_H - 1);
}

void SnakeApp::handleInput(char key) {
    if (gameOver) {
        if (key == '5') resetGame();
        return;
    }
    // Directional Input
    if (key == '2' && dirY == 0) { dirX = 0; dirY = -1; }
    if (key == '8' && dirY == 0) { dirX = 0; dirY = 1; }
    if (key == '4' && dirX == 0) { dirX = -1; dirY = 0; }
    if (key == '6' && dirX == 0) { dirX = 1; dirY = 0; }
}

void SnakeApp::update() {
    if (gameOver) return;
    
    // Frame-based timing
    framesSinceMove++;
    if (framesSinceMove < moveInterval) return;
    framesSinceMove = 0;

    // Shift Body
    for (int i = snakeLen - 1; i > 0; i--) {
        snake[i] = snake[i - 1];
    }
  
    // Move Head
    snake[0].x += dirX;
    snake[0].y += dirY;

    // Collision: Wall
    if (snake[0].x < 0 || snake[0].x >= BOARD_W || snake[0].y < 0 || snake[0].y >= BOARD_H) {
        gameOver = true;
    }

    // Collision: Self
    for (int i = 1; i < snakeLen; i++) {
        if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
            gameOver = true;
        }
    }

    // Eat Food
    if (snake[0].x == food.x && snake[0].y == food.y) {
        if (snakeLen < MAX_SNAKE_LEN) snakeLen++;
        spawnFood();
        // Speed up slightly every 5 apples
        if (snakeLen % 5 == 0 && moveInterval > 2) moveInterval--;
    }
}

void SnakeApp::render() {
    if (gameOver) {
        // Standardized Game Over Screen
        u8g2.setFont(u8g2_font_ncenB12_tr);
        u8g2.drawStr(8, 30, "GAME OVER");
        
        u8g2.setFont(FONT_SMALL);
        u8g2.drawStr(25, 50, "PRESS 5 TO RESTART");
        
        char score[15];
        sprintf(score, "Score: %d", snakeLen - 3);
        u8g2.drawStr(40, 60, score);
        return;
    }

    u8g2.drawFrame(0, 0, 128, 64);

    // Draw Food
    u8g2.drawBox(food.x * SNAKE_BLOCK_SIZE, food.y * SNAKE_BLOCK_SIZE, SNAKE_BLOCK_SIZE - 1, SNAKE_BLOCK_SIZE - 1);
  
    // Draw Snake
    for (int i = 0; i < snakeLen; i++) {
        if (i == 0) {
            u8g2.drawFrame(snake[i].x * SNAKE_BLOCK_SIZE, snake[i].y * SNAKE_BLOCK_SIZE, SNAKE_BLOCK_SIZE, SNAKE_BLOCK_SIZE);
        } else {
            u8g2.drawBox(snake[i].x * SNAKE_BLOCK_SIZE, snake[i].y * SNAKE_BLOCK_SIZE, SNAKE_BLOCK_SIZE - 1, SNAKE_BLOCK_SIZE - 1);
        }
    }
}