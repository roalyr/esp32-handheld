#include "lua_blank_app_template.h"

#include "config.h"

namespace {

static const char kLuaBlankAppTemplateBody[] = R"LUA(APP_METADATA = {
 desktop = true,
 name = "Blank",
 icon = {
  "  ######################  ",
  "  #                    #  ",
  "  #       ######       #  ",
  "  #      ##    ##      #  ",
  "  #      # ## ## #     #  ",
  "  #      #  ##  #      #  ",
  "  #      # #### #      #  ",
  "  #      #  ##  #      #  ",
  "  #      # ## ## #     #  ",
  "  #      ##    ##      #  ",
  "  #       ######       #  ",
  "  #                    #  ",
  "  ######################  "
 }
}

APP = {
 tick = 0,
 last_status = "Ready"
}

local FONT_SMALL = 0
local FONT_MEDIUM = 1
local FONT_LARGE = 2
local FONT_TINY = 3

local function format_clock(now)
 return string.format("%02d:%02d:%02d", now.hours, now.minutes, now.seconds)
end

function APP:init(descriptor)
 self.tick = 0
 self.last_status = "ENT:Ping TAB:Keys"

 print("Lua app init", APP_METADATA.name, sys.version(), host.source_path or "(memory)")

 -- Example: inspect the SD root.
 -- local entries, err = fs.list("/")
 -- if entries then
 -- for _, entry in ipairs(entries) do
 -- print(entry.name, entry.is_dir and "dir" or entry.size)
 -- end
 -- else
 -- print("fs.list failed", err)
 -- end

 -- Example: read this file back from SD.
 -- local source, err = fs.read(host.source_path)
 -- if source then print("source bytes", #source) end

 -- Example: prepare the built-in T9 engine if your app uses it.
 -- t9.reset()
 -- t9.setText("hello")

 -- Example: inspect heap and PSRAM availability.
 -- local mem = sys.memInfo()
 -- print("heap free", mem.heap_free, "psram free", mem.psram_free)
end

function APP:update()
 self.tick = self.tick + 1

 -- Example if you keep a custom T9 text field in your app:
 -- t9.update()
end

function APP:draw(descriptor)
 local now = sys.time()

 gfx.clear()
 gfx.setColor(1)
 gfx.setFont(FONT_SMALL)

 ui.header(APP_METADATA.name, sys.version())

 gfx.text(2, 22, "Template rev " .. sys.version())
 gfx.text(2, 30, "Time " .. format_clock(now))
 gfx.text(2, 38, "Tick " .. tostring(self.tick))
 gfx.text(2, 46, self.last_status)

 -- Example drawing primitives.
 gfx.rect(96, 18, 24, 18)
 gfx.line(96, 36, 120, 18)
 gfx.pixel(108, 27)

 ui.footer("ENT:Ping", "ALT+ESC:Exit")
end

function APP:input(key)
 if key == input.KEY_ENTER then
  self.last_status = "Hello from " .. APP_METADATA.name
  host.notice(self.last_status, 1200)
 elseif key == input.KEY_TAB then
  local held = input.getKeys()
  self.last_status = "Held keys: " .. tostring(#held)
  print("held keys", #held, sys.timeStr())
 elseif key == "1" then
  -- Example SD write:
  -- local ok, err = fs.write("/example.txt", "Hello from " .. APP_METADATA.name .. "\\n")
  -- if not ok then print("fs.write failed", err) end
  self.last_status = "See comment examples"
 elseif key == "2" then
  -- Example viewer launch from Lua:
  -- ui.viewFile(host.source_path, APP_METADATA.name)
  self.last_status = "ui.viewFile example"
 elseif key == "3" then
  -- Example RW editor launch from Lua:
  -- ui.editFile(host.source_path, APP_METADATA.name)
  self.last_status = "ui.editFile example"
 elseif key == "4" then
  -- Example modal helpers:
  -- ui.message("Hello", "OK")
  -- ui.confirm("Leave?", true)
  self.last_status = "ui helpers example"
 end
end

-- Required desktop schema:
-- APP_METADATA.desktop = true
-- APP_METADATA.name = "Visible name"
-- APP_METADATA.icon = {13 text rows with spaces and # pixels}
-- APP = {} with callback methods as needed

-- Custom firmware bindings available in desktop apps:
-- gfx.clear/send/pixel/line/rect/fillRect/circle/fillCircle/text/setFont/setColor/width/height
-- input.pressed/held/anyPressed/getKeys plus input.KEY_ESC/BKSP/TAB/ENTER/SHIFT/ALT/UP/DOWN/LEFT/RIGHT
-- sys.millis/delay/yield/time/timeStr/version and global print(...)
-- fs.list/read/write
-- ui.header/footer/confirm/message/choice3/viewFile/editFile/takeEditorResult
-- t9.reset/input/update/getText/setText/getCursorByte/getCursorChar/setCursor/isPending/isShifted/commit/getCharCount
-- host.close(), host.notice(message, duration_ms), host.source_path
)LUA";

}  // namespace

String buildLuaBlankAppTemplateText() {
    String text;
    text.reserve(sizeof(kLuaBlankAppTemplateBody) + 96);
    text += "-- ESP32 Handheld blank Lua app template\n";
    text += "-- TEMPLATE_REV: ";
    text += FIRMWARE_VERSION;
    text += "\n";
    text += "-- Generated from Settings > New blank .lua app\n\n";
    text += kLuaBlankAppTemplateBody;
    return text;
}