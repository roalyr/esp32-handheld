-- snake.lua - Classic Snake Game
-- Controls: Arrows=Move, Enter=Restart

-- Constants
local BLOCK = 4
local BOARD_W = math.floor(gfx.width() / BLOCK)
local BOARD_H = math.floor(gfx.height() / BLOCK)
local MAX_LEN = 100

-- Game state
local snake = {}
local snakeLen = 3
local dirX, dirY = 1, 0
local food = {x = 0, y = 0}
local gameOver = false
local score = 0
local moveInterval = 3
local frameCount = 0

-- Initialize snake
local function resetGame()
    snake = {}
    for i = 1, MAX_LEN do
        snake[i] = {x = 0, y = 0}
    end
    snake[1] = {x = 10, y = 8}
    snake[2] = {x = 9, y = 8}
    snake[3] = {x = 8, y = 8}
    snakeLen = 3
    dirX, dirY = 1, 0
    gameOver = false
    score = 0
    moveInterval = 3
    
    -- Spawn food
    food.x = math.random(1, BOARD_W - 2)
    food.y = math.random(1, BOARD_H - 2)
end

local function spawnFood()
    food.x = math.random(1, BOARD_W - 2)
    food.y = math.random(1, BOARD_H - 2)
end

local function handleInput()
    input.scan()
    
    if gameOver then
        if input.pressed(input.KEY_ENTER) then
            resetGame()
        end
        return
    end
    
    -- Direction changes (prevent 180 degree turns)
    if input.pressed(input.KEY_UP) and dirY == 0 then dirX, dirY = 0, -1 end
    if input.pressed(input.KEY_DOWN) and dirY == 0 then dirX, dirY = 0, 1 end
    if input.pressed(input.KEY_LEFT) and dirX == 0 then dirX, dirY = -1, 0 end
    if input.pressed(input.KEY_RIGHT) and dirX == 0 then dirX, dirY = 1, 0 end
end

local function updateGame()
    if gameOver then return end
    
    frameCount = frameCount + 1
    if frameCount < moveInterval then return end
    frameCount = 0
    
    -- Shift body
    for i = snakeLen, 2, -1 do
        snake[i].x = snake[i-1].x
        snake[i].y = snake[i-1].y
    end
    
    -- Move head
    snake[1].x = snake[1].x + dirX
    snake[1].y = snake[1].y + dirY
    
    -- Wall collision
    if snake[1].x < 0 or snake[1].x >= BOARD_W or
       snake[1].y < 0 or snake[1].y >= BOARD_H then
        gameOver = true
        return
    end
    
    -- Self collision
    for i = 2, snakeLen do
        if snake[1].x == snake[i].x and snake[1].y == snake[i].y then
            gameOver = true
            return
        end
    end
    
    -- Eat food
    if snake[1].x == food.x and snake[1].y == food.y then
        if snakeLen < MAX_LEN then
            snakeLen = snakeLen + 1
            snake[snakeLen] = {x = snake[snakeLen-1].x, y = snake[snakeLen-1].y}
        end
        score = score + 10
        spawnFood()
        -- Speed up every 5 apples
        if score % 50 == 0 and moveInterval > 1 then
            moveInterval = moveInterval - 1
        end
    end
end

local function drawGame()
    gfx.clear()
    
    if gameOver then
        -- Game over screen
        gfx.setFont(2)
        gfx.text(15, 25, "GAME OVER")
        gfx.setFont(0)
        gfx.text(35, 40, "Score: " .. score)
        gfx.text(15, 55, "Press Enter restart")
    else
        -- Border
        gfx.rect(0, 0, gfx.width(), gfx.height())
        
        -- Food
        gfx.fillRect(food.x * BLOCK, food.y * BLOCK, BLOCK - 1, BLOCK - 1)
        
        -- Snake
        for i = 1, snakeLen do
            local x = snake[i].x * BLOCK
            local y = snake[i].y * BLOCK
            if i == 1 then
                -- Head as outline
                gfx.rect(x, y, BLOCK, BLOCK)
            else
                -- Body as filled
                gfx.fillRect(x, y, BLOCK - 1, BLOCK - 1)
            end
        end
        
        -- Score (small, top-left)
        gfx.setFont(0)
        gfx.text(2, 7, tostring(score))
    end
    
    gfx.send()
end

-- Main game loop
print("Snake started - Hold ESC to exit")
resetGame()

local running = true
while running do
    handleInput()
    updateGame()
    drawGame()
    
    -- Check for exit
    if input.held(input.KEY_ESC) then
        running = false
    end
    
    sys.delay(16)  -- ~60 FPS
    sys.yield()
end

print("Snake ended. Final score: " .. score)
