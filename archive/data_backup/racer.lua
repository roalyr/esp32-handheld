-- Road Racer - Infinite branching roads
-- Classic top-down chase view racing
-- Controls: Left/Right=Steer, Down=Brake, Up=Accel, ESC=Exit

math.randomseed(sys.millis())

-- Screen
local SW, SH = 128, 64

-- Player car
local car = {
    x = 64,           -- Screen X position
    lane = 2,         -- Current lane (1-3)
    targetX = 64,     -- Target X for smooth movement
    speed = 2,        -- Current speed
    maxSpeed = 6,
    minSpeed = 1
}

-- Road parameters
local roadWidth = 80
local laneWidth = 26
local roadLeft = (SW - roadWidth) / 2
local roadRight = roadLeft + roadWidth

-- Road segments (scrolling)
local segments = {}
local segmentHeight = 8
local numSegments = 10

-- Road curve and branching
local roadOffset = 0       -- Current road center offset
local targetOffset = 0     -- Target offset for curves
local curveAmount = 0      -- How much road is curving

-- Score and distance
local distance = 0
local score = 0

-- Traffic (other cars)
local traffic = {}
local maxTraffic = 3

-- Game state
local gameOver = false
local running = true

-- Initialize road segments
local function initRoad()
    segments = {}
    for i = 1, numSegments do
        table.insert(segments, {
            y = SH - (i-1) * segmentHeight,
            offset = 0,
            hasDivider = true,
            branchLeft = false,
            branchRight = false
        })
    end
end

-- Spawn traffic car
local function spawnTraffic()
    if #traffic < maxTraffic and math.random() < 0.02 * car.speed then
        local lane = math.random(1, 3)
        local t = {
            x = roadLeft + roadOffset + (lane - 0.5) * laneWidth + laneWidth/2,
            y = -10,
            lane = lane,
            speed = car.speed * (0.5 + math.random() * 0.3),
            width = 8,
            height = 12
        }
        table.insert(traffic, t)
    end
end

