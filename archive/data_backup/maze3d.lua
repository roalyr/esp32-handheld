-- 3D Maze (Siemens C35 style)
-- Raycasting engine for 128x64 display
-- Controls: Up=Forward, Down=Back, Left/Right=Turn, ESC=Exit

-- Player position and direction
local px, py = 1.5, 1.5  -- Start position
local pa = 0             -- Player angle (radians)
local fov = 3.14159 / 3  -- 60 degree FOV

-- Movement speed
local moveSpeed = 0.15
local turnSpeed = 0.15

-- Screen dimensions
local SW, SH = 128, 64

-- Simple maze map (1 = wall, 0 = floor)
local map = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,1,0,1,1,1,1,0,1,1,1,0,1},
    {1,0,1,0,0,0,0,0,0,1,0,0,0,1,0,1},
    {1,0,1,0,1,1,1,1,0,1,1,1,0,1,0,1},
    {1,0,0,0,1,0,0,0,0,0,0,1,0,0,0,1},
    {1,1,1,0,1,0,1,1,1,1,0,1,0,1,1,1},
    {1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,1,1,1,0,1,1,1,1,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,1,1,0,1,1,0,1,1,1,1,0,1},
    {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},
    {1,1,1,1,0,1,1,1,1,0,1,0,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
}

local mapW, mapH = 16, 16

-- Check if position is wall
local function isWall(x, y)
    local mx = math.floor(x)
    local my = math.floor(y)
    if mx < 1 or mx > mapW or my < 1 or my > mapH then
        return true
    end
    return map[my][mx] == 1
end

-- Cast a single ray and return distance to wall
local function castRay(angle)
    local rayX, rayY = px, py
    local dx = math.cos(angle) * 0.05
    local dy = math.sin(angle) * 0.05
    local dist = 0
    local maxDist = 16
    
    while dist < maxDist do
        rayX = rayX + dx
        rayY = rayY + dy
        dist = dist + 0.05
        
        if isWall(rayX, rayY) then
            return dist
        end
    end
    
    return maxDist
end

-- Draw the 3D view
local function render()
    gfx.clear()
    
    -- Draw ceiling (top half - black already from clear)
    -- Draw floor (bottom half)
    gfx.setColor(1)
    
    -- Cast rays for each column
    local numRays = SW
    for i = 0, numRays - 1 do
        -- Calculate ray angle
        local rayAngle = pa - fov/2 + (i / numRays) * fov
        
        -- Cast ray
        local dist = castRay(rayAngle)
        
        -- Fix fisheye effect
        dist = dist * math.cos(rayAngle - pa)
        
        -- Calculate wall height
        local wallHeight = SH / dist
        if wallHeight > SH then wallHeight = SH end
        
        -- Calculate wall top and bottom
        local wallTop = (SH - wallHeight) / 2
        local wallBottom = wallTop + wallHeight
        
        -- Draw wall column (single pixel width)
        if dist < 8 then
            -- Close walls - solid line
            gfx.line(i, math.floor(wallTop), i, math.floor(wallBottom))
        elseif dist < 12 then
            -- Medium distance - dithered (every other pixel)
            for y = math.floor(wallTop), math.floor(wallBottom) do
                if (y + i) % 2 == 0 then
                    gfx.pixel(i, y)
                end
            end
        else
            -- Far walls - sparse dither
            for y = math.floor(wallTop), math.floor(wallBottom) do
                if (y + i) % 4 == 0 then
                    gfx.pixel(i, y)
                end
            end
        end
    end
    
    -- Draw minimap in corner
    local mapScale = 3
    local mapOffX, mapOffY = 2, 2
    for my = 1, 8 do
        for mx = 1, 8 do
            local wx = math.floor(px) - 4 + mx
            local wy = math.floor(py) - 4 + my
            if wx >= 1 and wx <= mapW and wy >= 1 and wy <= mapH then
                if map[wy][wx] == 1 then
                    gfx.fillRect(mapOffX + (mx-1)*mapScale, mapOffY + (my-1)*mapScale, mapScale-1, mapScale-1)
                end
            end
        end
    end
    -- Player dot on minimap
    gfx.fillRect(mapOffX + 4*mapScale - 1, mapOffY + 4*mapScale - 1, 2, 2)
    
    gfx.send()
end

-- Main game loop
local running = true

while running do
    input.scan()
    
    -- Turn left/right
    if input.held(input.KEY_LEFT) then
        pa = pa - turnSpeed
    end
    if input.held(input.KEY_RIGHT) then
        pa = pa + turnSpeed
    end
    
    -- Move forward/backward
    local newX, newY = px, py
    
    if input.held(input.KEY_UP) then
        newX = px + math.cos(pa) * moveSpeed
        newY = py + math.sin(pa) * moveSpeed
    end
    if input.held(input.KEY_DOWN) then
        newX = px - math.cos(pa) * moveSpeed
        newY = py - math.sin(pa) * moveSpeed
    end
    
    -- Collision detection
    if not isWall(newX, py) then px = newX end
    if not isWall(px, newY) then py = newY end
    
    -- Exit
    if input.pressed(input.KEY_ESC) then
        running = false
    end
    
    render()
    sys.delay(33)  -- ~30 FPS
end

-- Clear screen on exit
gfx.clear()
gfx.send()
