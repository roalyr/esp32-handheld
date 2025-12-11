-- keytest.lua - Interactive key tester
-- Displays which keys are pressed

print("Key tester started")

local running = true
local lastKeys = {}

while running do
    input.scan()
    local keys = input.getKeys()
    
    gfx.clear()
    gfx.setFont(1)
    gfx.text(20, 12, "Key Tester")
    gfx.setFont(0)
    
    -- Show pressed keys
    if #keys > 0 then
        local keyStr = table.concat(keys, " ")
        gfx.text(10, 30, "Pressed: " .. keyStr)
    else
        gfx.text(10, 30, "Press any key...")
    end
    
    gfx.text(2, 55, "Hold * and # to exit")
    
    -- Draw key indicator boxes
    local startX = 10
    for i, key in ipairs(keys) do
        local kx = startX + (i-1) * 15
        gfx.fillRect(kx, 38, 12, 12)
        gfx.setColor(0)
        gfx.text(kx + 3, 47, key)
        gfx.setColor(1)
    end
    
    gfx.send()
    
    -- Exit condition: both * and # held
    local hasStar = false
    local hasHash = false
    for _, k in ipairs(keys) do
        if k == "*" then hasStar = true end
        if k == "#" then hasHash = true end
    end
    if hasStar and hasHash then
        running = false
    end
    
    sys.delay(50)
    sys.yield()
end

print("Key tester ended")
