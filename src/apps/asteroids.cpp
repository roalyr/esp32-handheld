// [Revision: v1.1] [Path: src/apps/asteroids.cpp] [Date: 2025-12-10]
// Description: Asteroids. Tuned for 60Hz loop. Standardized Game Over font.

#include "asteroids.h"

// Physics Constants (Tuned for 60 FPS)
const float DRAG = 0.985;      // Slightly less drag to maintain momentum
const float ACCEL = 0.15;      // Stronger thrust for responsiveness
const float ROT_SPEED = 0.12;  // Faster rotation
const float MAX_VEL = 4.0;     // Higher top speed
const int SCREEN_W = 128;
const int SCREEN_H = 64;

AsteroidsApp::AsteroidsApp() {
    bullets.resize(5);   
    particles.resize(20); 
}

void AsteroidsApp::start() {
    u8g2.setContrast(systemContrast);
    resetGame();
}

void AsteroidsApp::stop() {
    asteroids.clear();
}

void AsteroidsApp::resetGame() {
    score = 0;
    level = 1;
    gameOver = false;
    shipPos = {SCREEN_W / 2.0f, SCREEN_H / 2.0f};
    shipVel = {0, 0};
    shipAngle = -PI / 2; 
    
    asteroids.clear();
    for(int i=0; i<3; i++) {
        spawnAsteroid(random(0, SCREEN_W), random(0, SCREEN_H), 3);
    }
}

void AsteroidsApp::spawnAsteroid(float x, float y, int size) {
    Asteroid a;
    a.pos = {x, y};
    float speed = (4 - size) * 0.4; // Adjusted speed scale
    float angle = random(0, 628) / 100.0;
    a.vel = {cos(angle) * speed, sin(angle) * speed};
    a.size = size;
    a.radius = size * 3 + 2; 
    a.seed = random(0, 100);
    a.active = true;
    asteroids.push_back(a);
}

void AsteroidsApp::spawnBullet() {
    for(auto &b : bullets) {
        if(!b.active) {
            b.active = true;
            b.pos = shipPos;
            b.vel.x = shipVel.x + cos(shipAngle) * 5.0; // Faster bullets
            b.vel.y = shipVel.y + sin(shipAngle) * 5.0;
            b.life = 50; // Last slightly longer (approx 0.8s)
            break;
        }
    }
}

void AsteroidsApp::createExplosion(float x, float y, int count) {
    int spawned = 0;
    for(auto &p : particles) {
        if(!p.active && spawned < count) {
            p.active = true;
            p.pos = {x, y};
            float angle = random(0, 628) / 100.0;
            float speed = random(50, 250) / 100.0;
            p.vel = {cos(angle) * speed, sin(angle) * speed};
            p.life = random(15, 30); // ~0.5s at 60fps
            spawned++;
        }
    }
}

// --------------------------------------------------------------------------
// INPUT & UPDATE
// --------------------------------------------------------------------------

void AsteroidsApp::handleInput(char key) {
    if (gameOver && key == '5') {
        resetGame();
        return;
    }
    if (!gameOver && key == '5') {
        spawnBullet();
    }
}