-- Update road segments
local function updateRoad()
    -- Scroll segments
    for i, seg in ipairs(segments) do
        seg.y = seg.y + car.speed
    end
    
    -- Remove off-screen segments and add new ones
    while segments[1] and segments[1].y > SH + segmentHeight do
        table.remove(segments, 1)
        distance = distance + 1
        score = score + math.floor(car.speed)
        
        -- Occasionally change road direction
        if math.random() < 0.05 then
            targetOffset = (math.random() - 0.5) * 40
        end
        
        -- Add new segment at top
        local lastSeg = segments[#segments]
        local newOffset = lastSeg and lastSeg.offset or 0
        
        -- Smoothly curve toward target
        newOffset = newOffset + (targetOffset - newOffset) * 0.1
        
        -- Clamp offset so road stays on screen
        if roadLeft + newOffset < 5 then newOffset = 5 - roadLeft end
        if roadRight + newOffset > SW - 5 then newOffset = SW - 5 - roadRight end
        
        local newSeg = {
            y = (lastSeg and lastSeg.y or 0) - segmentHeight,
            offset = newOffset,
            hasDivider = math.random() > 0.1,
            branchLeft = math.random() < 0.03,
            branchRight = math.random() < 0.03
        }
        table.insert(segments, newSeg)
    end
    
    -- Update current road offset (for car positioning)
    for _, seg in ipairs(segments) do
        if seg.y > SH/2 and seg.y < SH/2 + segmentHeight then
            roadOffset = seg.offset
            break
        end
    end
end

-- Update traffic
local function updateTraffic()
    for i = #traffic, 1, -1 do
        local t = traffic[i]
        t.y = t.y + (car.speed - t.speed)
        
        -- Update X based on road curve
        t.x = roadLeft + roadOffset + (t.lane - 0.5) * laneWidth + laneWidth/2
        
        -- Remove if off screen
        if t.y > SH + 20 or t.y < -30 then
            table.remove(traffic, i)
        end
    end
end

-- Check collision
local function checkCollision()
    local carW, carH = 10, 14
    local carLeft = car.x - carW/2
    local carRight = car.x + carW/2
    local carTop = SH - 20
    local carBottom = carTop + carH
    
    -- Check road boundaries
    local currentRoadLeft = roadLeft + roadOffset + 2
    local currentRoadRight = roadRight + roadOffset - 2
    
    if car.x < currentRoadLeft or car.x > currentRoadRight then
        return true  -- Off road
    end
    
    -- Check traffic collision
    for _, t in ipairs(traffic) do
        local tLeft = t.x - t.width/2
        local tRight = t.x + t.width/2
        local tTop = t.y
        local tBottom = t.y + t.height
        
        if carRight > tLeft and carLeft < tRight and
           carBottom > tTop and carTop < tBottom then
            return true
        end
    end
    
    return false
end

-- Draw car sprite
local function drawCar(x, y, isPlayer)
    local w, h = 10, 14
    x = math.floor(x - w/2)
    y = math.floor(y)
    
    if isPlayer then
        -- Player car (more detailed)
        gfx.fillRect(x + 2, y, 6, h)        -- Body
        gfx.fillRect(x, y + 2, w, 4)        -- Windshield area
        gfx.fillRect(x, y + 9, w, 3)        -- Rear
        gfx.setColor(0)
        gfx.fillRect(x + 3, y + 3, 4, 2)    -- Windshield (black)
        gfx.setColor(1)
    else
        -- Traffic car (simpler)
        gfx.fillRect(x + 1, y, 8, h - 2)
        gfx.fillRect(x, y + 2, w, 3)
    end
end

-- Render game
local function render()
    gfx.clear()
    
    -- Draw road segments
    for _, seg in ipairs(segments) do
        local left = roadLeft + seg.offset
        local right = roadRight + seg.offset
        local y = math.floor(seg.y)
        
        if y >= -segmentHeight and y <= SH then
            -- Road edges
            gfx.line(left, y, left, y + segmentHeight)
            gfx.line(right, y, right, y + segmentHeight)
            
            -- Lane dividers (dashed)
            if seg.hasDivider then
                local lane1 = left + laneWidth
                local lane2 = left + laneWidth * 2
                gfx.line(lane1, y + 2, lane1, y + segmentHeight - 2)
                gfx.line(lane2, y + 2, lane2, y + segmentHeight - 2)
            end
            
            -- Branch indicators
            if seg.branchLeft then
                gfx.line(left, y + segmentHeight/2, left - 10, y)
            end
            if seg.branchRight then
                gfx.line(right, y + segmentHeight/2, right + 10, y)
            end
        end
    end
    
    -- Draw traffic
    for _, t in ipairs(traffic) do
        if t.y > -15 and t.y < SH then
            drawCar(t.x, t.y, false)
        end
    end
    
    -- Draw player car
    car.x = car.x + (car.targetX - car.x) * 0.3
    drawCar(car.x, SH - 20, true)
    
    -- HUD
    gfx.setFont(0)
    gfx.text(2, 7, string.format("%04d", score))
    gfx.text(SW - 25, 7, string.format("S:%d", math.floor(car.speed)))
    
    -- Speed bar
    local barW = (car.speed / car.maxSpeed) * 20
    gfx.rect(SW - 25, 10, 22, 5)
    gfx.fillRect(SW - 24, 11, math.floor(barW), 3)
    
    gfx.send()
end

-- Render game over screen
local function renderGameOver()
    gfx.clear()
    
    gfx.setFont(1)
    gfx.text(30, 20, "GAME OVER")
    
    gfx.setFont(0)
    gfx.text(35, 35, "Score: " .. score)
    gfx.text(25, 48, "Dist: " .. distance .. "m")
    gfx.text(10, 60, "Press Enter to restart")
    
    gfx.send()
end

-- Initialize
initRoad()

-- Main game loop
while running do
    input.scan()
    
    -- Exit
    if input.pressed(input.KEY_ESC) then
        running = false
    end
    
    if not gameOver then
        -- Controls
        if input.held(input.KEY_LEFT) then
            car.lane = car.lane - 0.08
            if car.lane < 1 then car.lane = 1 end
        end
        if input.held(input.KEY_RIGHT) then
            car.lane = car.lane + 0.08
            if car.lane > 3 then car.lane = 3 end
        end
        if input.held(input.KEY_UP) then
            car.speed = car.speed + 0.1
            if car.speed > car.maxSpeed then car.speed = car.maxSpeed end
        end
        if input.held(input.KEY_DOWN) then
            car.speed = car.speed - 0.15
            if car.speed < car.minSpeed then car.speed = car.minSpeed end
        end
        
        -- Update target X based on lane and road offset
        car.targetX = roadLeft + roadOffset + (car.lane - 0.5) * laneWidth + laneWidth/2
        
        -- Update game
        updateRoad()
        updateTraffic()
        spawnTraffic()
        
        -- Collision check
        if checkCollision() then
            gameOver = true
        end
        
        render()
    else
        -- Game over state
        renderGameOver()
        
        if input.pressed(input.KEY_ENTER) then
            -- Restart
            gameOver = false
            car.x = 64
            car.lane = 2
            car.speed = 2
            score = 0
            distance = 0
            roadOffset = 0
            targetOffset = 0
            traffic = {}
            initRoad()
        end
    end
    
    sys.delay(33)  -- ~30 FPS
end

gfx.clear()
gfx.send()
