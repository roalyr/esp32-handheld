APP_METADATA = {
    desktop = true,
    name = "Snake",
    icon = {
        "  ######################  ",
        "  #                    #  ",
        "  #   #####            #  ",
        "  #  ##   ##           #  ",
        "  #  #     #           #  ",
        "  #  #  ## #           #  ",
        "  #  #   ###           #  ",
        "  #  ##                #  ",
        "  #   #####            #  ",
        "  #       ##           #  ",
        "  #  ##   ##           #  ",
        "  #   #####            #  ",
        "  #                    #  ",
        "  ######################  "
    }
}

APP = {
    snake = {},
    dir = {x = 1, y = 0},
    food = {x = 5, y = 5},
    score = 0,
    game_over = false,
    timer = 0
}

function APP:init()
    self.snake = {{x = 10, y = 10}, {x = 9, y = 10}, {x = 8, y = 10}}
    self.dir = {x = 1, y = 0}
    self.score = 0
    self.game_over = false
    self:placeFood()
end

function APP:placeFood()
    self.food.x = math.random(1, 23)
    self.food.y = math.random(3, 10)
end

function APP:update()
    if self.game_over then return end
    
    self.timer = self.timer + 1
    if self.timer < 3 then return end -- Adjust speed (roughly 10 FPS game loop)
    self.timer = 0

    -- Move head
    local head = self.snake[1]
    local new_head = {x = head.x + self.dir.x, y = head.y + self.dir.y}

    -- Wrap screen
    if new_head.x > 24 then new_head.x = 0 end
    if new_head.x < 0 then new_head.x = 24 end
    if new_head.y > 11 then new_head.y = 3 end
    if new_head.y < 3 then new_head.y = 11 end

    -- Check self collision
    for _, segment in ipairs(self.snake) do
        if new_head.x == segment.x and new_head.y == segment.y then
            self.game_over = true
            host.notice("Game Over!", 2000)
            return
        end
    end

    -- Insert new head
    table.insert(self.snake, 1, new_head)

    -- Check food collision
    if new_head.x == self.food.x and new_head.y == self.food.y then
        self.score = self.score + 10
        self:placeFood()
    else
        table.remove(self.snake) -- Remove tail
    end
end

function APP:draw()
    gfx.clear()
    gfx.setColor(1)
    ui.header("SNAKE", "Score: " .. tostring(self.score))

    if self.game_over then
        gfx.setFont(1)
        gfx.text(30, 30, "GAME OVER")
        ui.footer("ENTER: Restart", "ALT+ESC: Exit")
        return
    end

    -- Draw border
    gfx.rect(0, 15, 128, 48)

    -- Draw food (as a 4x4 square)
    gfx.fillRect(self.food.x * 5 + 2, self.food.y * 4 + 4, 4, 3)

    -- Draw snake
    for _, segment in ipairs(self.snake) do
        gfx.fillRect(segment.x * 5 + 2, segment.y * 4 + 4, 4, 3)
    end

    ui.footer("Arrows: Move", "ALT+ESC: Exit")
end

function APP:input(key)
    if self.game_over and key == input.KEY_ENTER then
        self:init()
    elseif key == input.KEY_UP and self.dir.y == 0 then
        self.dir = {x = 0, y = -1}
    elseif key == input.KEY_DOWN and self.dir.y == 0 then
        self.dir = {x = 0, y = 1}
    elseif key == input.KEY_LEFT and self.dir.x == 0 then
        self.dir = {x = -1, y = 0}
    elseif key == input.KEY_RIGHT and self.dir.x == 0 then
        self.dir = {x = 1, y = 0}
    end
end
