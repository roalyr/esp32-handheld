APP_METADATA = {
    desktop = true,
    name = "Render3D",
    icon = {
        "                        ",
        "      ############      ",
        "     #          ##      ",
        "    #          # #      ",
        "   ############  #      ",
        "   #          #  #      ",
        "   #          #  #      ",
        "   #          #  #      ",
        "   #          # #       ",
        "   #          ##        ",
        "   ############         ",
        "                        ",
        "                        "
    }
}

APP = {
    shapes = {},
    perspective_enabled = true,
    culling_enabled = true,
    auto_rotate = true,
    speed_multiplier = 1.0,
    zoom_factor = 1.0,
    camera_pan_x = 0.0
}

-- Winding order must be counter-clockwise when viewed from outside
function APP:createCube(size)
    local h = size / 2
    local vertices = {
        {x = -h, y = -h, z = -h},
        {x =  h, y = -h, z = -h},
        {x =  h, y =  h, z = -h},
        {x = -h, y =  h, z = -h},
        {x = -h, y = -h, z =  h},
        {x =  h, y = -h, z =  h},
        {x =  h, y =  h, z =  h},
        {x = -h, y =  h, z =  h}
    }
    local faces = {
        {1, 4, 3}, {1, 3, 2}, -- Front
        {6, 7, 8}, {6, 8, 5}, -- Back
        {4, 8, 7}, {4, 7, 3}, -- Top
        {1, 2, 6}, {1, 6, 5}, -- Bottom
        {1, 5, 8}, {1, 8, 4}, -- Left
        {2, 3, 7}, {2, 7, 6}  -- Right
    }
    return {vertices = vertices, faces = faces}
end

function APP:createPyramid(size)
    local h = size / 2
    local vertices = {
        {x = -h, y = -h, z = -h},
        {x =  h, y = -h, z = -h},
        {x =  h, y = -h, z =  h},
        {x = -h, y = -h, z =  h},
        {x =  0, y =  h, z =  0}
    }
    local faces = {
        {1, 2, 3}, {1, 3, 4}, -- Base
        {1, 5, 2},            -- Front
        {2, 5, 3},            -- Right
        {3, 5, 4},            -- Back
        {4, 5, 1}             -- Left
    }
    return {vertices = vertices, faces = faces}
end

function APP:createOctahedron(size)
    local h = size / 2
    local vertices = {
        {x =  0, y =  h, z =  0}, -- Top Apex
        {x =  0, y = -h, z =  0}, -- Bottom Apex
        {x = -h, y =  0, z = -h},
        {x =  h, y =  0, z = -h},
        {x =  h, y =  0, z =  h},
        {x = -h, y =  0, z =  h}
    }
    local faces = {
        {1, 4, 3}, {1, 5, 4}, {1, 6, 5}, {1, 3, 6}, -- Top faces
        {2, 3, 4}, {2, 4, 5}, {2, 5, 6}, {2, 6, 3}  -- Bottom faces
    }
    return {vertices = vertices, faces = faces}
end

function APP:createSphere(radius, lat_count, lon_count)
    local vertices = {}
    local faces = {}
    
    for lat = 0, lat_count do
        local theta = lat * math.pi / lat_count
        local sin_theta = math.sin(theta)
        local cos_theta = math.cos(theta)
        for lon = 0, lon_count - 1 do
            local phi = lon * 2 * math.pi / lon_count
            local x = radius * sin_theta * math.cos(phi)
            local y = radius * cos_theta
            local z = radius * sin_theta * math.sin(phi)
            table.insert(vertices, {x = x, y = y, z = z})
        end
    end
    
    for lat = 0, lat_count - 1 do
        for lon = 0, lon_count - 1 do
            local next_lon = (lon + 1) % lon_count
            local p1 = lat * lon_count + lon + 1
            local p2 = lat * lon_count + next_lon + 1
            local p3 = (lat + 1) * lon_count + lon + 1
            local p4 = (lat + 1) * lon_count + next_lon + 1
            
            table.insert(faces, {p1, p3, p2})
            table.insert(faces, {p2, p3, p4})
        end
    end
    
    return {vertices = vertices, faces = faces}
end

