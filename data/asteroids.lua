-- asteroids.lua - Classic Asteroids Game
-- Controls: 4=Left, 6=Right, 2=Thrust, 5=Fire, 5=Restart(gameover)

-- Constants
local SCREEN_W, SCREEN_H = gfx.width(), gfx.height()
local DRAG = 0.98
local ACCEL = 0.15
local ROT_SPEED = 0.12
local MAX_VEL = 4.0
local PI = 3.14159

-- Game state
local shipX, shipY = SCREEN_W / 2, SCREEN_H / 2
local shipVX, shipVY = 0, 0
local shipAngle = -PI / 2
local isThrusting = false
local gameOver = false
local score = 0
local level = 1

-- Bullets
local bullets = {}
local MAX_BULLETS = 5

-- Asteroids
local asteroids = {}

-- Particles
local particles = {}

-- Helper: wrap position
local function wrap(val, max)
    if val < 0 then return val + max end
    if val > max then return val - max end
    return val
end

-- Helper: distance squared
local function distSq(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

-- Spawn asteroid
local function spawnAsteroid(x, y, size)
    local speed = (4 - size) * 0.5
    local angle = math.random() * 2 * PI
    table.insert(asteroids, {
        x = x, y = y,
        vx = math.cos(angle) * speed,
        vy = math.sin(angle) * speed,
        size = size,
        radius = size * 3 + 2,
        active = true
    })
end

-- Create explosion particles
local function explode(x, y, count)
    for i = 1, count do
        local angle = math.random() * 2 * PI
        local speed = math.random() * 2 + 0.5
        table.insert(particles, {
            x = x, y = y,
            vx = math.cos(angle) * speed,
            vy = math.sin(angle) * speed,
            life = math.random(10, 25),
            active = true
        })
    end
end

-- Reset game
local function resetGame()
    shipX, shipY = SCREEN_W / 2, SCREEN_H / 2
    shipVX, shipVY = 0, 0
    shipAngle = -PI / 2
    gameOver = false
    score = 0
    level = 1
    
    bullets = {}
    asteroids = {}
    particles = {}
    
    -- Spawn initial asteroids
    for i = 1, 3 do
        spawnAsteroid(math.random(0, SCREEN_W), math.random(0, SCREEN_H), 3)
    end
end

-- Fire bullet
local function fireBullet()
    if #bullets >= MAX_BULLETS then return end
    table.insert(bullets, {
        x = shipX,
        y = shipY,
        vx = shipVX + math.cos(shipAngle) * 5,
        vy = shipVY + math.sin(shipAngle) * 5,
        life = 50,
        active = true
    })
end

-- Handle input
local function handleInput()
    input.scan()
    
    if gameOver then
        if input.pressed("5") then
            resetGame()
        end
        return
    end
    
    -- Rotation
    if input.held("4") then shipAngle = shipAngle - ROT_SPEED end
    if input.held("6") then shipAngle = shipAngle + ROT_SPEED end
    
    -- Thrust
    isThrusting = input.held("2")
    if isThrusting then
        shipVX = shipVX + math.cos(shipAngle) * ACCEL
        shipVY = shipVY + math.sin(shipAngle) * ACCEL
    end
    
    -- Fire
    if input.pressed("5") then
        fireBullet()
    end
end

-- Update game
local function updateGame()
    if gameOver then return end
    
    -- Ship physics
    shipVX = shipVX * DRAG
    shipVY = shipVY * DRAG
    
    -- Speed cap
    local speed = math.sqrt(shipVX * shipVX + shipVY * shipVY)
    if speed > MAX_VEL then
        shipVX = (shipVX / speed) * MAX_VEL
        shipVY = (shipVY / speed) * MAX_VEL
    end
    
    shipX = wrap(shipX + shipVX, SCREEN_W)
    shipY = wrap(shipY + shipVY, SCREEN_H)
    
    -- Update bullets
    for i = #bullets, 1, -1 do
        local b = bullets[i]
        if b.active then
            b.x = wrap(b.x + b.vx, SCREEN_W)
            b.y = wrap(b.y + b.vy, SCREEN_H)
            b.life = b.life - 1
            if b.life <= 0 then
                table.remove(bullets, i)
            end
        end
    end
    
    -- Update particles
    for i = #particles, 1, -1 do
        local p = particles[i]
        if p.active then
            p.x = p.x + p.vx
            p.y = p.y + p.vy
            p.life = p.life - 1
            if p.life <= 0 then
                table.remove(particles, i)
            end
        end
    end
    
    -- Update asteroids and check collisions
    local levelCleared = true
    for i = #asteroids, 1, -1 do
        local a = asteroids[i]
        if a.active then
            levelCleared = false
            a.x = wrap(a.x + a.vx, SCREEN_W)
            a.y = wrap(a.y + a.vy, SCREEN_H)
            
            -- Ship collision
            if distSq(shipX, shipY, a.x, a.y) < (3 + a.radius) * (3 + a.radius) then
                explode(shipX, shipY, 10)
                gameOver = true
            end
            
            -- Bullet collisions
            for j = #bullets, 1, -1 do
                local b = bullets[j]
                if b.active and distSq(b.x, b.y, a.x, a.y) < a.radius * a.radius then
                    b.active = false
                    a.active = false
                    explode(a.x, a.y, 5)
                    score = score + math.floor(100 / a.size)
                    
                    -- Split asteroid
                    if a.size > 1 then
                        spawnAsteroid(a.x, a.y, a.size - 1)
                        spawnAsteroid(a.x, a.y, a.size - 1)
                    end
                    
                    table.remove(asteroids, i)
                    table.remove(bullets, j)
                    break
                end
            end
        end
    end
    
    -- Next level
    if levelCleared and not gameOver then
        level = level + 1
        shipX, shipY = SCREEN_W / 2, SCREEN_H / 2
        shipVX, shipVY = 0, 0
        for i = 1, level + 2 do
            spawnAsteroid(math.random(0, SCREEN_W), math.random(0, SCREEN_H), 3)
        end
    end
end

-- Draw ship
local function drawShip()
    local cos_a, sin_a = math.cos(shipAngle), math.sin(shipAngle)
    
    -- Ship triangle points
    local nose_x = shipX + cos_a * 5
    local nose_y = shipY + sin_a * 5
    local left_x = shipX + math.cos(shipAngle + 2.5) * 4
    local left_y = shipY + math.sin(shipAngle + 2.5) * 4
    local right_x = shipX + math.cos(shipAngle - 2.5) * 4
    local right_y = shipY + math.sin(shipAngle - 2.5) * 4
    
    gfx.line(math.floor(nose_x), math.floor(nose_y), math.floor(left_x), math.floor(left_y))
    gfx.line(math.floor(left_x), math.floor(left_y), math.floor(right_x), math.floor(right_y))
    gfx.line(math.floor(right_x), math.floor(right_y), math.floor(nose_x), math.floor(nose_y))
    
    -- Thrust flame
    if isThrusting then
        local flame_x = shipX - cos_a * 6
        local flame_y = shipY - sin_a * 6
        gfx.line(math.floor(shipX - cos_a * 3), math.floor(shipY - sin_a * 3),
                 math.floor(flame_x), math.floor(flame_y))
    end
end

-- Draw game
local function drawGame()
    gfx.clear()
    
    if gameOver then
        gfx.setFont(2)
        gfx.text(15, 25, "GAME OVER")
        gfx.setFont(0)
        gfx.text(35, 40, "Score: " .. score)
        gfx.text(20, 55, "Press 5 to restart")
    else
        -- Border
        gfx.rect(0, 0, SCREEN_W, SCREEN_H)
        
        -- Ship
        drawShip()
        
        -- Bullets
        for _, b in ipairs(bullets) do
            if b.active then
                gfx.fillCircle(math.floor(b.x), math.floor(b.y), 1)
            end
        end
        
        -- Particles
        for _, p in ipairs(particles) do
            if p.active then
                gfx.pixel(math.floor(p.x), math.floor(p.y))
            end
        end
        
        -- Asteroids
        for _, a in ipairs(asteroids) do
            if a.active then
                gfx.fillCircle(math.floor(a.x), math.floor(a.y), a.radius)
            end
        end
        
        -- Score
        gfx.setFont(0)
        gfx.text(2, 7, tostring(score))
    end
    
    gfx.send()
end

-- Main loop
print("Asteroids started - Press * or # to exit")
resetGame()

local running = true
while running do
    handleInput()
    updateGame()
    drawGame()
    
    if input.held("*") or input.held("#") then
        running = false
    end
    
    sys.delay(16)
    sys.yield()
end

print("Asteroids ended. Final score: " .. score)
