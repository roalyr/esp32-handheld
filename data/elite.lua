-- Elite-style Space Trading Game
-- Classic wireframe 3D, trading, combat

-- Game state
local state = "docked" -- docked, flying, combat, hyperspace
local credits = 100
local fuel = 7
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
  {name="Textiles", base=15, var=8},
  {name="Minerals", base=25, var=12},
  {name="Gold", base=80, var=30},
  {name="Computers", base=120, var=50},
  {name="Narcotics", base=200, var=100},
  {name="Weapons", base=150, var=60},
  {name="Machinery", base=60, var=25}
}

-- Galaxy - procedural star systems
local systems = {}
local current_sys = 1
local target_sys = 0
local NUM_SYSTEMS = 16

-- 3D flight variables
local px, py, pz = 0, 0, -500  -- player position
local pitch, yaw = 0, 0
local speed = 0
local MAX_SPEED = 20

-- Space objects (stations, ships, asteroids)
local objects = {}

-- Enemy ships in combat
local enemies = {}

-- UI state
local menu_sel = 1
local trade_sel = 1
local trade_mode = "buy"
local galmap_cursor = 1

-- Generate galaxy
local function generateGalaxy()
  math.randomseed(42) -- fixed seed for consistent galaxy
  for i = 1, NUM_SYSTEMS do
    local x = math.random(10, 118)
    local y = math.random(10, 54)
    local tech = math.random(1, 8)
    local econ = math.random(1, 4) -- 1=agri, 2=ind, 3=rich, 4=poor
    local danger = math.random(1, 5)
    
    -- Generate name
    local c1 = string.char(65 + math.random(0, 25))
    local c2 = string.char(97 + math.random(0, 25))
    local c3 = string.char(97 + math.random(0, 25))
    local name = c1 .. c2 .. c3 .. "-" .. i
    
    systems[i] = {
      x = x, y = y,
      name = name,
      tech = tech,
      econ = econ,
      danger = danger
    }
  end
  math.randomseed(sys.millis())
end

-- Calculate distance between systems
local function systemDist(a, b)
  local dx = systems[a].x - systems[b].x
  local dy = systems[a].y - systems[b].y
  return math.sqrt(dx*dx + dy*dy) / 15 -- scale to light years
end

-- Get price at current system
local function getPrice(commIdx)
  local comm = commodities[commIdx]
  local s = systems[current_sys]
  local base = comm.base
  -- Economy affects prices
  if s.econ == 1 then -- agricultural
    if commIdx <= 2 then base = base * 0.7 end
    if commIdx >= 5 then base = base * 1.3 end
  elseif s.econ == 2 then -- industrial
    if commIdx >= 5 then base = base * 0.7 end
    if commIdx <= 2 then base = base * 1.3 end
  elseif s.econ == 3 then -- rich
    base = base * 1.2
  end
  -- Random variance
  local var = (math.random() - 0.5) * comm.var
  return math.floor(base + var)
end

-- Get cargo amount
local function getCargoAmount(name)
  return cargo[name] or 0
end

-- Get total cargo
local function getTotalCargo()
  local total = 0
  for _, amt in pairs(cargo) do
    total = total + amt
  end
  return total
end

-- Spawn station and ships
local function spawnSpaceObjects()
  objects = {}
  enemies = {}
  
  -- Station always at origin
  table.insert(objects, {
    type = "station",
    x = 0, y = 0, z = 0,
    rot = 0
  })
  
  -- Random ships/asteroids
  local danger = systems[current_sys].danger
  for i = 1, danger do
    if math.random() < 0.5 then
      -- Hostile ship
      local angle = math.random() * 6.28
      local dist = math.random(200, 600)
      table.insert(enemies, {
        x = math.cos(angle) * dist,
        y = (math.random() - 0.5) * 200,
        z = math.sin(angle) * dist,
        vx = 0, vy = 0, vz = 0,
        hull = 30 + math.random(20),
        type = "pirate"
      })
    end
  end
  
  -- Asteroids
  for i = 1, 5 do
    local angle = math.random() * 6.28
    local dist = math.random(100, 800)
    table.insert(objects, {
      type = "asteroid",
      x = math.cos(angle) * dist,
      y = (math.random() - 0.5) * 300,
      z = math.sin(angle) * dist,
      size = math.random(5, 15)
    })
  end
