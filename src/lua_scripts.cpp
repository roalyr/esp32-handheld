// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/lua_scripts.cpp
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TRUTH_HARDWARE.md Section 1, Section 2, Section 3; TACTICAL_TODO TASK_2
// LOG_REF: 2026-04-28

#include "lua_scripts.h"

const char LUA_DESKTOP[] = R"LUA(
local GRID_COLS = 4
local GRID_ROWS = 2
local PAGE_SIZE = GRID_COLS * GRID_ROWS
local TILE_W = 32
local TILE_H = 20
local DESKTOP_TOP = 13
local TILE_PAD_X = 2
local ICON_BOX_W = 28
local ICON_BOX_H = 13
local ICON_TARGET_W = 28
local ICON_TARGET_H = 13
local LABEL_BOX_Y = 14
local LABEL_BOX_W = 28
local LABEL_BASELINE_Y = 19
local LABEL_CHAR_CAP = 7
local LABEL_CHAR_W = 4

local FILE_VIEW_FULL = 1
local FILE_VIEW_DETAILS = 2
local FILE_VIEW_MIN = 3

local FONT_SMALL = 0
local FONT_MEDIUM = 1
local FONT_LARGE = 2
local FONT_TINY = 3

local unpack_fn = unpack or table.unpack

local FILE_BROWSER_ICON = {
    "    ################      ",
    "  ######################  ",
    " ######################## ",
    "##                      ##",
    "##  ######              ##",
    "##  #    #   ########   ##",
    "##  # ## #   #      #   ##",
    "##  #    #   # #### #   ##",
    "##  ######   #      #   ##",
    "##           ########   ##",
    "##                      ##",
    " ######################## ",
    "  ######################  "
}

local function clamp(value, low, high)
    if value < low then
        return low
    end
    if value > high then
        return high
    end
    return value
end

local function lower(value)
    return string.lower(value or "")
end

local function upper(value)
    return string.upper(value or "")
end