void AsteroidsApp::update() {
    if (gameOver) return;

    // 1. POLL INPUT
    bool left = false, right = false, thrust = false;
    for(int i=0; i<activeKeyCount; i++) {
        if (activeKeys[i] == '4') left = true;
        if (activeKeys[i] == '6') right = true;
        if (activeKeys[i] == '2') thrust = true;
    }

    // 2. SHIP PHYSICS
    if (left) shipAngle -= ROT_SPEED;
    if (right) shipAngle += ROT_SPEED;
    
    isThrusting = thrust;
    if (thrust) {
        shipVel.x += cos(shipAngle) * ACCEL;
        shipVel.y += sin(shipAngle) * ACCEL;
    }
    
    // Drag
    shipVel.x *= DRAG;
    shipVel.y *= DRAG;
    
    // Cap Speed (Manual clamp to keep vector direction correct roughly)
    // Simple clamp per axis is okay for this retro feel, or normalize vector
    float speed = sqrt(shipVel.x*shipVel.x + shipVel.y*shipVel.y);
    if (speed > MAX_VEL) {
        shipVel.x = (shipVel.x / speed) * MAX_VEL;
        shipVel.y = (shipVel.y / speed) * MAX_VEL;
    }
    
    shipPos.x += shipVel.x;
    shipPos.y += shipVel.y;

    // Wrap
    if (shipPos.x < 0) shipPos.x += SCREEN_W;
    if (shipPos.x > SCREEN_W) shipPos.x -= SCREEN_W;
    if (shipPos.y < 0) shipPos.y += SCREEN_H;
    if (shipPos.y > SCREEN_H) shipPos.y -= SCREEN_H;

    // 3. BULLETS
    for(auto &b : bullets) {
        if (b.active) {
            b.pos.x += b.vel.x;
            b.pos.y += b.vel.y;
            b.life--;
            
            if (b.pos.x < 0) b.pos.x += SCREEN_W;
            if (b.pos.x > SCREEN_W) b.pos.x -= SCREEN_W;
            if (b.pos.y < 0) b.pos.y += SCREEN_H;
            if (b.pos.y > SCREEN_H) b.pos.y -= SCREEN_H;

            if (b.life <= 0) b.active = false;
        }
    }

    // 4. PARTICLES
    for(auto &p : particles) {
        if (p.active) {
            p.pos.x += p.vel.x;
            p.pos.y += p.vel.y;
            p.life--;
            if (p.life <= 0) p.active = false;
        }
    }

    // 5. ASTEROIDS & COLLISIONS
    bool levelCleared = true;
    
    for(auto &a : asteroids) {
        if (!a.active) continue;
        levelCleared = false;
        
        a.pos.x += a.vel.x;
        a.pos.y += a.vel.y;
        
        if (a.pos.x < 0) a.pos.x += SCREEN_W;
        if (a.pos.x > SCREEN_W) a.pos.x -= SCREEN_W;
        if (a.pos.y < 0) a.pos.y += SCREEN_H;
        if (a.pos.y > SCREEN_H) a.pos.y -= SCREEN_H;

        // Collision: Ship
        if (checkCollision(shipPos.x, shipPos.y, 3, a.pos.x, a.pos.y, a.radius)) {
            createExplosion(shipPos.x, shipPos.y, 10);
            gameOver = true;
        }

        // Collision: Bullets
        for(auto &b : bullets) {
            if (b.active && checkCollision(b.pos.x, b.pos.y, 1, a.pos.x, a.pos.y, a.radius)) {
                b.active = false;
                a.active = false;
                createExplosion(a.pos.x, a.pos.y, 5);
                score += 100 / a.size;
                if (a.size > 1) {
                    spawnAsteroid(a.pos.x, a.pos.y, a.size - 1);
                    spawnAsteroid(a.pos.x, a.pos.y, a.size - 1);
                }
                break;
            }
        }
    }

    if (levelCleared) {
        level++;
        shipPos = {SCREEN_W/2.0f, SCREEN_H/2.0f};
        shipVel = {0,0};
        for(int i=0; i<level+2; i++) {
            spawnAsteroid(random(0,SCREEN_W), random(0,SCREEN_H), 3);
        }
    }
}

bool AsteroidsApp::checkCollision(float x1, float y1, float r1, float x2, float y2, float r2) {
    float dx = x1 - x2;
    float dy = y1 - y2;
    float distSq = dx*dx + dy*dy;
    float rSum = r1 + r2;
    return distSq < (rSum * rSum);
}

void AsteroidsApp::drawWireframeModel(const float* coords, int numPoints, float x, float y, float angle, float scale) {
    float cosA = cos(angle);
    float sinA = sin(angle);
    
    int x0, y0, x1, y1;
    
    auto transform = [&](int idx, int &outX, int &outY) {
        float px = coords[idx*2] * scale;
        float py = coords[idx*2+1] * scale;
        outX = x + (px * cosA - py * sinA);
        outY = y + (px * sinA + py * cosA);
    };

    transform(0, x0, y0);
    for(int i=1; i<=numPoints; i++) {
        int idx = i % numPoints; 
        transform(idx, x1, y1);
        u8g2.drawLine(x0, y0, x1, y1);
        x0 = x1;
        y0 = y1;
    }
}

void AsteroidsApp::render() {
    if (gameOver) {
        // Standardized Game Over Screen
        u8g2.setFont(u8g2_font_ncenB12_tr);
        u8g2.drawStr(8, 30, "GAME OVER");
        
        char buf[20];
        sprintf(buf, "SCORE: %d", score);
        u8g2.setFont(FONT_SMALL);
        u8g2.drawStr(30, 50, buf);
        u8g2.drawStr(10, 62, "PRESS 5 TO RESTART");
        return;
    }

    // Ship
    const float shipModel[] = { 4,0, -3,-3, -2,0, -3,3 }; 
    drawWireframeModel(shipModel, 4, shipPos.x, shipPos.y, shipAngle, 1.0);
    
    // Thrust
    if (isThrusting) {
        u8g2.drawLine(
            shipPos.x - cos(shipAngle)*5, 
            shipPos.y - sin(shipAngle)*5,
            shipPos.x - cos(shipAngle)*2, 
            shipPos.y - sin(shipAngle)*2
        );
    }

    // Bullets
    for(auto &b : bullets) {
        if(b.active) u8g2.drawDisc(b.pos.x, b.pos.y, 2);
    }

    // Particles
    for(auto &p : particles) {
        if(p.active) u8g2.drawPixel(p.pos.x, p.pos.y);
    }

    // Asteroids
    for(auto &a : asteroids) {
        if(a.active) {
            u8g2.drawDisc(a.pos.x, a.pos.y, a.radius);
        }
    }

    // UI
    char scoreBuf[16];
    sprintf(scoreBuf, "%d", score);
    u8g2.setFont(u8g2_font_micro_tr); 
    u8g2.drawStr(2, 6, scoreBuf);
    u8g2.drawFrame(0, 0, 128, 64);

}