end

-- 3D projection
local function project(x, y, z)
  -- Rotate by yaw
  local cy, sy = math.cos(yaw), math.sin(yaw)
  local rx = x * cy - z * sy
  local rz = x * sy + z * cy
  
  -- Rotate by pitch
  local cp, sp = math.cos(pitch), math.sin(pitch)
  local ry = y * cp - rz * sp
  local rz2 = y * sp + rz * cp
  
  -- Translate
  rx = rx - px
  ry = ry - py
  rz2 = rz2 - pz
  
  if rz2 < 10 then return nil, nil, rz2 end
  
  local scale = 200 / rz2
  local sx = 64 + rx * scale
  local sy = 32 - ry * scale
  
  return math.floor(sx), math.floor(sy), rz2
end

-- Draw wireframe cube (station)
local function drawStation(obj)
  local size = 30
  local x, y, z = obj.x, obj.y, obj.z
  local rot = obj.rot
  
  local cr, sr = math.cos(rot), math.sin(rot)
  
  local verts = {}
  for i = -1, 1, 2 do
    for j = -1, 1, 2 do
      for k = -1, 1, 2 do
        local vx = i * size
        local vz = k * size
        -- Rotate
        local rvx = vx * cr - vz * sr
        local rvz = vx * sr + vz * cr
        
        local sx, sy, sz = project(x + rvx, y + j * size, z + rvz)
        if sx then
          table.insert(verts, {sx, sy, sz})
        end
      end
    end
  end
  
  -- Draw edges
  if #verts == 8 then
    local edges = {
      {1,2},{3,4},{5,6},{7,8},
      {1,3},{2,4},{5,7},{6,8},
      {1,5},{2,6},{3,7},{4,8}
    }
    for _, e in ipairs(edges) do
      local v1, v2 = verts[e[1]], verts[e[2]]
      if v1 and v2 then
        gfx.line(v1[1], v1[2], v2[1], v2[2])
      end
    end
  end
end

-- Draw ship (triangle)
local function drawShip(e)
  local sx, sy, sz = project(e.x, e.y, e.z)
  if not sx then return end
  if sx < 0 or sx > 127 or sy < 0 or sy > 63 then return end
  
  local size = math.max(3, math.floor(300 / sz))
  gfx.line(sx, sy - size, sx - size, sy + size)
  gfx.line(sx - size, sy + size, sx + size, sy + size)
  gfx.line(sx + size, sy + size, sx, sy - size)
end

-- Draw asteroid
local function drawAsteroid(obj)
  local sx, sy, sz = project(obj.x, obj.y, obj.z)
  if not sx then return end
  if sx < 0 or sx > 127 or sy < 0 or sy > 63 then return end
  
  local size = math.max(2, math.floor(obj.size * 50 / sz))
  gfx.rect(sx - size, sy - size, size * 2, size * 2)
end

-- Draw HUD
local function drawHUD()
  -- Crosshair
  gfx.line(60, 32, 68, 32)
  gfx.line(64, 28, 64, 36)
  
  -- Speed bar
  gfx.rect(2, 50, 20, 6)
  local spd_w = math.floor(speed / MAX_SPEED * 18)
  gfx.fillRect(3, 51, spd_w, 4)
  
  -- Shield/hull
  gfx.text(2, 8, "S:" .. shields)
  gfx.text(2, 16, "H:" .. hull)
  
  -- Laser temp
  if laser_temp > 0 then
    gfx.text(100, 8, "L:" .. laser_temp)
  end
  
  -- Missiles
  gfx.text(100, 56, "M:" .. missiles)
  
  -- Target indicator
  if #enemies > 0 then
    local e = enemies[1]
    local sx, sy, sz = project(e.x, e.y, e.z)
    if sx and sx >= 0 and sx <= 127 and sy >= 0 and sy <= 63 then
      gfx.rect(sx - 8, sy - 8, 16, 16)
    end
  end
end

