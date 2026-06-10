-- hello.lua - Simple test script for the Lua Runner
-- Demonstrates the available APIs

print("Hello from Lua!")
print("Display size: " .. gfx.width() .. "x" .. gfx.height())

-- Draw a simple test pattern
gfx.clear()
gfx.setFont(1)
gfx.text(10, 20, "Lua Works!")
gfx.setFont(0)
gfx.text(10, 35, "Press keys to test")

-- Draw some shapes
gfx.rect(0, 0, 40, 15)
gfx.fillRect(90, 0, 38, 10)
gfx.circle(64, 50, 10)

gfx.send()

print("Script complete!")
print("Memory used: " .. collectgarbage("count") .. " KB")
