// [Revision: v1.0] [Path: src/apps/asteroids.h] [Date: 2025-12-10]
// Description: Asteroids Arcade Game.
// features: Vector physics, screen wrapping, collision detection, particle system.

#ifndef APP_ASTEROIDS_H
#define APP_ASTEROIDS_H

#include "../app_interface.h"
#include <vector>

struct Vec2 {
    float x, y;
};

struct Bullet {
    Vec2 pos;
    Vec2 vel;
    int life;     // Frames remaining
    bool active;
};

struct Asteroid {
    Vec2 pos;
    Vec2 vel;
    int size;     // 3=Large, 2=Medium, 1=Small
    int radius;
    float seed;   // Random seed for jagged shape generation
    bool active;
};

struct Particle {
    Vec2 pos;
    Vec2 vel;
    int life;
    bool active;
};

class AsteroidsApp : public App {
  private:
    // Game Objects
    Vec2 shipPos;
    Vec2 shipVel;
    float shipAngle; // Radians
    bool isThrusting;
    
    // Object Pools
    std::vector<Bullet> bullets;
    std::vector<Asteroid> asteroids;
    std::vector<Particle> particles;

    // Game State
    int score;
    bool gameOver;
    int level;
    unsigned long lastShotTime;

    // Helpers
    void spawnAsteroid(float x, float y, int size);
    void spawnBullet();
    void createExplosion(float x, float y, int count);
    void resetGame();
    void nextLevel();
    bool checkCollision(float x1, float y1, float r1, float x2, float y2, float r2);
    void drawWireframeModel(const float* coords, int numPoints, float x, float y, float angle, float scale);

  public:
    AsteroidsApp();
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
};

#endif