-- Draw starfield
local function drawStars()
  math.randomseed(current_sys * 1000)
  for i = 1, 30 do
    local x = math.random(0, 127)
    local y = math.random(0, 63)
    gfx.pixel(x, y)
  end
  math.randomseed(sys.millis())
end

-- Flying state
local function updateFlying()
  -- Pitch/yaw control
  if input.held("2") then pitch = pitch - 0.05 end
  if input.held("8") then pitch = pitch + 0.05 end
  if input.held("4") then yaw = yaw + 0.05 end
  if input.held("6") then yaw = yaw - 0.05 end
  
  -- Speed control
  if input.held("A") then speed = math.min(MAX_SPEED, speed + 0.5) end
  if input.held("B") then speed = math.max(0, speed - 0.5) end
  
  -- Move player
  local dx = math.sin(yaw) * speed
  local dz = math.cos(yaw) * speed
  px = px + dx
  pz = pz + dz
  
  -- Fire laser
  if input.pressed("5") and laser_temp < 80 then
    laser_temp = laser_temp + 10
    -- Check hits
    for i, e in ipairs(enemies) do
      local sx, sy, sz = project(e.x, e.y, e.z)
      if sx and math.abs(sx - 64) < 10 and math.abs(sy - 32) < 10 then
        e.hull = e.hull - 15
        if e.hull <= 0 then
          table.remove(enemies, i)
          credits = credits + math.random(10, 30)
        end
        break
      end
    end
  end
  
  -- Fire missile
  if input.pressed("M") and missiles > 0 and #enemies > 0 then
    missiles = missiles - 1
    enemies[1].hull = enemies[1].hull - 40
    if enemies[1].hull <= 0 then
      table.remove(enemies, 1)
      credits = credits + math.random(20, 50)
    end
  end
  
  -- Cool laser
  if laser_temp > 0 then laser_temp = laser_temp - 1 end
  
  -- Dock with station
  local dist = math.sqrt(px*px + py*py + pz*pz)
  if dist < 50 and speed < 5 then
    if input.pressed("D") then
      state = "docked"
      px, py, pz = 0, 0, -500
      speed = 0
    end
  end
  
  -- Update enemies
  for _, e in ipairs(enemies) do
    -- Simple AI - move toward player
    local edx = px - e.x
    local edy = py - e.y
    local edz = pz - e.z
    local edist = math.sqrt(edx*edx + edy*edy + edz*edz)
    if edist > 50 then
      e.x = e.x + edx / edist * 3
      e.y = e.y + edy / edist * 3
      e.z = e.z + edz / edist * 3
    end
    
    -- Enemy fires
    if edist < 200 and math.random() < 0.02 then
      if shields > 0 then
        shields = shields - 5
      else
        hull = hull - 10
      end
    end
  end
  
  -- Rotate station
  for _, obj in ipairs(objects) do
    if obj.type == "station" then
      obj.rot = obj.rot + 0.01
    end
  end
  
  -- Jump to hyperspace
  if input.pressed("C") then
    state = "galmap"
    galmap_cursor = current_sys
  end
  
  -- Game over
  if hull <= 0 then
    state = "gameover"
  end
end

local function drawFlying()
  gfx.clear()
  drawStars()
  
  -- Draw objects
  for _, obj in ipairs(objects) do
    if obj.type == "station" then
      drawStation(obj)
    elseif obj.type == "asteroid" then
      drawAsteroid(obj)
    end
  end
  
  -- Draw enemies
  for _, e in ipairs(enemies) do
    drawShip(e)
  end
  
  -- Laser effect
  if laser_temp > 70 then
    gfx.line(64, 40, 64, 32)
  end
  
  drawHUD()
  gfx.flip()
end

-- Docked menu
local dock_menu = {"Launch", "Trade", "Equip", "Galaxy Map", "Status"}

