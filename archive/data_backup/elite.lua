-- Elite-style Space Trading Game
-- Classic wireframe 3D, trading, combat
-- Controls: Arrows=Navigate, Enter=Select, ESC=Exit

-- Game state
local state = "docked" -- docked, flying, trade, equip, galmap, status, hyperspace, gameover
local credits = 100
local fuel = 7.0
local MAX_FUEL = 7

-- Ship stats
local hull = 100
local shields = 100
local missiles = 3
local laser_temp = 0

-- Cargo hold (max 20 tons)
local cargo = {}
local CARGO_MAX = 20

-- Commodities: name, base_price, variance
local commodities = {
  {name="Food", base=10, var=5},
  {name="Textile", base=15, var=8},
  {name="Mineral", base=25, var=12},
  {name="Gold", base=80, var=30},
  {name="Compute", base=120, var=50},
  {name="Narcoti", base=200, var=100},
  {name="Weapons", base=150, var=60},
  {name="Machine", base=60, var=25}
}

-- Galaxy - procedural star systems
local systems = {}
local current_sys = 1
local NUM_SYSTEMS = 12

-- 3D flight variables
local px, py, pz = 0, 0, -400
local pitch, yaw = 0, 0
local speed = 0
local MAX_SPEED = 15

-- Space objects
local objects = {}
local enemies = {}

-- UI state
local menu_sel = 1
local trade_sel = 1
local trade_mode = "buy"
local galmap_cursor = 1
local hyper_timer = 0

-- Generate galaxy with fixed seed
local function generateGalaxy()
  math.randomseed(42)
  for i = 1, NUM_SYSTEMS do
    local x = 10 + ((i-1) % 4) * 35
    local y = 15 + math.floor((i-1) / 4) * 18
    local tech = math.random(1, 8)
    local econ = math.random(1, 4)
    local danger = math.random(1, 4)
    local c1 = string.char(65 + math.random(0, 25))
    local c2 = string.char(97 + math.random(0, 25))
    systems[i] = {
      x = x, y = y,
      name = c1 .. c2 .. i,
      tech = tech, econ = econ, danger = danger
    }
  end
  math.randomseed(sys.millis())
end

-- Distance between systems
local function systemDist(a, b)
  local dx = systems[a].x - systems[b].x
  local dy = systems[a].y - systems[b].y
  return math.sqrt(dx*dx + dy*dy) / 12
end

-- Get price at current system
local function getPrice(idx)
  local c = commodities[idx]
  local s = systems[current_sys]
  local base = c.base
  if s.econ == 1 then
    if idx <= 2 then base = base * 0.7 end
    if idx >= 5 then base = base * 1.3 end
  elseif s.econ == 2 then
    if idx >= 5 then base = base * 0.7 end
    if idx <= 2 then base = base * 1.3 end
  elseif s.econ == 3 then
    base = base * 1.2
  end
  return math.floor(base + (math.random() - 0.5) * c.var)
end

local function getCargoAmt(name)
  return cargo[name] or 0
end

local function getTotalCargo()
  local t = 0
  for _, amt in pairs(cargo) do t = t + amt end
  return t
end

-- Spawn objects when launching
local function spawnObjects()
  objects = {}
  enemies = {}
  table.insert(objects, {type="station", x=0, y=0, z=0, rot=0})
  local danger = systems[current_sys].danger
  for i = 1, danger do
    local ang = math.random() * 6.28
    local dist = 200 + math.random(200)
    table.insert(enemies, {
      x = math.cos(ang) * dist,
      y = (math.random() - 0.5) * 100,
      z = math.sin(ang) * dist,
      hull = 30 + math.random(20)
    })
  end
end

-- 3D projection
local function project(x, y, z)
  local cy, sy = math.cos(yaw), math.sin(yaw)
  local rx = x * cy - z * sy
  local rz = x * sy + z * cy
  local cp, sp = math.cos(pitch), math.sin(pitch)
  local ry = y * cp - rz * sp
  local rz2 = y * sp + rz * cp
  rx = rx - px
  ry = ry - py
  rz2 = rz2 - pz
  if rz2 < 20 then return nil, nil, rz2 end
  local scale = 150 / rz2
  return math.floor(64 + rx * scale), math.floor(32 - ry * scale), rz2
end

-- Draw wireframe station
local function drawStation(obj)
  local sz = 25
  local x, y, z = obj.x, obj.y, obj.z
  local cr, sr = math.cos(obj.rot), math.sin(obj.rot)
  local verts = {}
  for i = -1, 1, 2 do
    for j = -1, 1, 2 do
      for k = -1, 1, 2 do
        local vx, vz = i * sz, k * sz
        local rvx = vx * cr - vz * sr
        local rvz = vx * sr + vz * cr
        local sx, sy, _ = project(x + rvx, y + j * sz, z + rvz)
        if sx then table.insert(verts, {sx, sy}) end
      end
    end
  end
  if #verts == 8 then
    local edges = {{1,2},{3,4},{5,6},{7,8},{1,3},{2,4},{5,7},{6,8},{1,5},{2,6},{3,7},{4,8}}
    for _, e in ipairs(edges) do
      gfx.line(verts[e[1]][1], verts[e[1]][2], verts[e[2]][1], verts[e[2]][2])
    end
  end
