// [Revision: v2.0] [Path: src/apps/snake.cpp] [Date: 2025-12-10]
// Description: Implementation of Snake Game Application class.

#include "snake.h"

SnakeApp::SnakeApp() {
    gameSpeed = 150;
    snakeLen = 3;
}

void SnakeApp::start() {
    u8g2.setContrast(systemContrast);
    // Initialize Random Generator
    randomSeed(esp_random());
    resetGame();
}

void SnakeApp::stop() {
    // No specific cleanup needed
}

void SnakeApp::resetGame() {
    snakeLen = 3;
    // Start position
    snake[0] = {10, 10};
    snake[1] = {9, 10};
    snake[2] = {8, 10};
  
    // Default direction: Right
    dirX = 1; 
    dirY = 0;
  
    gameOver = false;
    spawnFood();
}

void SnakeApp::spawnFood() {
    // Random coordinates within board limits (1px buffer)
    food.x = random(1, BOARD_W - 1);
    food.y = random(1, BOARD_H - 1);
}

void SnakeApp::handleInput(char key) {
    if (gameOver) {
        if (key == '5') resetGame();
        return;
    }
  
    // Directional Input (prevent 180 degree turns)
    // '2' = UP
    if (key == '2' && dirY == 0) { dirX = 0; dirY = -1; }
    // '8' = DOWN
    if (key == '8' && dirY == 0) { dirX = 0; dirY = 1; }
    // '4' = LEFT
    if (key == '4' && dirX == 0) { dirX = -1; dirY = 0; }
    // '6' = RIGHT
    if (key == '6' && dirX == 0) { dirX = 1; dirY = 0; }
}

void SnakeApp::update() {
    if (gameOver) return;
    if (millis() - lastMoveTime < gameSpeed) return;
  
    lastMoveTime = millis();

    // Move body segments
    for (int i = snakeLen - 1; i > 0; i--) {
        snake[i] = snake[i - 1];
    }
  
    // Move head
    snake[0].x += dirX;
    snake[0].y += dirY;

    // 1. Wall Collision
    if (snake[0].x < 0 || snake[0].x >= BOARD_W || snake[0].y < 0 || snake[0].y >= BOARD_H) {
        gameOver = true;
    }

    // 2. Self Collision
    for (int i = 1; i < snakeLen; i++) {
        if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
            gameOver = true;
        }
    }

    // 3. Eat Food
    if (snake[0].x == food.x && snake[0].y == food.y) {
        if (snakeLen < MAX_SNAKE_LEN) snakeLen++;
        spawnFood();
    }
}

void SnakeApp::render() {
    if (gameOver) {
        u8g2.setFont(u8g2_font_ncenB14_tr);
        u8g2.drawStr(15, 30, "GAME OVER");
        u8g2.setFont(FONT_SMALL);
        u8g2.drawStr(25, 50, "PRESS 5 TO RESTART");
    
        // Draw Score
        u8g2.setDrawColor(1);
        char score[15];
        sprintf(score, "Score: %d", snakeLen - 3);
        u8g2.drawStr(40, 60, score);
        return;
    }

    // Draw Food
    u8g2.drawBox(food.x * SNAKE_BLOCK_SIZE, food.y * SNAKE_BLOCK_SIZE, SNAKE_BLOCK_SIZE - 1, SNAKE_BLOCK_SIZE - 1);
  
    // Draw Snake
    for (int i = 0; i < snakeLen; i++) {
        if (i == 0) {
            // Head is a Frame (Hollow)
            u8g2.drawFrame(snake[i].x * SNAKE_BLOCK_SIZE, snake[i].y * SNAKE_BLOCK_SIZE, SNAKE_BLOCK_SIZE, SNAKE_BLOCK_SIZE);
        } else {
            // Body is a Box (Filled)
            u8g2.drawBox(snake[i].x * SNAKE_BLOCK_SIZE, snake[i].y * SNAKE_BLOCK_SIZE, SNAKE_BLOCK_SIZE - 1, SNAKE_BLOCK_SIZE - 1);
        }
    }
}