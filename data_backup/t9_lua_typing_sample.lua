-- PROJECT: ESP32-S2-Mini handheld terminal
-- MODULE: data_backup/t9_lua_typing_sample.lua
-- STATUS: [Level 2 - Implementation]
-- TRUTH_LINK: TACTICAL_TODO TASK_2
-- LOG_REF: 2026-04-22
-- Description: Lua typing comfort reference for the on-device T9 editor.

-- operators: + - * / % == ~= <= >= and or not .. #

local config = {
  title = "T9 Lua Sample",
  path = "scripts/sample.lua",
  flags = { enabled = true, debug = false },
  keys = { "up", "down", "left", "right" },
  limits = { min = 1, max = 9 },
}

local function clamp(value, low, high)
  if value < low then
    return low
  elseif value > high then
    return high
  else
    return value
  end
end

local function describe(score, name)
  local label = "player:\t" .. name .. "\nscore:\t" .. tostring(score)
  if score >= 100 and name ~= "" then
    return label .. "\nrank:\t\"gold\""
  elseif score >= 50 or name == "dev" then
    return label .. "\nrank:\t'silver'"
  else
    return label .. "\nrank:\tbronze"
  end
end

local total = 0
for i = 1, 5 do
  total = total + (i * 2)
end

local index = 1
while index <= #config.keys do
  local key = config.keys[index]
  if key == "left" or key == "right" then
    print("axis<" .. key .. ">")
  else
    print("move[" .. key .. "]")
  end
  index = index + 1
end

local expr = (total + 3) * 2 / 5 % 4
local pair = { open = "(", close = ")" }
local escaped = "quote: \"ok\", slash: \\\\, tab: \\t"

return {
  config = config,
  total = clamp(total, config.limits.min, config.limits.max * 3),
  expr = expr,
  pair = pair,
  message = describe(total, "lua_user"),
  escaped = escaped,
}