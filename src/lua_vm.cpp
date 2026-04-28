// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/lua_vm.cpp
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TRUTH_HARDWARE.md Section 0.1, Section 1, Section 3; TACTICAL_TODO TASK_1
// LOG_REF: 2026-04-28

#include "lua_vm.h"
#include "hal.h"
#include "config.h"
#include "clock.h"
#include "gui.h"
#include "t9_engine.h"
#include <lua.hpp>

namespace LuaVM {

// --------------------------------------------------------------------------
// INTERNAL STATE
// --------------------------------------------------------------------------

static lua_State* L = nullptr;
static String lastError = "";

// Lua error handler that appends a stack traceback
static int luaTraceback(lua_State* L) {
    const char* msg = lua_tostring(L, 1);
    if (msg) {
        luaL_traceback(L, L, msg, 1);
    } else {
        lua_pushliteral(L, "(no error message)");
    }
    return 1;
}

struct SdSessionGuard {
    bool active = false;

    bool begin() {
        active = sdBeginSession();
        return active;
    }

    ~SdSessionGuard() {
        if (active) {
            sdEndSession();
        }
    }
};

static int pushLuaResultError(lua_State* L, const String& message) {
    lua_pushnil(L);
    lua_pushlstring(L, message.c_str(), message.length());
    return 2;
}

static String normalizeFsPath(const char* rawPath) {
    String path = rawPath ? rawPath : "/";
    if (path.length() == 0) {
        path = "/";
    }
    if (!path.startsWith("/")) {
        path = "/" + path;
    }
    while (path.length() > 1 && path.endsWith("/")) {
        path.remove(path.length() - 1);
    }
    return path;
}

static String formatFatDate(uint16_t fatDate) {
    if (fatDate == 0) {
        return "";
    }

    const uint16_t year = 1980 + ((fatDate >> 9) & 0x7F);
    const uint16_t month = (fatDate >> 5) & 0x0F;
    const uint16_t day = fatDate & 0x1F;

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u", year, month, day);
    return String(buffer);
}

static String formatFatTime(uint16_t fatTime) {
    if (fatTime == 0) {
        return "";
    }

    const uint16_t hour = (fatTime >> 11) & 0x1F;
    const uint16_t minute = (fatTime >> 5) & 0x3F;

    char buffer[8];
    snprintf(buffer, sizeof(buffer), "%02u:%02u", hour, minute);
    return String(buffer);
}

static String formatFatCompactDateTime(uint16_t fatDate, uint16_t fatTime) {
    if (fatDate == 0) {
        return "";
    }

    const uint16_t month = (fatDate >> 5) & 0x0F;
    const uint16_t day = fatDate & 0x1F;
    const uint16_t hour = (fatTime >> 11) & 0x1F;
    const uint16_t minute = (fatTime >> 5) & 0x3F;

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02u-%02u %02u:%02u", month, day, hour, minute);
    return String(buffer);
}

static String formatFatFullDateTime(uint16_t fatDate, uint16_t fatTime) {
    if (fatDate == 0) {
        return "";
    }

    const uint16_t year = 1980 + ((fatDate >> 9) & 0x7F);
    const uint16_t month = (fatDate >> 5) & 0x0F;
    const uint16_t day = fatDate & 0x1F;
    const uint16_t hour = (fatTime >> 11) & 0x1F;
    const uint16_t minute = (fatTime >> 5) & 0x3F;

    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u %02u:%02u", year, month, day, hour, minute);
    return String(buffer);
}

// --------------------------------------------------------------------------
// LUA BINDINGS - Display Functions
// --------------------------------------------------------------------------

// gfx.clear() - Clear the display buffer
static int lua_gfx_clear(lua_State* L) {
    u8g2.clearBuffer();
    return 0;
}

// gfx.send() - Send buffer to display
static int lua_gfx_send(lua_State* L) {
    u8g2.sendBuffer();
    return 0;
}

// gfx.pixel(x, y) - Draw a pixel
static int lua_gfx_pixel(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    u8g2.drawPixel(x, y);
    return 0;
}

// gfx.line(x1, y1, x2, y2) - Draw a line
static int lua_gfx_line(lua_State* L) {
    int x1 = luaL_checkinteger(L, 1);
    int y1 = luaL_checkinteger(L, 2);
    int x2 = luaL_checkinteger(L, 3);
    int y2 = luaL_checkinteger(L, 4);
    u8g2.drawLine(x1, y1, x2, y2);
    return 0;
}

// gfx.rect(x, y, w, h) - Draw rectangle outline
static int lua_gfx_rect(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int w = luaL_checkinteger(L, 3);
    int h = luaL_checkinteger(L, 4);
    u8g2.drawFrame(x, y, w, h);
    return 0;
}

// gfx.fillRect(x, y, w, h) - Draw filled rectangle
static int lua_gfx_fillRect(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int w = luaL_checkinteger(L, 3);
    int h = luaL_checkinteger(L, 4);
    u8g2.drawBox(x, y, w, h);
    return 0;
}

// gfx.circle(x, y, r) - Draw circle outline
static int lua_gfx_circle(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int r = luaL_checkinteger(L, 3);
    u8g2.drawCircle(x, y, r);
    return 0;
}

// gfx.fillCircle(x, y, r) - Draw filled circle
static int lua_gfx_fillCircle(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int r = luaL_checkinteger(L, 3);
    u8g2.drawDisc(x, y, r);
    return 0;
}

// gfx.text(x, y, str) - Draw text at position
static int lua_gfx_text(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    const char* str = luaL_checkstring(L, 3);
    u8g2.drawStr(x, y, str);
    return 0;
}

// gfx.setFont(size) - Set font size (0=small, 1=medium, 2=large, 3=tiny)
static int lua_gfx_setFont(lua_State* L) {
    int size = luaL_optinteger(L, 1, 0);
    switch (size) {
        case 0: u8g2.setFont(u8g2_font_5x7_tf); break;
        case 1: u8g2.setFont(u8g2_font_6x10_tf); break;
        case 2: u8g2.setFont(u8g2_font_8x13_tf); break;
        case 3: u8g2.setFont(u8g2_font_4x6_tf); break;
        default: u8g2.setFont(u8g2_font_5x7_tf); break;
    }
    return 0;
}

// gfx.setColor(color) - Set draw color (0=black, 1=white)
static int lua_gfx_setColor(lua_State* L) {
    int color = luaL_checkinteger(L, 1);
    u8g2.setDrawColor(color);
    return 0;
}

// gfx.width() - Get display width
static int lua_gfx_width(lua_State* L) {
    lua_pushinteger(L, u8g2.getDisplayWidth());
    return 1;
}

// gfx.height() - Get display height
static int lua_gfx_height(lua_State* L) {
    lua_pushinteger(L, u8g2.getDisplayHeight());
    return 1;
}

// Register all gfx functions
static void registerGfxModule(lua_State* L) {
    static const luaL_Reg gfx_funcs[] = {
        {"clear", lua_gfx_clear},
        {"send", lua_gfx_send},
        {"pixel", lua_gfx_pixel},
        {"line", lua_gfx_line},
        {"rect", lua_gfx_rect},
        {"fillRect", lua_gfx_fillRect},
        {"circle", lua_gfx_circle},
        {"fillCircle", lua_gfx_fillCircle},
        {"text", lua_gfx_text},
        {"setFont", lua_gfx_setFont},
        {"setColor", lua_gfx_setColor},
        {"width", lua_gfx_width},
        {"height", lua_gfx_height},
        {NULL, NULL}
    };
    
    luaL_newlib(L, gfx_funcs);
    lua_setglobal(L, "gfx");
}

// --------------------------------------------------------------------------
// LUA BINDINGS - Input Functions
// --------------------------------------------------------------------------

// input.pressed(key) - Check if key was just pressed this frame
static int lua_input_pressed(lua_State* L) {
    const char* keyStr = luaL_checkstring(L, 1);
    if (strlen(keyStr) > 0) {
        lua_pushboolean(L, isJustPressed(keyStr[0]));
    } else {
        lua_pushboolean(L, false);
    }
    return 1;
}

// input.held(key) - Check if key is currently held
static int lua_input_held(lua_State* L) {
    const char* keyStr = luaL_checkstring(L, 1);
    if (strlen(keyStr) > 0) {
        char key = keyStr[0];
        for (int i = 0; i < activeKeyCount; i++) {
            if (activeKeys[i] == key) {
                lua_pushboolean(L, true);
                return 1;
            }
        }
    }
    lua_pushboolean(L, false);
    return 1;
}

// input.anyPressed() - Check if any key was just pressed
static int lua_input_anyPressed(lua_State* L) {
    // Check if any key transitioned from not pressed to pressed
    for (int i = 0; i < activeKeyCount; i++) {
        if (isJustPressed(activeKeys[i])) {
            lua_pushboolean(L, true);
            return 1;
        }
    }
    lua_pushboolean(L, false);
    return 1;
}

// input.getKeys() - Get table of currently pressed keys
static int lua_input_getKeys(lua_State* L) {
    lua_newtable(L);
    for (int i = 0; i < activeKeyCount; i++) {
        char keyStr[2] = {activeKeys[i], '\0'};
        lua_pushstring(L, keyStr);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// Register all input functions
static void registerInputModule(lua_State* L) {
    static const luaL_Reg input_funcs[] = {
        {"pressed", lua_input_pressed},
        {"held", lua_input_held},
        {"anyPressed", lua_input_anyPressed},
        {"getKeys", lua_input_getKeys},
        {NULL, NULL}
    };
    
    luaL_newlib(L, input_funcs);
    
    // Add key constants as input.KEY_* for new keyboard layout
    // Special keys use ASCII control codes to avoid conflicts
    lua_pushstring(L, "\x1b"); lua_setfield(L, -2, "KEY_ESC");    // 27 = ESC
    lua_pushstring(L, "\x08"); lua_setfield(L, -2, "KEY_BKSP");   // 8 = Backspace
    lua_pushstring(L, "\x09"); lua_setfield(L, -2, "KEY_TAB");    // 9 = Tab
    lua_pushstring(L, "\x0d"); lua_setfield(L, -2, "KEY_ENTER");  // 13 = Enter/CR
    lua_pushstring(L, "\x0e"); lua_setfield(L, -2, "KEY_SHIFT");  // 14 = Shift
    lua_pushstring(L, "\x0f"); lua_setfield(L, -2, "KEY_ALT");    // 15 = Alt
    lua_pushstring(L, "\x10"); lua_setfield(L, -2, "KEY_UP");     // 16 = Up arrow
    lua_pushstring(L, "\x11"); lua_setfield(L, -2, "KEY_DOWN");   // 17 = Down arrow
    lua_pushstring(L, "\x12"); lua_setfield(L, -2, "KEY_LEFT");   // 18 = Left arrow
    lua_pushstring(L, "\x13"); lua_setfield(L, -2, "KEY_RIGHT");  // 19 = Right arrow
    
    lua_setglobal(L, "input");
}

// --------------------------------------------------------------------------
// LUA BINDINGS - System Functions
// --------------------------------------------------------------------------

// sys.millis() - Get milliseconds since boot
static int lua_sys_millis(lua_State* L) {
    lua_pushinteger(L, millis());
    return 1;
}

// sys.delay(ms) - Delay for milliseconds
static int lua_sys_delay(lua_State* L) {
    int ms = luaL_checkinteger(L, 1);
    delay(ms);
    return 0;
}

// sys.yield() - Yield to other tasks (call in loops)
static int lua_sys_yield(lua_State* L) {
    yield();
    return 0;
}

// sys.time() - Get system clock time as table {hours, minutes, seconds}
static int lua_sys_time(lua_State* L) {
    lua_newtable(L);
    lua_pushinteger(L, SystemClock::getHours());
    lua_setfield(L, -2, "hours");
    lua_pushinteger(L, SystemClock::getMinutes());
    lua_setfield(L, -2, "minutes");
    lua_pushinteger(L, SystemClock::getSeconds());
    lua_setfield(L, -2, "seconds");
    return 1;
}

// sys.timeStr() - Get formatted time string "HH:MM:SS"
static int lua_sys_timeStr(lua_State* L) {
    char buf[16];
    SystemClock::getFullTimeString(buf, sizeof(buf));
    lua_pushstring(L, buf);
    return 1;
}

// sys.version() - Get firmware version string
static int lua_sys_version(lua_State* L) {
    lua_pushstring(L, FIRMWARE_VERSION);
    return 1;
}

// print(str) - Print to serial
static int lua_print(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; i++) {
        if (i > 1) Serial.print("\t");
        if (lua_isstring(L, i)) {
            Serial.print(lua_tostring(L, i));
        } else if (lua_isnil(L, i)) {
            Serial.print("nil");
        } else if (lua_isboolean(L, i)) {
            Serial.print(lua_toboolean(L, i) ? "true" : "false");
        } else {
            Serial.print(luaL_typename(L, i));
        }
    }
    Serial.println();
    return 0;
}

// Register all system functions
static void registerSysModule(lua_State* L) {
    static const luaL_Reg sys_funcs[] = {
        {"millis", lua_sys_millis},
        {"delay", lua_sys_delay},
        {"yield", lua_sys_yield},
        {"time", lua_sys_time},
        {"timeStr", lua_sys_timeStr},
        {"version", lua_sys_version},
        {NULL, NULL}
    };
    
    luaL_newlib(L, sys_funcs);
    lua_setglobal(L, "sys");
    
    // Also register print as global
    lua_pushcfunction(L, lua_print);
    lua_setglobal(L, "print");
}

// --------------------------------------------------------------------------
// LUA BINDINGS - SD Filesystem Functions
// --------------------------------------------------------------------------

// fs.list(path) - Return array of {name, path, is_dir, size, modified}
static int lua_fs_list(lua_State* L) {
    String path = normalizeFsPath(luaL_optstring(L, 1, "/"));
    if (!isSDMounted()) {
        return pushLuaResultError(L, "SD not mounted");
    }

    SdSessionGuard session;
    FsFile dir;
    FsFile entry;
    if (!session.begin()) {
        return pushLuaResultError(L, "Failed to open SD session");
    }
    if (!dir.open(path.c_str(), O_RDONLY) || !dir.isDir()) {
        return pushLuaResultError(L, String("Failed to open directory: ") + path);
    }

    lua_newtable(L);
    dir.rewind();

    int index = 1;
    char name[128];
    while (entry.openNext(&dir, O_RDONLY)) {
        size_t nameLen = entry.getName(name, sizeof(name));
        if (nameLen > 0 && strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
            lua_newtable(L);

            lua_pushstring(L, name);
            lua_setfield(L, -2, "name");

            String childPath = path;
            if (childPath != "/") {
                childPath += "/";
            }
            childPath += name;
            lua_pushlstring(L, childPath.c_str(), childPath.length());
            lua_setfield(L, -2, "path");

            bool isDir = entry.isDir();
            lua_pushboolean(L, isDir);
            lua_setfield(L, -2, "is_dir");

            uint64_t entrySize = isDir ? 0 : entry.size();
            lua_pushinteger(L, static_cast<lua_Integer>(entrySize));
            lua_setfield(L, -2, "size");

            uint16_t fatDate = 0;
            uint16_t fatTime = 0;
            const bool hasModified = entry.getModifyDateTime(&fatDate, &fatTime);
            const String modified = hasModified ? formatFatDate(fatDate) : "";
            const String modifiedTime = hasModified ? formatFatTime(fatTime) : "";
            const String modifiedShort = hasModified ? formatFatCompactDateTime(fatDate, fatTime) : "";
            const String modifiedFull = hasModified ? formatFatFullDateTime(fatDate, fatTime) : "";
            lua_pushlstring(L, modified.c_str(), modified.length());
            lua_setfield(L, -2, "modified");

            lua_pushlstring(L, modifiedTime.c_str(), modifiedTime.length());
            lua_setfield(L, -2, "modified_time");

            lua_pushlstring(L, modifiedShort.c_str(), modifiedShort.length());
            lua_setfield(L, -2, "modified_short");

            lua_pushlstring(L, modifiedFull.c_str(), modifiedFull.length());
            lua_setfield(L, -2, "modified_full");

            lua_rawseti(L, -2, index++);
        }
        entry.close();
    }

    if (dir.getError()) {
        lua_pop(L, 1);
        return pushLuaResultError(L, String("Directory read failed: ") + path);
    }
    return 1;
}

// fs.read(path) - Read an entire text file into a Lua string
static int lua_fs_read(lua_State* L) {
    String path = normalizeFsPath(luaL_checkstring(L, 1));
    if (!isSDMounted()) {
        return pushLuaResultError(L, "SD not mounted");
    }

    SdSessionGuard session;
    FsFile file;
    if (!session.begin()) {
        return pushLuaResultError(L, "Failed to open SD session");
    }
    if (!file.open(path.c_str(), O_RDONLY)) {
        return pushLuaResultError(L, String("Failed to open file: ") + path);
    }
    if (file.isDir()) {
        return pushLuaResultError(L, String("Path is a directory: ") + path);
    }

    uint64_t fileSize = file.size();
    if (fileSize > 262144ULL) {
        return pushLuaResultError(L, String("File too large for Lua read: ") + path);
    }

    String content;
    if (fileSize > 0 && !content.reserve(static_cast<unsigned int>(fileSize))) {
        return pushLuaResultError(L, String("Not enough memory to read file: ") + path);
    }

    char buffer[129];
    while (true) {
        int bytesRead = file.read(buffer, sizeof(buffer) - 1);
        if (bytesRead < 0) {
            return pushLuaResultError(L, String("Failed to read file: ") + path);
        }
        if (bytesRead == 0) {
            break;
        }
        buffer[bytesRead] = '\0';
        if (!content.concat(buffer)) {
            return pushLuaResultError(L, String("Failed to append file data: ") + path);
        }
    }

    lua_pushlstring(L, content.c_str(), content.length());
    return 1;
}

static void registerFsModule(lua_State* L) {
    static const luaL_Reg fs_funcs[] = {
        {"list", lua_fs_list},
        {"read", lua_fs_read},
        {NULL, NULL}
    };

    luaL_newlib(L, fs_funcs);
    lua_setglobal(L, "fs");
}

// --------------------------------------------------------------------------
// LUA BINDINGS - UI Helpers
// --------------------------------------------------------------------------

// ui.header(title, rightText) - Draw standard app header
static int lua_ui_header(lua_State* L) {
    const char* title = luaL_checkstring(L, 1);
    const char* rightText = lua_isnoneornil(L, 2) ? nullptr : luaL_checkstring(L, 2);
    GUI::drawHeader(title, rightText);
    return 0;
}

// ui.footer(leftHint, rightHint) - Draw standard footer hints
static int lua_ui_footer(lua_State* L) {
    const char* leftHint = luaL_checkstring(L, 1);
    const char* rightHint = lua_isnoneornil(L, 2) ? nullptr : luaL_checkstring(L, 2);
    GUI::drawFooterHints(leftHint, rightHint);
    return 0;
}

// ui.confirm(message, yesSelected) - Draw shared yes/no confirmation dialog
static int lua_ui_confirm(lua_State* L) {
    const char* message = luaL_checkstring(L, 1);
    const bool yesSelected = lua_toboolean(L, 2);
    GUI::drawYesNoDialog(message, yesSelected);
    return 0;
}

static void registerUiModule(lua_State* L) {
    static const luaL_Reg ui_funcs[] = {
        {"header", lua_ui_header},
        {"footer", lua_ui_footer},
        {"confirm", lua_ui_confirm},
        {NULL, NULL}
    };

    luaL_newlib(L, ui_funcs);
    lua_setglobal(L, "ui");
}

// --------------------------------------------------------------------------
// LUA BINDINGS - T9 Engine Functions
// --------------------------------------------------------------------------

// t9.reset() - Reset engine state
static int lua_t9_reset(lua_State* L) {
    engine.reset();
    return 0;
}

// t9.input(key) - Feed a key char to T9 engine
static int lua_t9_input(lua_State* L) {
    const char* keyStr = luaL_checkstring(L, 1);
    if (strlen(keyStr) > 0) {
        engine.handleInput(keyStr[0]);
    }
    return 0;
}

// t9.update() - Call per frame (commits pending char on timeout)
static int lua_t9_update(lua_State* L) {
    engine.update();
    return 0;
}

// t9.getText() - Return current text buffer
static int lua_t9_getText(lua_State* L) {
    lua_pushstring(L, engine.textBuffer.c_str());
    return 1;
}

// t9.setText(str) - Set text buffer content
static int lua_t9_setText(lua_State* L) {
    const char* str = luaL_checkstring(L, 1);
    engine.textBuffer = str;
    return 0;
}

// t9.getCursorByte() - Return cursor byte position
static int lua_t9_getCursorByte(lua_State* L) {
    lua_pushinteger(L, engine.cursorPos);
    return 1;
}

// t9.getCursorChar() - Return cursor character position (UTF-8 aware)
static int lua_t9_getCursorChar(lua_State* L) {
    // Count UTF-8 characters up to cursorPos bytes
    const char* str = engine.textBuffer.c_str();
    int charCount = 0;
    int byteIdx = 0;
    while (byteIdx < engine.cursorPos && str[byteIdx] != '\0') {
        uint8_t c = (uint8_t)str[byteIdx];
        if (c < 0x80) byteIdx += 1;
        else if ((c & 0xE0) == 0xC0) byteIdx += 2;
        else if ((c & 0xF0) == 0xE0) byteIdx += 3;
        else byteIdx += 4;
        charCount++;
    }
    lua_pushinteger(L, charCount);
    return 1;
}

// t9.setCursor(pos) - Set cursor to character position
static int lua_t9_setCursor(lua_State* L) {
    int pos = luaL_checkinteger(L, 1);
    engine.setCursor(pos);
    return 0;
}

// t9.isPending() - Return bool: is there a pending multitap char
static int lua_t9_isPending(lua_State* L) {
    lua_pushboolean(L, engine.pendingCommit);
    return 1;
}

// t9.isShifted() - Return bool: shift/caps state
static int lua_t9_isShifted(lua_State* L) {
    lua_pushboolean(L, engine.getIsShifted());
    return 1;
}

// t9.commit() - Force commit pending character
static int lua_t9_commit(lua_State* L) {
    engine.commit();
    return 0;
}

// t9.getCharCount() - Return total character count (UTF-8 aware)
static int lua_t9_getCharCount(lua_State* L) {
    const char* str = engine.textBuffer.c_str();
    int charCount = 0;
    int byteIdx = 0;
    while (str[byteIdx] != '\0') {
        uint8_t c = (uint8_t)str[byteIdx];
        if (c < 0x80) byteIdx += 1;
        else if ((c & 0xE0) == 0xC0) byteIdx += 2;
        else if ((c & 0xF0) == 0xE0) byteIdx += 3;
        else byteIdx += 4;
        charCount++;
    }
    lua_pushinteger(L, charCount);
    return 1;
}

// Register all T9 functions
static void registerT9Module(lua_State* L) {
    static const luaL_Reg t9_funcs[] = {
        {"reset", lua_t9_reset},
        {"input", lua_t9_input},
        {"update", lua_t9_update},
        {"getText", lua_t9_getText},
        {"setText", lua_t9_setText},
        {"getCursorByte", lua_t9_getCursorByte},
        {"getCursorChar", lua_t9_getCursorChar},
        {"setCursor", lua_t9_setCursor},
        {"isPending", lua_t9_isPending},
        {"isShifted", lua_t9_isShifted},
        {"commit", lua_t9_commit},
        {"getCharCount", lua_t9_getCharCount},
        {NULL, NULL}
    };
    
    luaL_newlib(L, t9_funcs);
    lua_setglobal(L, "t9");
}

// --------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION
// --------------------------------------------------------------------------

bool init() {
    if (L != nullptr) {
        return true; // Already initialized
    }
    
    L = luaL_newstate();
    if (L == nullptr) {
        lastError = "Failed to create Lua state: not enough memory";
        return false;
    }
    
    // Open standard libraries (math, string, table, etc.)
    luaL_openlibs(L);
    
    // Register our custom modules
    registerGfxModule(L);
    registerInputModule(L);
    registerSysModule(L);
    registerFsModule(L);
    registerUiModule(L);
    registerT9Module(L);
    
    lastError = "";
    Serial.println("[LuaVM] Initialized successfully");
    return true;
}

void shutdown() {
    if (L != nullptr) {
        lua_close(L);
        L = nullptr;
        Serial.println("[LuaVM] Shut down");
    }
}

bool isReady() {
    return L != nullptr;
}

bool executeString(const char* script, const char* name) {
    if (L == nullptr) {
        lastError = "Lua VM not initialized";
        return false;
    }
    
    int result = luaL_loadbuffer(L, script, strlen(script), name);
    if (result != LUA_OK) {
        lastError = lua_tostring(L, -1);
        lua_pop(L, 1);
        return false;
    }
    
    lua_pushcfunction(L, luaTraceback);
    lua_insert(L, -2);  // put error handler below chunk
    result = lua_pcall(L, 0, LUA_MULTRET, lua_gettop(L) - 1);
    if (result != LUA_OK) {
        lastError = lua_tostring(L, -1);
        lua_pop(L, 2);  // pop error + error handler
        return false;
    }
    lua_remove(L, 1);  // remove error handler
    
    lastError = "";
    return true;
}

const char* getLastError() {
    return lastError.c_str();
}

void clearError() {
    lastError = "";
}

size_t getMemoryUsage() {
    if (L == nullptr) return 0;
    return lua_gc(L, LUA_GCCOUNT, 0) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0);
}

void collectGarbage() {
    if (L != nullptr) {
        lua_gc(L, LUA_GCCOLLECT, 0);
    }
}

// --------------------------------------------------------------------------
// COOPERATIVE FRAME-LOOP API
// --------------------------------------------------------------------------

bool hasFunction(const char* funcName) {
    if (L == nullptr) return false;
    lua_getglobal(L, funcName);
    bool exists = lua_isfunction(L, -1);
    lua_pop(L, 1);
    return exists;
}

bool callGlobalFunction(const char* funcName) {
    if (L == nullptr) {
        lastError = "Lua VM not initialized";
        return false;
    }
    
    lua_pushcfunction(L, luaTraceback);  // push error handler
    int errIdx = lua_gettop(L);
    
    lua_getglobal(L, funcName);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);  // pop non-function + error handler
        return true;  // Function doesn't exist — not an error
    }
    
    int result = lua_pcall(L, 0, 0, errIdx);
    if (result != LUA_OK) {
        lastError = lua_tostring(L, -1);
        lua_pop(L, 2);  // pop error + error handler
        return false;
    }
    lua_pop(L, 1);  // pop error handler
    return true;
}

bool callInputHandler(char key) {
    if (L == nullptr) {
        lastError = "Lua VM not initialized";
        return false;
    }
    
    lua_pushcfunction(L, luaTraceback);  // push error handler
    int errIdx = lua_gettop(L);
    
    lua_getglobal(L, "_input");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);  // pop non-function + error handler
        return true;  // _input doesn't exist — not an error
    }
    
    char keyStr[2] = {key, '\0'};
    lua_pushstring(L, keyStr);
    
    int result = lua_pcall(L, 1, 0, errIdx);
    if (result != LUA_OK) {
        lastError = lua_tostring(L, -1);
        lua_pop(L, 2);  // pop error + error handler
        return false;
    }
    lua_pop(L, 1);  // pop error handler
    return true;
}

} // namespace LuaVM
