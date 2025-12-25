// [Revision: v1.2] [Path: src/lua_vm.cpp] [Date: 2025-12-11]
// Description: Lua VM implementation with bindings for display, input, filesystem, and system clock.

#include "lua_vm.h"
#include "hal.h"
#include "config.h"
#include "clock.h"
#include <SPIFFS.h>
#include <lua.hpp>

namespace LuaVM {

// --------------------------------------------------------------------------
// INTERNAL STATE
// --------------------------------------------------------------------------

static lua_State* L = nullptr;
static String lastError = "";

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

// gfx.setFont(size) - Set font size (0=small, 1=medium, 2=large)
static int lua_gfx_setFont(lua_State* L) {
    int size = luaL_optinteger(L, 1, 0);
    switch (size) {
        case 0: u8g2.setFont(u8g2_font_5x7_tf); break;
        case 1: u8g2.setFont(u8g2_font_6x10_tf); break;
        case 2: u8g2.setFont(u8g2_font_8x13_tf); break;
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

// input.scan() - Scan the key matrix (call once per frame)
static int lua_input_scan(lua_State* L) {
    scanMatrix();
    return 0;
}

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
        {"scan", lua_input_scan},
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
// LUA BINDINGS - File System Functions
// --------------------------------------------------------------------------

// fs.exists(path) - Check if file exists
static int lua_fs_exists(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    lua_pushboolean(L, SPIFFS.exists(path));
    return 1;
}

// fs.read(path) - Read entire file as string
static int lua_fs_read(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    File file = SPIFFS.open(path, "r");
    if (!file) {
        lua_pushnil(L);
        lua_pushstring(L, "Could not open file");
        return 2;
    }
    
    String content = file.readString();
    file.close();
    lua_pushstring(L, content.c_str());
    return 1;
}

// fs.write(path, content) - Write string to file
static int lua_fs_write(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    const char* content = luaL_checkstring(L, 2);
    
    File file = SPIFFS.open(path, "w");
    if (!file) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "Could not open file for writing");
        return 2;
    }
    
    size_t written = file.print(content);
    file.close();
    lua_pushboolean(L, written > 0);
    return 1;
}

// fs.list(path) - List files in directory
static int lua_fs_list(lua_State* L) {
    const char* path = luaL_optstring(L, 1, "/");
    
    File root = SPIFFS.open(path);
    if (!root || !root.isDirectory()) {
        lua_newtable(L);
        return 1;
    }
    
    lua_newtable(L);
    int index = 1;
    File file = root.openNextFile();
    while (file) {
        lua_pushstring(L, file.name());
        lua_rawseti(L, -2, index++);
        file = root.openNextFile();
    }
    return 1;
}

// Register all filesystem functions
static void registerFsModule(lua_State* L) {
    static const luaL_Reg fs_funcs[] = {
        {"exists", lua_fs_exists},
        {"read", lua_fs_read},
        {"write", lua_fs_write},
        {"list", lua_fs_list},
        {NULL, NULL}
    };
    
    luaL_newlib(L, fs_funcs);
    lua_setglobal(L, "fs");
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
    
    result = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (result != LUA_OK) {
        lastError = lua_tostring(L, -1);
        lua_pop(L, 1);
        return false;
    }
    
    lastError = "";
    return true;
}

bool executeFile(const char* path) {
    if (L == nullptr) {
        lastError = "Lua VM not initialized";
        return false;
    }
    
    // Ensure path starts with /
    String fullPath = path;
    if (!fullPath.startsWith("/")) {
        fullPath = "/" + fullPath;
    }
    
    File file = SPIFFS.open(fullPath.c_str(), "r");
    if (!file) {
        lastError = "Could not open file: ";
        lastError += path;
        return false;
    }
    
    String content = file.readString();
    file.close();
    
    return executeString(content.c_str(), path);
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

} // namespace LuaVM
