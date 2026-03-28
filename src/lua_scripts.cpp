#include "lua_scripts.h"

const char LUA_DESKTOP[] = R"LUA(
local cx, cy = 1, 1
local COLS, ROWS = 4, 2
function _init() end
function _draw()
    gfx.clear()
    gfx.setFont(0)
    gfx.fillRect(0, 0, 128, 12)
    gfx.setColor(0)
    gfx.text(2, 9, "Desktop (embedded)")
    gfx.setColor(1)
    for r = 1, ROWS do
        for c = 1, COLS do
            local x = (c-1)*32
            local y = 13 + (r-1)*20
            gfx.rect(x, y, 32, 20)
            if r==cy and c==cx then
                gfx.fillRect(x+1, y+1, 30, 18)
            end
        end
    end
    gfx.line(0, 54, 128, 54)
    gfx.text(2, 62, "ESC:Settings")
    gfx.send()
end
function _update() end
function _input(key)
    if key==input.KEY_UP and cy>1 then cy=cy-1 end
    if key==input.KEY_DOWN and cy<ROWS then cy=cy+1 end
    if key==input.KEY_LEFT and cx>1 then cx=cx-1 end
    if key==input.KEY_RIGHT and cx<COLS then cx=cx+1 end
end
)LUA";
