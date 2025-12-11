-- bounce.lua - Bouncing ball demo
-- Shows game loop pattern with gfx and input APIs

local x, y = 64, 32
local vx, vy = 2, 1
local radius = 5
local running = true

print("Bounce demo started")
print("Press any key to exit")

-- Main loop - runs until user presses a key
local startTime = sys.millis()
local frameCount = 0

while running do
    -- Update physics
    x = x + vx
    y = y + vy
    
    -- Bounce off walls
    if x <= radius or x >= gfx.width() - radius then
        vx = -vx
    end
    if y <= radius or y >= gfx.height() - radius then
        vy = -vy
    end
    
    -- Clamp to bounds
    if x < radius then x = radius end
    if x > gfx.width() - radius then x = gfx.width() - radius end
    if y < radius then y = radius end
    if y > gfx.height() - radius then y = gfx.height() - radius end
    
    -- Draw
    gfx.clear()
    gfx.rect(0, 0, gfx.width(), gfx.height())  -- Border
    gfx.fillCircle(math.floor(x), math.floor(y), radius)
    
    -- FPS counter
    frameCount = frameCount + 1
    local elapsed = sys.millis() - startTime
    if elapsed > 0 then
        local fps = (frameCount * 1000) / elapsed
        gfx.setFont(0)
        gfx.text(2, 8, string.format("FPS: %.0f", fps))
    end
    
    gfx.send()
    
    -- Check for exit
    input.scan()
    if input.anyPressed() then
        running = false
    end
    
    -- Small delay to not hog CPU
    sys.delay(16)  -- ~60 FPS target
    sys.yield()
end

print("Bounce demo ended")
print("Frames: " .. frameCount)
