-- keytest.lua - Interactive key tester
-- Displays which keys are pressed

print("Key tester started")

local running = true
local lastKeys = {}

-- Helper to get readable key name
local function getKeyName(k)
    if k == input.KEY_ESC then return "ESC"
    elseif k == input.KEY_BKSP then return "BKSP"
    elseif k == input.KEY_TAB then return "TAB"
    elseif k == input.KEY_ENTER then return "ENT"
    elseif k == input.KEY_SHIFT then return "SHF"
    elseif k == input.KEY_ALT then return "ALT"
    elseif k == input.KEY_UP then return "UP"
    elseif k == input.KEY_DOWN then return "DN"
    elseif k == input.KEY_LEFT then return "LT"
    elseif k == input.KEY_RIGHT then return "RT"
    else return k
    end
end

while running do
    input.scan()
    local keys = input.getKeys()
    
    gfx.clear()
    gfx.setFont(1)
    gfx.text(20, 12, "Key Tester")
    gfx.setFont(0)
    
    -- Show pressed keys
    if #keys > 0 then
        local names = {}
        for _, k in ipairs(keys) do
            table.insert(names, getKeyName(k))
        end
        local keyStr = table.concat(names, " ")
        gfx.text(10, 30, "Pressed: " .. keyStr)
    else
        gfx.text(10, 30, "Press any key...")
    end
    
    gfx.text(2, 55, "Hold ESC to exit")
    
    -- Draw key indicator boxes
    local startX = 10
    for i, key in ipairs(keys) do
        local kx = startX + (i-1) * 20
        if kx < 110 then
            gfx.fillRect(kx, 38, 18, 12)
            gfx.setColor(0)
            gfx.text(kx + 2, 47, getKeyName(key))
            gfx.setColor(1)
        end
    end
    
    gfx.send()
    
    -- Exit condition: ESC held
    for _, k in ipairs(keys) do
        if k == input.KEY_ESC then
            running = false
        end
    end
    
    sys.delay(50)
    sys.yield()
end

print("Key tester ended")