function APP:spawnShapes()
    self.shapes = {}
    local shape_factories = {
        function(size) return self:createCube(size) end,
        function(size) return self:createPyramid(size) end,
        function(size) return self:createOctahedron(size) end,
        function(size) return self:createSphere(0.3, 4, 6) end
    }
    
    local positions = {
        {x = -1.6, y = 0.0, z = 4.5},
        {x = 0.0, y = 0.0, z = 5.0},
        {x = 1.6, y = 0.0, z = 4.5}
    }
    
    for i = 1, 3 do
        local factory = shape_factories[math.random(1, #shape_factories)]
        local shape = factory(0.65)
        
        shape.pos = {
            x = positions[i].x + (math.random() - 0.5) * 0.25,
            y = positions[i].y + (math.random() - 0.5) * 0.25,
            z = positions[i].z + (math.random() - 0.5) * 0.6
        }
        
        shape.rot = {
            x = math.random() * math.pi * 2,
            y = math.random() * math.pi * 2,
            z = math.random() * math.pi * 2
        }
        
        shape.speed = {
            x = (math.random() - 0.5) * 0.035 + 0.01,
            y = (math.random() - 0.5) * 0.035 + 0.01,
            z = (math.random() - 0.5) * 0.02
        }
        
        table.insert(self.shapes, shape)
    end
end

function APP:init()
    math.randomseed(sys.millis())
    self:spawnShapes()
    self.perspective_enabled = true
    self.culling_enabled = true
    self.auto_rotate = true
    self.speed_multiplier = 1.0
    self.zoom_factor = 1.0
    self.camera_pan_x = 0.0
end

local function rotate_x(x, y, z, angle)
    local cos_a = math.cos(angle)
    local sin_a = math.sin(angle)
    return x, y * cos_a - z * sin_a, y * sin_a + z * cos_a
end

local function rotate_y(x, y, z, angle)
    local cos_a = math.cos(angle)
    local sin_a = math.sin(angle)
    return x * cos_a + z * sin_a, y, -x * sin_a + z * cos_a
end

local function rotate_z(x, y, z, angle)
    local cos_a = math.cos(angle)
    local sin_a = math.sin(angle)
    return x * cos_a - y * sin_a, x * sin_a + y * cos_a, z
end

local function is_face_front(p1, p2, p3, is_perspective)
    local ax = p2.x - p1.x
    local ay = p2.y - p1.y
    local az = p2.z - p1.z
    local bx = p3.x - p1.x
    local by = p3.y - p1.y
    local bz = p3.z - p1.z
    local nx = ay * bz - az * by
    local ny = az * bx - ax * bz
    local nz = ax * by - ay * bx
    
    if is_perspective then
        return (nx * p1.x + ny * p1.y + nz * p1.z) < 0
    else
        return nz < 0
    end
end

function APP:update()
    if not self.auto_rotate then return end
    
    for _, shape in ipairs(self.shapes) do
        shape.rot.x = shape.rot.x + shape.speed.x * self.speed_multiplier
        shape.rot.y = shape.rot.y + shape.speed.y * self.speed_multiplier
        shape.rot.z = shape.rot.z + shape.speed.z * self.speed_multiplier
    end
end

function APP:draw()
    gfx.clear()
    gfx.setColor(1)
    
    local proj_str = self.perspective_enabled and "PERS" or "ORTH"
    local cull_str = self.culling_enabled and "CUL" or "OFF"
    local info_str = string.format("%s|%s Z:%.1f P:%.1f", proj_str:sub(1,1), cull_str:sub(1,1), self.zoom_factor, self.camera_pan_x)
    ui.header("3D Renderer", info_str)
    
    local layout = ui.metrics()
    local cx = math.floor(layout.screen_width / 2)
    local cy = math.floor((layout.content_top + layout.content_bottom) / 2)
    
    local fov_scale = 90 * self.zoom_factor
    local ortho_scale = 22 * self.zoom_factor
    
    for _, shape in ipairs(self.shapes) do
        local transformed = {}
        for _, v in ipairs(shape.vertices) do
            local rx, ry, rz = rotate_y(v.x, v.y, v.z, shape.rot.y)
            rx, ry, rz = rotate_x(rx, ry, rz, shape.rot.x)
            rx, ry, rz = rotate_z(rx, ry, rz, shape.rot.z)
            
            local wx = rx + shape.pos.x - self.camera_pan_x
            local wy = ry + shape.pos.y
            local wz = rz + shape.pos.z
            
            table.insert(transformed, {x = wx, y = wy, z = wz})
        end
        
        local projected = {}
        for _, w in ipairs(transformed) do
            local sx, sy
            if self.perspective_enabled then
                local depth = w.z
                if depth < 0.1 then depth = 0.1 end
                sx = cx + (w.x * fov_scale) / depth
                sy = cy - (w.y * fov_scale) / depth
            else
                sx = cx + w.x * ortho_scale
                sy = cy - w.y * ortho_scale
            end
            table.insert(projected, {x = math.floor(sx), y = math.floor(sy)})
        end
        
        for _, face in ipairs(shape.faces) do
            local p1 = transformed[face[1]]
            local p2 = transformed[face[2]]
            local p3 = transformed[face[3]]
            
            local show = true
            if self.culling_enabled then
                show = is_face_front(p1, p2, p3, self.perspective_enabled)
            end
            
            if show then
                for i = 1, #face do
                    local next_idx = i % #face + 1
                    local v1 = projected[face[i]]
                    local v2 = projected[face[next_idx]]
                    if v1 and v2 then
                        gfx.line(v1.x, v1.y, v2.x, v2.y)
                    end
                end
            end
        end
    end
    
    ui.footer("1:Proj 2:Cull 3:Auto 4:New", "Arrows: Camera")
end

function APP:input(key)
    if key == "1" then
        self.perspective_enabled = not self.perspective_enabled
        host.notice(self.perspective_enabled and "Perspective" or "Orthographic", 1000)
    elseif key == "2" then
        self.culling_enabled = not self.culling_enabled
        host.notice(self.culling_enabled and "Culling: ON" or "Culling: OFF", 1000)
    elseif key == "3" then
        self.auto_rotate = not self.auto_rotate
        host.notice(self.auto_rotate and "Auto-Rotate" or "Manual Control", 1000)
    elseif key == "4" then
        self:spawnShapes()
        host.notice("Shapes Randomized", 1000)
    elseif key == input.KEY_UP then
        self.zoom_factor = math.min(3.0, self.zoom_factor + 0.1)
        host.notice(string.format("Zoom: %.1f", self.zoom_factor), 500)
    elseif key == input.KEY_DOWN then
        self.zoom_factor = math.max(0.3, self.zoom_factor - 0.1)
        host.notice(string.format("Zoom: %.1f", self.zoom_factor), 500)
    elseif key == input.KEY_LEFT then
        self.camera_pan_x = self.camera_pan_x - 0.15
        host.notice(string.format("Pan: %.2f", self.camera_pan_x), 500)
    elseif key == input.KEY_RIGHT then
        self.camera_pan_x = self.camera_pan_x + 0.15
        host.notice(string.format("Pan: %.2f", self.camera_pan_x), 500)
    end
end