end

-- Draw enemy ship
local function drawEnemy(e)
  local sx, sy, sz = project(e.x, e.y, e.z)
  if not sx or sx < 0 or sx > 127 or sy < 0 or sy > 63 then return end
  local s = math.max(3, math.floor(200 / sz))
  gfx.line(sx, sy - s, sx - s, sy + s)
  gfx.line(sx - s, sy + s, sx + s, sy + s)
  gfx.line(sx + s, sy + s, sx, sy - s)
end

-- Draw stars
local function drawStars()
  math.randomseed(current_sys * 100)
  for i = 1, 20 do
    gfx.pixel(math.random(0, 127), math.random(0, 63))
  end
  math.randomseed(sys.millis())
end

-- Draw flight HUD
local function drawHUD()
  gfx.line(60, 32, 68, 32)
  gfx.line(64, 28, 64, 36)
  gfx.rect(2, 54, 22, 6)
  gfx.fillRect(3, 55, math.floor(speed / MAX_SPEED * 20), 4)
  gfx.text(2, 7, "S" .. shields)
  gfx.text(2, 15, "H" .. hull)
  if laser_temp > 0 then gfx.text(100, 7, "L" .. laser_temp) end
  gfx.text(104, 60, "M" .. missiles)
  if #enemies > 0 then
    local sx, sy, _ = project(enemies[1].x, enemies[1].y, enemies[1].z)
    if sx and sx >= 0 and sx <= 127 and sy >= 0 and sy <= 63 then
      gfx.rect(sx - 6, sy - 6, 12, 12)
    end
  end
end

-- DOCKED STATE
local dock_menu = {"Launch", "Trade", "Equip", "GalMap", "Status"}

