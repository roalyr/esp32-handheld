-- PROJECT: ESP32-S2-Mini handheld terminal
-- MODULE: data_backup/blank_desktop_app_template.lua
-- STATUS: [Level 2 - Implementation]
-- TRUTH_LINK: TRUTH_HARDWARE.md Section 1, Section 2, Section 3; TACTICAL_TODO TASK_2
-- LOG_REF: 2026-04-28

APP_METADATA = {
    desktop = true,
    name = "Bad icon APP"
}

APP = {}

function APP:init()
    self.message = "Blank SD app"
end

function APP:update()
end

function APP:draw()
    gfx.clear()
    gfx.setFont(0)
    ui.header("Blank", "Template")
    gfx.text(12, 28, "Detected from SD")
    gfx.text(12, 39, "ENT:Ping ALT:Desk")
    ui.footer("ENT:Ping", "ALT:Desk")
end

function APP:input(key)
    if key == input.KEY_ENTER then
        host.notice(self.message, 1200)
    end
end