local function ends_with(value, suffix)
    if suffix == "" then
        return true
    end
    return value:sub(-#suffix) == suffix
end

local function truncate(value, limit)
    value = value or ""
    if #value <= limit then
        return value
    end
    if limit <= 1 then
        return value:sub(1, limit)
    end
    return value:sub(1, limit - 1) .. "~"
end

local function marquee_text(value, visible_chars, tick)
    value = value or ""
    if #value <= visible_chars then
        return value
    end

    local cycle_len = #value + 3
    local offset = math.floor((tick or 0) / 6) % cycle_len
    local extended = value .. "   " .. value
    return extended:sub(offset + 1, offset + visible_chars)
end

local function path_tail(path, limit)
    path = path or "/"
    if #path <= limit then
        return path
    end
    if limit <= 1 then
        return path:sub(-limit)
    end
    return "~" .. path:sub(-(limit - 1))
end

local function basename(path)
    if not path or path == "/" then
        return "/"
    end
    return path:match("([^/]+)$") or path
end

local function parent_path(path)
    if not path or path == "/" then
        return "/"
    end
    local parent = path:match("^(.*)/[^/]+$")
    if not parent or parent == "" then
        return "/"
    end
    return parent
end

local function format_size(bytes)
    bytes = tonumber(bytes) or 0
    if bytes < 1024 then
        return string.format("%dB", bytes)
    end
    if bytes < 1024 * 1024 then
        return string.format("%dK", math.floor((bytes + 512) / 1024))
    end
    return string.format("%.1fM", bytes / (1024 * 1024))
end

local function extension_of(name)
    if not name then
        return nil
    end
    return name:match("%.([^.]+)$")
end

local function entry_type_label(entry)
    if entry.is_parent then
        return "PARENT"
    end
    if entry.is_dir then
        return "DIR"
    end
    local ext = extension_of(entry.name)
    if ext and ext ~= "" then
        return upper(ext):sub(1, 4)
    end
    return "FILE"
end

local function entry_size_type_text(entry)
    local kind = entry_type_label(entry)
    if entry.is_parent or entry.is_dir then
        return kind
    end
    return truncate(kind .. " " .. format_size(entry.size), 10)
end

local function entry_modified_text(entry)
    if entry.is_parent then
        return "-"
    end
    local modified = entry.modified_full or entry.modified_short or entry.modified or ""
    if modified == "" then
        return "-"
    end
    return modified
end

local function normalize_icon_rows(rows)
    if type(rows) ~= "table" or #rows == 0 then
        return nil
    end

    local width = 0
    local height = #rows
    for index = 1, height do
        if type(rows[index]) ~= "string" then
            return nil
        end
        width = math.max(width, #rows[index])
    end

    if width <= 0 then
        return nil
    end

    local pixels = {}
    for y = 1, height do
        local row = rows[y]
        for x = 1, width do
            pixels[#pixels + 1] = row:sub(x, x) ~= " "
        end
    end

    return { width = width, height = height, pixels = pixels }
end

local function normalize_icon(icon)
    if type(icon) ~= "table" then
        return nil
    end

    if type(icon[1]) == "string" then
        return normalize_icon_rows(icon)
    end

    return nil
end

local function is_valid_icon(icon)
    return normalize_icon(icon) ~= nil
end

local function draw_fallback_icon(tile_x, tile_y)
    local box_w = 11
    local box_h = 11
    local draw_x = tile_x + TILE_PAD_X + math.floor((ICON_BOX_W - box_w) / 2)
    local draw_y = tile_y + math.floor((ICON_BOX_H - box_h) / 2)

    gfx.rect(draw_x, draw_y, box_w, box_h)
    gfx.setFont(FONT_SMALL)
    gfx.text(draw_x + 3, draw_y + 8, "?")
end

local function sort_entries(entries)
    table.sort(entries, function(left, right)
        if left.is_dir ~= right.is_dir then
            return left.is_dir and not right.is_dir
        end
        return lower(left.name) < lower(right.name)
    end)
end

local function draw_icon(icon, tile_x, tile_y)
    local normalized = normalize_icon(icon)
    if not normalized then
        draw_fallback_icon(tile_x, tile_y)
        return
    end

    local src_w = normalized.width
    local src_h = normalized.height
    local pixels = normalized.pixels

    local target_w = math.min(ICON_TARGET_W, src_w)
    local target_h = math.min(ICON_TARGET_H, src_h)
    local start_x = math.floor((src_w - target_w) / 2)
    local start_y = math.floor((src_h - target_h) / 2)
    local draw_x = tile_x + TILE_PAD_X + math.floor((ICON_BOX_W - target_w) / 2)
    local draw_y = tile_y + math.floor((ICON_BOX_H - target_h) / 2)

    for y = 0, target_h - 1 do
        for x = 0, target_w - 1 do
            local pixel = pixels[(start_y + y) * src_w + (start_x + x) + 1]
            if pixel == 1 or pixel == true then
                gfx.pixel(draw_x + x, draw_y + y)
            end
        end
    end
end

local function centered_label_x(tile_x, text)
    local text_w = math.min(LABEL_BOX_W, #text * LABEL_CHAR_W)
    return tile_x + TILE_PAD_X + math.floor((LABEL_BOX_W - text_w) / 2)
end

local function draw_notice(message)
    gfx.fillRect(10, 24, 108, 14)
    gfx.setColor(0)
    gfx.text(14, 33, truncate(message, 19))
    gfx.setColor(1)
    gfx.rect(10, 24, 108, 14)
end

local function traceback_message(err)
    return debug.traceback(tostring(err), 2)
end

local function wrap_text_lines(text, width)
    local lines = {}
    local normalized = tostring(text or ""):gsub("\r", "")

    for raw_line in (normalized .. "\n"):gmatch("(.-)\n") do
        if raw_line == "" then
            lines[#lines + 1] = ""
        else
            while #raw_line > width do
                lines[#lines + 1] = raw_line:sub(1, width)
                raw_line = raw_line:sub(width + 1)
            end
            lines[#lines + 1] = raw_line
        end
    end

    if #lines == 0 then
        lines[1] = "(no details)"
    end
    return lines
end

local function draw_crash_popup(title, lines, scroll)
    local popup_x = 4
    local popup_y = 4
    local popup_w = 120
    local popup_h = 56
    local body_y = popup_y + 16
    local visible_lines = 6

    gfx.setColor(0)
    gfx.fillRect(popup_x, popup_y, popup_w, popup_h)
    gfx.setColor(1)
    gfx.rect(popup_x, popup_y, popup_w, popup_h)
    gfx.setFont(FONT_SMALL)
    gfx.text(popup_x + 4, popup_y + 8, truncate(title, 18))
    gfx.line(popup_x + 1, popup_y + 10, popup_x + popup_w - 2, popup_y + 10)
    gfx.setFont(FONT_TINY)

    for row = 0, visible_lines - 1 do
        local index = scroll + row
        if index > #lines then
            break
        end
        gfx.text(popup_x + 3, body_y + row * 6, truncate(lines[index], 28))
    end

    gfx.text(popup_x + 4, popup_y + popup_h - 3, "UP/DN scroll BKSP close")

    if #lines > visible_lines then
        local bar_y = popup_y + 13
        local bar_h = 34
        local thumb_h = math.max(6, math.floor(bar_h * visible_lines / #lines))
        local max_scroll = #lines - visible_lines
        local thumb_y = bar_y
        if max_scroll > 0 then
            thumb_y = bar_y + math.floor((bar_h - thumb_h) * ((scroll - 1) / max_scroll))
        end
        gfx.line(popup_x + popup_w - 4, bar_y, popup_x + popup_w - 4, bar_y + bar_h)
        gfx.fillRect(popup_x + popup_w - 5, thumb_y, 3, thumb_h)
    end

    gfx.setColor(1)
    gfx.setFont(FONT_SMALL)
end

local function draw_scrollbar(total, visible, first_index, top_y, height)
    if total <= visible or visible <= 0 then
        return
    end
    gfx.rect(124, top_y, 3, height)
    local thumb_h = math.max(6, math.floor(height * visible / total))
    local max_scroll = total - visible
    local thumb_y = top_y
    if max_scroll > 0 then
        thumb_y = top_y + math.floor((height - thumb_h) * ((first_index - 1) / max_scroll))
    end
    gfx.fillRect(125, thumb_y + 1, 1, math.max(4, thumb_h - 2))
end

local function draw_file_list(entries, selected, scroll, top_y, visible_rows, line_height, label_width)
    if #entries == 0 then
        gfx.text(12, top_y + 8, "(empty)")
        return
    end

    for row = 0, visible_rows - 1 do
        local index = scroll + row
        if index > #entries then
            break
        end

        local entry = entries[index]
        local baseline = top_y + row * line_height
        local marker = entry.is_parent and "<" or (entry.is_dir and "/" or "-")

        if index == selected then
            gfx.fillRect(0, baseline - 7, 123, line_height)
            gfx.setColor(0)
        else
            gfx.setColor(1)
        end

        gfx.text(1, baseline, marker)
        gfx.text(8, baseline, truncate(entry.name, label_width))
        gfx.setColor(1)
    end

    draw_scrollbar(#entries, visible_rows, scroll, top_y - 7, visible_rows * line_height)
end

local function draw_two_column_list(entries, selected, scroll, top_y, visible_rows, line_height, left_width, right_x, right_width, right_renderer)
    if #entries == 0 then
        gfx.text(12, top_y + 8, "(empty)")
        return
    end

    for row = 0, visible_rows - 1 do
        local index = scroll + row
        if index > #entries then
            break
        end

        local entry = entries[index]
        local baseline = top_y + row * line_height
        local marker = entry.is_parent and "<" or (entry.is_dir and "/" or "-")

        if index == selected then
            gfx.fillRect(0, baseline - 7, 123, line_height)
            gfx.setColor(0)
        else
            gfx.setColor(1)
        end

        gfx.setFont(FONT_SMALL)
        gfx.text(1, baseline, marker)
        gfx.text(8, baseline, truncate(entry.name, left_width))
        gfx.setFont(FONT_TINY)
        gfx.text(right_x, baseline, truncate(right_renderer(entry), right_width))
        gfx.setFont(FONT_SMALL)
        gfx.setColor(1)
    end

    draw_scrollbar(#entries, visible_rows, scroll, top_y - 7, visible_rows * line_height)
end

local host = {
    apps = {},
    selected_index = 1,
    active_descriptor = nil,
    notice = nil,
    notice_until = 0,
    close_prompt_active = false,
    close_prompt_leave = false,
    crash_popup_active = false,
    crash_title = nil,
    crash_lines = {},
    crash_scroll = 1,
    marquee_tick = 0,
    catalog_poll_interval = 3000,
    last_catalog_poll = 0
}

local file_browser = {
    current_path = "/",
    entries = {},
    selected = 1,
    scroll = 1,
    view_mode = FILE_VIEW_FULL,
    toast = nil,
    toast_until = 0,
    selection_memory = {}
}

local BUILTIN_APPS = {
    {
        id = "builtin:file_browser",
        name = "Files",
        icon = FILE_BROWSER_ICON,
        app = file_browser,
        built_in = true
    }
}

function host:show_notice(message, duration_ms)
    self.notice = message
    self.notice_until = sys.millis() + (duration_ms or 1800)
end

function host:show_crash_popup(title, message)
    self.crash_popup_active = true
    self.crash_title = title or "App crash"
    self.crash_lines = wrap_text_lines(message, 28)
    self.crash_scroll = 1
end

function host:update_notice()
    if self.notice and sys.millis() >= self.notice_until then
        self.notice = nil
        self.notice_until = 0
    end
end

local function make_app_env(path)
    local env = {
        APP = nil,
        APP_METADATA = nil,
        host = {
            close = function()
                host:close_current_app()
            end,
            notice = function(message, duration_ms)
                host:show_notice(message, duration_ms)
            end,
            source_path = path
        }
    }
    return setmetatable(env, { __index = _G })
end

local function inspect_sd_app(path)
    local source, read_err = fs.read(path)
    if not source then
        return nil, nil, read_err
    end

    local env = make_app_env(path)
    local chunk, load_err = load(source, "@" .. path, "t", env)
    if not chunk then
        return nil, nil, load_err
    end

    local ok, runtime_err = xpcall(chunk, traceback_message)
    if not ok then
        return nil, nil, runtime_err
    end

    local metadata = env.APP_METADATA
    if type(metadata) ~= "table" or metadata.desktop ~= true then
        return nil, nil, nil
    end
    if type(metadata.name) ~= "string" or metadata.name == "" then
        return nil, nil, "missing APP_METADATA.name"
    end
    if not is_valid_icon(metadata.icon) then
        return nil, nil, "invalid APP_METADATA.icon"
    end
    if type(env.APP) ~= "table" then
        return nil, nil, "missing APP table"
    end

    return env, metadata, nil
end

local function load_sd_app_descriptor(path)
    local env, metadata, load_err = inspect_sd_app(path)
    if not env then
        return nil, load_err
    end

    return {
        id = path,
        name = metadata.name,
        icon = metadata.icon,
        source_path = path,
        built_in = false
    }
end

local function instantiate_sd_app(descriptor)
    local env, metadata, load_err = inspect_sd_app(descriptor.source_path)
    if not env then
        return nil, load_err or "Launch failed"
    end

    return {
        id = descriptor.id,
        name = metadata.name,
        icon = metadata.icon,
        source_path = descriptor.source_path,
        built_in = false,
        app = env.APP
    }, nil
end

function host:refresh_catalog(preserve_id)
    local next_apps = {}
    for _, descriptor in ipairs(BUILTIN_APPS) do
        next_apps[#next_apps + 1] = descriptor
    end

    local root_entries, list_err = fs.list("/")
    local discovered = {}
    if root_entries then
        for _, entry in ipairs(root_entries) do
            if not entry.is_dir and ends_with(lower(entry.name), ".lua") then
                local descriptor = load_sd_app_descriptor(entry.path)
                if descriptor then
                    discovered[#discovered + 1] = descriptor
                end
            end
        end

        table.sort(discovered, function(left, right)
            return lower(left.name) < lower(right.name)
        end)

        for _, descriptor in ipairs(discovered) do
            next_apps[#next_apps + 1] = descriptor
        end
    elseif list_err ~= "SD not mounted" and #self.apps > 0 then
        self.last_catalog_poll = sys.millis()
        return false
    end

    self.apps = next_apps
    if #self.apps == 0 then
        self.selected_index = 0
        return
    end

    local matched_index = nil
    if preserve_id then
        for index, descriptor in ipairs(self.apps) do
            if descriptor.id == preserve_id then
                matched_index = index
                break
            end
        end
    end

    if matched_index then
        self.selected_index = matched_index
    else
        self.selected_index = clamp(self.selected_index > 0 and self.selected_index or 1, 1, #self.apps)
    end

    self.last_catalog_poll = sys.millis()
end

function host:get_selected_descriptor()
    return self.apps[self.selected_index]
end

function host:invoke(descriptor, method_name, ...)
    local app = descriptor and descriptor.app
    local callback = app and app[method_name]
    if type(callback) ~= "function" then
        return true
    end

    local args = { ... }
    local function runner()
        return callback(app, unpack_fn(args))
    end

    local ok, result = xpcall(runner, traceback_message)
    if not ok then
        print("[desktop] app error: " .. tostring(result))
        self.active_descriptor = nil
        self:refresh_catalog(descriptor and descriptor.id or nil)
        self:show_crash_popup((descriptor and descriptor.name or "App") .. " crash", result)
        return false
    end
    return true, result
end

function host:launch(descriptor)
    self.close_prompt_active = false
    self.close_prompt_leave = false
    local runtime_descriptor = descriptor
    if not descriptor.built_in then
        local err
        runtime_descriptor, err = instantiate_sd_app(descriptor)
        if not runtime_descriptor then
            self:refresh_catalog(descriptor.id)
            self:show_crash_popup("Launch failed", err or "App could not start")
            return
        end
    end

    self.active_descriptor = runtime_descriptor
    if not self:invoke(runtime_descriptor, "init", descriptor) then
        self.active_descriptor = nil
    end
end

function host:close_current_app()
    local preserve_id = self.active_descriptor and self.active_descriptor.id or nil
    self.close_prompt_active = false
    self.close_prompt_leave = false
    self.active_descriptor = nil
    self:refresh_catalog(preserve_id)
end

function host:handle_crash_popup_input(key)
    local visible_lines = 6
    local max_scroll = math.max(1, #self.crash_lines - visible_lines + 1)

    if key == input.KEY_UP or key == input.KEY_LEFT then
        self.crash_scroll = clamp(self.crash_scroll - 1, 1, max_scroll)
    elseif key == input.KEY_DOWN or key == input.KEY_RIGHT then
        self.crash_scroll = clamp(self.crash_scroll + 1, 1, max_scroll)
    elseif key == input.KEY_ENTER or key == input.KEY_BKSP or key == input.KEY_ALT then
        self.crash_popup_active = false
        self.crash_title = nil
        self.crash_lines = {}
        self.crash_scroll = 1
    end
end

local function draw_desktop_tile(descriptor, x, y, selected)
    local label = selected and marquee_text(descriptor.name, LABEL_CHAR_CAP, host.marquee_tick)
        or truncate(descriptor.name, LABEL_CHAR_CAP)
    local label_x = centered_label_x(x, label)

    if selected then
        gfx.fillRect(x + TILE_PAD_X, y + LABEL_BOX_Y, LABEL_BOX_W, 6)
        gfx.setColor(1)
        draw_icon(descriptor.icon, x, y)
        gfx.setColor(0)
        gfx.setFont(FONT_TINY)
        gfx.text(label_x, y + LABEL_BASELINE_Y, label)
        gfx.setFont(FONT_SMALL)
    else
        gfx.setColor(1)
        draw_icon(descriptor.icon, x, y)
        gfx.setFont(FONT_TINY)
        gfx.text(label_x, y + LABEL_BASELINE_Y, label)
        gfx.setFont(FONT_SMALL)
    end
    gfx.setColor(1)
end

function host:draw_desktop()
    local total = #self.apps
    local selected = self.selected_index
    local right_text = total > 0 and string.format("%d/%d", selected, total) or "0/0"
    ui.header("Desktop", right_text)

    if total == 0 then
        gfx.text(18, 31, "No desktop apps")
    else
        local page = math.floor((selected - 1) / PAGE_SIZE)
        local first_index = page * PAGE_SIZE + 1
        local last_index = math.min(first_index + PAGE_SIZE - 1, total)
        for index = first_index, last_index do
            local slot = index - first_index
            local col = slot % GRID_COLS
            local row = math.floor(slot / GRID_COLS)
            local x = col * TILE_W
            local y = DESKTOP_TOP + row * TILE_H
            draw_desktop_tile(self.apps[index], x, y, index == selected)
        end
    end

    ui.footer("ENT:Open", "ESC:Settings")
end

function host:update()
    self.marquee_tick = self.marquee_tick + 1
    self:update_notice()
    if self.active_descriptor then
        self:invoke(self.active_descriptor, "update")
    else
        local now = sys.millis()
        if now - self.last_catalog_poll >= self.catalog_poll_interval then
            local preserve_id = self.apps[self.selected_index] and self.apps[self.selected_index].id or nil
            self:refresh_catalog(preserve_id)
        end
    end
end

function host:draw()
    gfx.clear()
    gfx.setFont(0)

    if self.active_descriptor then
        if not self:invoke(self.active_descriptor, "draw", self.active_descriptor) then
            self:draw_desktop()
        end
    else
        self:draw_desktop()
    end

    if self.notice then
        draw_notice(self.notice)
    end
    if self.close_prompt_active then
        ui.confirm("Leave app?", self.close_prompt_leave)
    end
    if self.crash_popup_active then
        draw_crash_popup(self.crash_title or "App crash", self.crash_lines, self.crash_scroll)
    end
    gfx.send()
end

function host:handle_close_prompt_input(key)
    if key == input.KEY_LEFT or key == input.KEY_RIGHT or key == input.KEY_UP or key == input.KEY_DOWN then
        self.close_prompt_leave = not self.close_prompt_leave
    elseif key == input.KEY_ENTER then
        if self.close_prompt_leave then
            self:close_current_app()
        else
            self.close_prompt_active = false
        end
    elseif key == input.KEY_ALT or key == input.KEY_BKSP then
        self.close_prompt_active = false
    end
end

function host:desktop_input(key)
    if #self.apps == 0 then
        return
    end

    if key == input.KEY_LEFT and self.selected_index > 1 then
        self.selected_index = self.selected_index - 1
    elseif key == input.KEY_RIGHT and self.selected_index < #self.apps then
        self.selected_index = self.selected_index + 1
    elseif key == input.KEY_UP and self.selected_index - GRID_COLS >= 1 then
        self.selected_index = self.selected_index - GRID_COLS
    elseif key == input.KEY_DOWN and self.selected_index + GRID_COLS <= #self.apps then
        self.selected_index = self.selected_index + GRID_COLS
    elseif key == input.KEY_ENTER then
        self:launch(self.apps[self.selected_index])
    end
end

function host:input(key)
    if self.crash_popup_active then
        self:handle_crash_popup_input(key)
        return
    end

    if self.active_descriptor then
        if self.close_prompt_active then
            self:handle_close_prompt_input(key)
            return
        end
        if key == input.KEY_ALT then
            self.close_prompt_active = true
            self.close_prompt_leave = false
            return
        end
        self:invoke(self.active_descriptor, "input", key)
        return
    end

    self:desktop_input(key)
end

function host:init()
    self.apps = {}
    self.selected_index = 1
    self.active_descriptor = nil
    self.notice = nil
    self.notice_until = 0
    self.close_prompt_active = false
    self.close_prompt_leave = false
    self.crash_popup_active = false
    self.crash_title = nil
    self.crash_lines = {}
    self.crash_scroll = 1
    self.marquee_tick = 0
    self.last_catalog_poll = 0
    self:refresh_catalog(nil)
end

function file_browser:show_toast(message, duration_ms)
    self.toast = message
    self.toast_until = sys.millis() + (duration_ms or 1500)
end

function file_browser:show_message(message, button_label)
    self.toast = nil
    self.toast_until = 0
    self.message_active = true
    self.message_text = message
    self.message_button = button_label or "OK"
end

function file_browser:clear_message()
    self.message_active = false
    self.message_text = nil
    self.message_button = nil
end

function file_browser:load_path(path)
    local listed_entries, err = fs.list(path)
    self.current_path = path
    self.entries = {}
    local remembered_path = self.selection_memory[path]

    if path ~= "/" then
        self.entries[#self.entries + 1] = {
            name = "..",
            path = parent_path(path),
            is_dir = true,
            is_parent = true,
            size = 0
        }
    end

    if not listed_entries then
        self.selected = #self.entries > 0 and 1 or 0
        self.scroll = 1
        if err == "SD not mounted" then
            self:show_message("No SD card mounted")
        else
            self:show_toast(err or "SD browse error", 2000)
        end
        return
    end

    sort_entries(listed_entries)
    for _, entry in ipairs(listed_entries) do
        self.entries[#self.entries + 1] = entry
    end

    self.selected = #self.entries > 0 and 1 or 0
    if remembered_path then
        for index, entry in ipairs(self.entries) do
            if entry.path == remembered_path then
                self.selected = index
                break
            end
        end
    end
    self.scroll = 1
end

function file_browser:selected_entry()
    return self.entries[self.selected]
end

function file_browser:ensure_visible(visible_rows)
    if self.selected <= 0 then
        self.scroll = 1
        return
    end
    if self.selected < self.scroll then
        self.scroll = self.selected
    end
    if self.selected > self.scroll + visible_rows - 1 then
        self.scroll = self.selected - visible_rows + 1
    end
    if self.scroll < 1 then
        self.scroll = 1
    end
end

function file_browser:init()
    self.view_mode = FILE_VIEW_FULL
    self.toast = nil
    self.toast_until = 0
    self.message_active = false
    self.message_text = nil
    self.message_button = nil
    self.selection_memory = {}
    self:load_path("/")
end

function file_browser:update()
    if self.toast and sys.millis() >= self.toast_until then
        self.toast = nil
        self.toast_until = 0
    end
end

function file_browser:move_selection(delta)
    if #self.entries == 0 then
        return
    end
    self.selected = clamp(self.selected + delta, 1, #self.entries)
end

function file_browser:remember_current_selection(include_parent)
    local entry = self:selected_entry()
    if not entry then
        return
    end
    if not include_parent and entry.is_parent then
        return
    end
    self.selection_memory[self.current_path] = entry.path
end

function file_browser:go_parent()
    if self.current_path == "/" then
        self:show_message("Already at root")
        return
    end
    local parent = parent_path(self.current_path)
    self:remember_current_selection(false)
    self.selection_memory[parent] = self.current_path
    self:load_path(parent)
end

function file_browser:open_selected()
    local entry = self:selected_entry()
    if not entry then
        return
    end
    if entry.is_parent then
        self:go_parent()
        return
    end
    if entry.is_dir then
        self:remember_current_selection(true)
        self:load_path(entry.path)
        return
    end
    local ok, err = ui.viewFile(entry.path, entry.name)
    if not ok then
        if err == "SD not mounted" then
            self:show_message("No SD card mounted")
        else
            self:show_toast(err or "Open failed", 2000)
        end
    end
end

function file_browser:handle_message_input(key)
    if key == input.KEY_ENTER or key == input.KEY_BKSP or key == input.KEY_ALT
        or key == input.KEY_LEFT or key == input.KEY_RIGHT
        or key == input.KEY_UP or key == input.KEY_DOWN then
        self:clear_message()
    end
end

function file_browser:detail_line()
    local entry = self:selected_entry()
    if not entry then
        return path_tail(self.current_path, 15)
    end
    if entry.is_parent then
        return "Parent " .. path_tail(entry.path, 12)
    end
    if entry.is_dir then
        return "DIR " .. path_tail(entry.path, 15)
    end
    return format_size(entry.size) .. " " .. truncate(entry.name, 10)
end

function file_browser:draw_full()
    ui.header("Files", path_tail(self.current_path, 10))
    self:ensure_visible(4)
    draw_file_list(self.entries, self.selected, self.scroll, 21, 4, 9, 18)
    ui.footer("ENT:Open", "TAB:Mode")
end

function file_browser:draw_details()
    ui.header("Files", path_tail(self.current_path, 10))
    self:ensure_visible(4)
    draw_two_column_list(self.entries, self.selected, self.scroll, 21, 4, 9, 7, 46, 16, entry_size_type_text)
    ui.footer("ENT:Open", "TAB:Mode")
end

function file_browser:draw_min()
    ui.header("Files", path_tail(self.current_path, 10))
    self:ensure_visible(4)
    draw_two_column_list(self.entries, self.selected, self.scroll, 21, 4, 9, 6, 44, 16, entry_modified_text)
    ui.footer("ENT:Open", "TAB:Mode")
end

function file_browser:draw()
    if self.view_mode == FILE_VIEW_FULL then
        self:draw_full()
    elseif self.view_mode == FILE_VIEW_DETAILS then
        self:draw_details()
    else
        self:draw_min()
    end

    if self.toast then
        draw_notice(self.toast)
    end
    if self.message_active then
        ui.message(self.message_text, self.message_button)
    end
end

function file_browser:input(key)
    if self.message_active then
        self:handle_message_input(key)
        return
    end

    if key == input.KEY_TAB then
        self.view_mode = self.view_mode + 1
        if self.view_mode > FILE_VIEW_MIN then
            self.view_mode = FILE_VIEW_FULL
        end
    elseif key == input.KEY_UP then
        self:move_selection(-1)
    elseif key == input.KEY_DOWN then
        self:move_selection(1)
    elseif key == input.KEY_LEFT or key == input.KEY_BKSP then
        self:go_parent()
    elseif key == input.KEY_ENTER then
        self:open_selected()
    end
end

function _init()
    host:init()
end

function _update()
    host:update()
end

function _draw()
    host:draw()
end

function _input(key)
    host:input(key)
end
)LUA";
