APP_METADATA = {
    desktop = true,
    name = "Notepad",
    icon = {
        "  ######################  ",
        "  #                    #  ",
        "  #   ##############   #  ",
        "  #   #            #   #  ",
        "  #   #  ########  #   #  ",
        "  #   #            #   #  ",
        "  #   #  ########  #   #  ",
        "  #   #            #   #  ",
        "  #   #  ########  #   #  ",
        "  #   #            #   #  ",
        "  #   ##############   #  ",
        "  #                    #  ",
        "  #                    #  ",
        "  ######################  "
    }
}

APP = {
    logs = {},
    status = "1: Log Timestamp"
}

function APP:init()
    self.logs = {}
    self.status = "1: Log Time  2: Clear"
end

function APP:update() end

function APP:draw()
    gfx.clear()
    gfx.setColor(1)
    ui.header("Notepad", "SD: /notes.txt")

    gfx.setFont(0)
    gfx.text(2, 20, "Last Log Entries:")
    for i, log in ipairs(self.logs) do
        gfx.text(2, 20 + (i * 8), "- " .. log)
    end

    ui.footer(self.status, "ALT+ESC: Exit")
end

function APP:input(key)
    if key == "1" then
        local entry = sys.timeStr() .. " - User Note"
        table.insert(self.logs, 1, entry)
        if #self.logs > 4 then table.remove(self.logs) end

        -- Append log to SD file notes.txt
        local existing = fs.read("/notes.txt") or ""
        local ok, err = fs.write("/notes.txt", existing .. entry .. "\n")
        if ok then
            host.notice("Logged to notes.txt", 1000)
        else
            host.notice("Write Error: " .. tostring(err), 2000)
        end
    elseif key == "2" then
        fs.write("/notes.txt", "")
        self.logs = {}
        host.notice("Cleared notes.txt", 1000)
    end
end