local function updateDocked()
  if input.pressed(input.KEY_UP) then menu_sel = math.max(1, menu_sel - 1)
  elseif input.pressed(input.KEY_DOWN) then menu_sel = math.min(#dock_menu, menu_sel + 1)
  elseif input.pressed(input.KEY_ENTER) then
    if menu_sel == 1 then
      state = "flying"
      px, py, pz = 0, 0, -400
      speed = 0
      spawnObjects()
    elseif menu_sel == 2 then state = "trade"; trade_sel = 1
    elseif menu_sel == 3 then state = "equip"
    elseif menu_sel == 4 then state = "galmap"; galmap_cursor = current_sys
    elseif menu_sel == 5 then state = "status"
    end
  end
end

local function drawDocked()
  gfx.clear()
  gfx.text(20, 7, "== STATION ==")
  gfx.text(2, 16, systems[current_sys].name)
  gfx.text(80, 16, credits .. "CR")
  for i, item in ipairs(dock_menu) do
    local y = 24 + (i - 1) * 8
    if i == menu_sel then
      gfx.fillRect(0, y - 1, 128, 8)
      gfx.setColor(0)
    end
    gfx.text(10, y, item)
    gfx.setColor(1)
  end
  gfx.send()
end

-- FLYING STATE
local function updateFlying()
  if input.held(input.KEY_UP) then pitch = pitch - 0.04 end
  if input.held(input.KEY_DOWN) then pitch = pitch + 0.04 end
  if input.held(input.KEY_LEFT) then yaw = yaw + 0.04 end
  if input.held(input.KEY_RIGHT) then yaw = yaw - 0.04 end
  if input.held("1") then speed = math.min(MAX_SPEED, speed + 0.3) end
  if input.held("3") then speed = math.max(0, speed - 0.3) end
  
  px = px + math.sin(yaw) * speed
  pz = pz + math.cos(yaw) * speed
  
  -- Fire laser
  if input.pressed(input.KEY_ENTER) and laser_temp < 70 then
    laser_temp = laser_temp + 12
    for i, e in ipairs(enemies) do
      local sx, sy, _ = project(e.x, e.y, e.z)
      if sx and math.abs(sx - 64) < 12 and math.abs(sy - 32) < 12 then
        e.hull = e.hull - 18
        if e.hull <= 0 then
          table.remove(enemies, i)
          credits = credits + 10 + math.random(20)
        end
        break
      end
    end
  end
  
  -- Fire missile
  if input.pressed("2") and missiles > 0 and #enemies > 0 then
    missiles = missiles - 1
    enemies[1].hull = enemies[1].hull - 45
    if enemies[1].hull <= 0 then
      table.remove(enemies, 1)
      credits = credits + 20 + math.random(30)
    end
  end
  
  if laser_temp > 0 then laser_temp = laser_temp - 1 end
  
  -- Dock
  local dist = math.sqrt(px*px + py*py + pz*pz)
  if dist < 60 and speed < 5 and input.pressed("0") then
    state = "docked"
    menu_sel = 1
  end
  
  -- Enemy AI
  for _, e in ipairs(enemies) do
    local dx, dy, dz = px - e.x, py - e.y, pz - e.z
    local d = math.sqrt(dx*dx + dy*dy + dz*dz)
    if d > 60 then
      e.x = e.x + dx / d * 2
      e.y = e.y + dy / d * 2
      e.z = e.z + dz / d * 2
    end
    if d < 180 and math.random() < 0.015 then
      if shields > 0 then shields = shields - 4
      else hull = hull - 8 end
    end
  end
  
  -- Rotate station
  for _, obj in ipairs(objects) do
    if obj.type == "station" then obj.rot = obj.rot + 0.008 end
  end
  
  -- Galaxy map
  if input.pressed(input.KEY_TAB) then
    state = "galmap"
    galmap_cursor = current_sys
  end
  
  if hull <= 0 then state = "gameover" end
end

local function drawFlying()
  gfx.clear()
  drawStars()
  for _, obj in ipairs(objects) do
    if obj.type == "station" then drawStation(obj) end
  end
  for _, e in ipairs(enemies) do drawEnemy(e) end
  if laser_temp > 60 then gfx.line(64, 38, 64, 32) end
  drawHUD()
  gfx.send()
end

-- TRADE STATE
local function updateTrade()
  if input.pressed(input.KEY_UP) then trade_sel = math.max(1, trade_sel - 1)
  elseif input.pressed(input.KEY_DOWN) then trade_sel = math.min(#commodities, trade_sel + 1)
  elseif input.pressed(input.KEY_LEFT) or input.pressed(input.KEY_RIGHT) then
    trade_mode = trade_mode == "buy" and "sell" or "buy"
  elseif input.pressed(input.KEY_ENTER) then
    local c = commodities[trade_sel]
    local price = getPrice(trade_sel)
    if trade_mode == "buy" then
      if credits >= price and getTotalCargo() < CARGO_MAX then
        credits = credits - price
        cargo[c.name] = (cargo[c.name] or 0) + 1
      end
    else
      if getCargoAmt(c.name) > 0 then
        credits = credits + price
        cargo[c.name] = cargo[c.name] - 1
      end
    end
  elseif input.pressed(input.KEY_ESC) then state = "docked" end
end

local function drawTrade()
  gfx.clear()
  gfx.text(30, 7, "== TRADE ==")
  gfx.text(2, 15, trade_mode == "buy" and "[BUY] sell" or "buy [SELL]")
  gfx.text(85, 15, credits .. "CR")
  local start = math.max(1, trade_sel - 2)
  local yy = 24
  for i = start, math.min(#commodities, start + 4) do
    local c = commodities[i]
    local price = getPrice(i)
    local have = getCargoAmt(c.name)
    local txt = c.name .. " " .. price .. " [" .. have .. "]"
    if i == trade_sel then
      gfx.fillRect(0, yy - 1, 128, 8)
      gfx.setColor(0)
    end
    gfx.text(2, yy, txt)
    gfx.setColor(1)
    yy = yy + 8
  end
  gfx.text(2, 58, "Cargo:" .. getTotalCargo() .. "/" .. CARGO_MAX)
  gfx.send()
end

-- EQUIP STATE
local function updateEquip()
  if input.pressed(input.KEY_ENTER) then
    local need = MAX_FUEL - fuel
    local cost = math.floor(need * 5)
    if credits >= cost and need > 0.1 then
      credits = credits - cost
      fuel = MAX_FUEL
    end
  elseif input.pressed("2") then
    if credits >= 30 and missiles < 4 then
      credits = credits - 30
      missiles = missiles + 1
    end
  elseif input.pressed(input.KEY_ESC) then state = "docked" end
end

local function drawEquip()
  gfx.clear()
  gfx.text(30, 7, "== EQUIP ==")
  gfx.text(85, 7, credits .. "CR")
  gfx.text(2, 20, "Fuel: " .. string.format("%.1f", fuel) .. "/" .. MAX_FUEL)
  local need = MAX_FUEL - fuel
  gfx.text(2, 30, "[OK] Refuel " .. math.floor(need * 5) .. "CR")
  gfx.text(2, 44, "Missiles: " .. missiles .. "/4")
  gfx.text(2, 54, "[2] Buy +1 = 30CR")
  gfx.send()
end

-- GALMAP STATE
local function updateGalmap()
  if input.pressed(input.KEY_UP) then galmap_cursor = math.max(1, galmap_cursor - 1)
  elseif input.pressed(input.KEY_DOWN) then galmap_cursor = math.min(NUM_SYSTEMS, galmap_cursor + 1)
  elseif input.pressed(input.KEY_LEFT) then galmap_cursor = math.max(1, galmap_cursor - 4)
  elseif input.pressed(input.KEY_RIGHT) then galmap_cursor = math.min(NUM_SYSTEMS, galmap_cursor + 4)
  elseif input.pressed(input.KEY_ENTER) then
    if galmap_cursor ~= current_sys then
      local d = systemDist(current_sys, galmap_cursor)
      if d <= fuel then
        fuel = fuel - d
        current_sys = galmap_cursor
        shields = math.min(100, shields + 20)
        state = "hyperspace"
        hyper_timer = 0
        spawnObjects()
      end
    end
  elseif input.pressed(input.KEY_ESC) then state = "docked" end
end

local function drawGalmap()
  gfx.clear()
  gfx.text(25, 7, "== GALAXY ==")
  for i, s in ipairs(systems) do
    if i == current_sys then gfx.fillCircle(s.x, s.y, 3)
    elseif i == galmap_cursor then gfx.circle(s.x, s.y, 4)
    else gfx.pixel(s.x, s.y) end
  end
  local cs = systems[current_sys]
  local range = math.floor(fuel * 12)
  gfx.circle(cs.x, cs.y, range)
  local ts = systems[galmap_cursor]
  local d = systemDist(current_sys, galmap_cursor)
  gfx.fillRect(0, 56, 128, 8)
  gfx.setColor(0)
  gfx.text(2, 57, ts.name .. " D:" .. string.format("%.1f", d) .. " F:" .. string.format("%.1f", fuel))
  gfx.setColor(1)
  gfx.send()
end

-- STATUS STATE
local function updateStatus()
  if input.pressed(input.KEY_ESC) or input.pressed(input.KEY_ENTER) then state = "docked" end
end

local function drawStatus()
  gfx.clear()
  gfx.text(25, 7, "== STATUS ==")
  gfx.text(2, 18, "Credits: " .. credits)
  gfx.text(2, 28, "Hull: " .. hull .. "%")
  gfx.text(2, 38, "Shields: " .. shields .. "%")
  gfx.text(2, 48, "Fuel: " .. string.format("%.1f", fuel) .. "LY")
  gfx.text(70, 28, "Miss:" .. missiles)
  gfx.text(70, 38, "Cargo:" .. getTotalCargo())
  gfx.send()
end

-- HYPERSPACE STATE
local function updateHyperspace()
  hyper_timer = hyper_timer + 1
  if hyper_timer > 40 then
    state = "docked"
    menu_sel = 1
  end
end

local function drawHyperspace()
  gfx.clear()
  for i = 1, 15 do
    local r = hyper_timer + i * 4
    if r < 50 then gfx.circle(64, 32, r) end
  end
  gfx.text(25, 30, "HYPERSPACE")
  gfx.send()
end

-- GAMEOVER STATE
local function updateGameover()
  if input.pressed(input.KEY_ENTER) then
    credits = 100
    fuel = 7.0
    hull = 100
    shields = 100
    missiles = 3
    cargo = {}
    current_sys = 1
    state = "docked"
    menu_sel = 1
  end
end

local function drawGameover()
  gfx.clear()
  gfx.text(30, 25, "GAME OVER")
  gfx.text(20, 38, "Score: " .. credits .. "CR")
  gfx.text(20, 50, "[Enter] Restart")
  gfx.send()
end

-- MAIN LOOP
print("Elite started - Hold ESC to exit")
generateGalaxy()

local running = true
while running do
  input.scan()
  
  -- Update current state
  if state == "docked" then updateDocked()
  elseif state == "flying" then updateFlying()
  elseif state == "trade" then updateTrade()
  elseif state == "equip" then updateEquip()
  elseif state == "galmap" then updateGalmap()
  elseif state == "status" then updateStatus()
  elseif state == "hyperspace" then updateHyperspace()
  elseif state == "gameover" then updateGameover()
  end
  
  -- Draw current state
  if state == "docked" then drawDocked()
  elseif state == "flying" then drawFlying()
  elseif state == "trade" then drawTrade()
  elseif state == "equip" then drawEquip()
  elseif state == "galmap" then drawGalmap()
  elseif state == "status" then drawStatus()
  elseif state == "hyperspace" then drawHyperspace()
  elseif state == "gameover" then drawGameover()
  end
  
  -- Exit check (only from docked)
  if state == "docked" and input.held(input.KEY_ESC) then
    running = false
  end
  
  sys.delay(33)  -- ~30 FPS
  sys.yield()
end

print("Elite ended. Final credits: " .. credits)