local function updateDocked()
  if input.pressed("2") then
    menu_sel = math.max(1, menu_sel - 1)
  elseif input.pressed("8") then
    menu_sel = math.min(#dock_menu, menu_sel + 1)
  elseif input.pressed("5") then
    if menu_sel == 1 then
      state = "flying"
      spawnSpaceObjects()
    elseif menu_sel == 2 then
      state = "trade"
      trade_sel = 1
    elseif menu_sel == 3 then
      state = "equip"
    elseif menu_sel == 4 then
      state = "galmap"
      galmap_cursor = current_sys
    elseif menu_sel == 5 then
      state = "status"
    end
  end
end

local function drawDocked()
  gfx.clear()
  gfx.text(30, 2, "=STATION=")
  gfx.text(2, 12, systems[current_sys].name)
  gfx.text(70, 12, credits .. "CR")
  
  for i, item in ipairs(dock_menu) do
    local y = 22 + (i - 1) * 9
    if i == menu_sel then
      gfx.fillRect(0, y - 1, 128, 9)
      gfx.setColor(0)
      gfx.text(10, y, item)
      gfx.setColor(1)
    else
      gfx.text(10, y, item)
    end
  end
  
  gfx.flip()
end

-- Trade screen
local function updateTrade()
  if input.pressed("2") then
    trade_sel = math.max(1, trade_sel - 1)
  elseif input.pressed("8") then
    trade_sel = math.min(#commodities, trade_sel + 1)
  elseif input.pressed("4") or input.pressed("6") then
    trade_mode = trade_mode == "buy" and "sell" or "buy"
  elseif input.pressed("5") then
    local comm = commodities[trade_sel]
    local price = getPrice(trade_sel)
    if trade_mode == "buy" then
      if credits >= price and getTotalCargo() < CARGO_MAX then
        credits = credits - price
        cargo[comm.name] = (cargo[comm.name] or 0) + 1
      end
    else
      if getCargoAmount(comm.name) > 0 then
        credits = credits + price
        cargo[comm.name] = cargo[comm.name] - 1
      end
    end
  elseif input.pressed("B") then
    state = "docked"
  end
end

local function drawTrade()
  gfx.clear()
  gfx.text(35, 2, "=TRADE=")
  gfx.text(2, 12, trade_mode == "buy" and "[BUY]  sell" or " buy  [SELL]")
  gfx.text(90, 12, credits .. "CR")
  
  local start = math.max(1, trade_sel - 2)
  local yy = 22
  for i = start, math.min(#commodities, start + 4) do
    local comm = commodities[i]
    local price = getPrice(i)
    local have = getCargoAmount(comm.name)
    
    local txt = string.sub(comm.name, 1, 6) .. " " .. price .. " [" .. have .. "]"
    
    if i == trade_sel then
      gfx.fillRect(0, yy - 1, 128, 9)
      gfx.setColor(0)
      gfx.text(2, yy, txt)
      gfx.setColor(1)
    else
      gfx.text(2, yy, txt)
    end
    yy = yy + 9
  end
  
  gfx.text(2, 56, "Cargo:" .. getTotalCargo() .. "/" .. CARGO_MAX)
  gfx.flip()
end

-- Equipment screen
local function updateEquip()
  if input.pressed("5") then
    -- Buy fuel
    local need = MAX_FUEL - fuel
    local cost = need * 5
    if credits >= cost and need > 0 then
      credits = credits - cost
      fuel = MAX_FUEL
    end
  elseif input.pressed("M") then
    -- Buy missile
    if credits >= 30 and missiles < 4 then
      credits = credits - 30
      missiles = missiles + 1
    end
  elseif input.pressed("B") then
    state = "docked"
  end
end

local function drawEquip()
  gfx.clear()
  gfx.text(35, 2, "=EQUIP=")
  gfx.text(90, 2, credits .. "CR")
  
  gfx.text(2, 16, "Fuel: " .. string.format("%.1f", fuel) .. "/" .. MAX_FUEL .. " LY")
  gfx.text(2, 26, "[5] Refuel - " .. ((MAX_FUEL - fuel) * 5) .. "CR")
  
  gfx.text(2, 40, "Missiles: " .. missiles .. "/4")
  gfx.text(2, 50, "[M] Buy missile - 30CR")
  
  gfx.text(40, 56, "[B] Back")
  gfx.flip()
end

-- Galaxy map
local function updateGalmap()
  if input.pressed("2") then
    galmap_cursor = math.max(1, galmap_cursor - 1)
  elseif input.pressed("8") then
    galmap_cursor = math.min(NUM_SYSTEMS, galmap_cursor + 1)
  elseif input.pressed("5") then
    if galmap_cursor ~= current_sys then
      local dist = systemDist(current_sys, galmap_cursor)
      if dist <= fuel then
        fuel = fuel - dist
        current_sys = galmap_cursor
        shields = math.min(100, shields + 20)
        state = "hyperspace"
        spawnSpaceObjects()
      end
    end
  elseif input.pressed("B") then
    state = "docked"
  end
end

local function drawGalmap()
  gfx.clear()
  gfx.text(30, 2, "=GALAXY=")
  
  -- Draw systems
  for i, s in ipairs(systems) do
    local x, y = s.x, s.y
    if i == current_sys then
      gfx.fillCircle(x, y, 3)
    elseif i == galmap_cursor then
      gfx.circle(x, y, 4)
    else
      gfx.pixel(x, y)
    end
  end
  
  -- Draw range circle
  local cs = systems[current_sys]
  local range = math.floor(fuel * 15)
  gfx.circle(cs.x, cs.y, range)
  
  -- Info
  local ts = systems[galmap_cursor]
  local dist = systemDist(current_sys, galmap_cursor)
  gfx.fillRect(0, 56, 128, 8)
  gfx.setColor(0)
  gfx.text(2, 57, ts.name .. " D:" .. string.format("%.1f", dist) .. "LY")
  gfx.setColor(1)
  
  gfx.flip()
end

-- Status screen
local function drawStatus()
  gfx.clear()
  gfx.text(35, 2, "=STATUS=")
  
  gfx.text(2, 14, "Commander")
  gfx.text(2, 24, "Credits: " .. credits)
  gfx.text(2, 34, "Hull: " .. hull .. "%")
  gfx.text(2, 44, "Shields: " .. shields .. "%")
  gfx.text(2, 54, "Fuel: " .. string.format("%.1f", fuel) .. " LY")
  
  gfx.text(70, 24, "Missiles:" .. missiles)
  gfx.text(70, 34, "Cargo:" .. getTotalCargo())
  
  gfx.flip()
  
  if input.pressed("B") or input.pressed("5") then
    state = "docked"
  end
end

-- Hyperspace animation
local hyper_timer = 0
local function updateHyperspace()
  hyper_timer = hyper_timer + 1
  if hyper_timer > 60 then
    hyper_timer = 0
    state = "docked"
  end
end

local function drawHyperspace()
  gfx.clear()
  -- Hyperspace tunnel effect
  for i = 1, 20 do
    local r = hyper_timer + i * 5
    if r < 64 then
      gfx.circle(64, 32, r)
    end
  end
  gfx.text(30, 28, "HYPERSPACE")
  gfx.flip()
end

-- Game over
local function drawGameOver()
  gfx.clear()
  gfx.text(30, 25, "GAME OVER")
  gfx.text(20, 40, "Final: " .. credits .. "CR")
  gfx.flip()
  
  if input.pressed("5") then
    -- Reset game
    credits = 100
    fuel = 7
    hull = 100
    shields = 100
    missiles = 3
    cargo = {}
    current_sys = 1
    state = "docked"
    px, py, pz = 0, 0, -500
  end
end

-- Main
generateGalaxy()

function _update()
  if state == "docked" then
    updateDocked()
  elseif state == "flying" then
    updateFlying()
  elseif state == "trade" then
    updateTrade()
  elseif state == "equip" then
    updateEquip()
  elseif state == "galmap" then
    updateGalmap()
  elseif state == "hyperspace" then
    updateHyperspace()
  elseif state == "status" then
    -- handled in draw
  elseif state == "gameover" then
    -- handled in draw
  end
end

function _draw()
  if state == "docked" then
    drawDocked()
  elseif state == "flying" then
    drawFlying()
  elseif state == "trade" then
    drawTrade()
  elseif state == "equip" then
    drawEquip()
  elseif state == "galmap" then
    drawGalmap()
  elseif state == "hyperspace" then
    drawHyperspace()
  elseif state == "status" then
    drawStatus()
  elseif state == "gameover" then
    drawGameOver()
  end
end
