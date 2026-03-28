# TACTICAL_TODO.md
<!-- Machine-readable Implementation Contract -->

## CURRENT GOAL: Two-State Architecture — Cooperative Lua Runtime (Embedded Scripts)

- **TARGET_FILES:** `src/main.cpp`, `src/lua_vm.cpp`, `src/lua_vm.h`, `src/hal.cpp`, `src/apps/settings.cpp`, `src/apps/settings.h`, `src/config.h`, `data/luainit.lua`
- **TRUTH_RELIANCE:** `TRUTH_HARDWARE.md` — Section 0 (4MB Flash / 2MB PSRAM bounds), Section 3 (SD card NOT YET WIRED — SPIFFS only)
- **TECHNICAL_CONSTRAINTS:**
  - ESP32-S2-Mini: 4MB flash, 2MB PSRAM. SPIFFS partition uses internal flash (no SD card yet)
  - **SPIFFS is strictly READ-ONLY at runtime** — it hosts built-in Lua scripts uploaded via `pio run -t uploadfs`. No write/append/remove/rename from Lua or C++. Future SD card will be the writable storage.
  - `board_build.filesystem = spiffs` is already declared in `platformio.ini`
  - Lua VM (libLua) is already linked; `LuaVM` namespace exists with gfx/input/sys/fs bindings
  - Current `fs` Lua module is stubbed ("No filesystem") — must be wired to SPIFFS
  - Current `executeFile()` is stubbed — must read from SPIFFS
  - Current architecture: MenuApp → N CPP apps. **New architecture: two C++ states only: MODE_LUA ↔ MODE_SETTINGS**
  - Lua state must PERSIST across ESC toggles (no init/shutdown on mode switch)
  - T9 engine (t9_engine.cpp/h) must be exposed to Lua for text input capability
  - `gui.cpp/h` remains for Settings rendering only — Lua apps use `gfx.*` bindings
  - `clock.cpp/h` remains as utility (sys.time/sys.timeStr already bound)
  - Sleep mode logic stays in main.cpp (C++ level, applies to both modes)
  - Key repeat logic stays in hal.cpp (C++ level, available to both modes)
  - All removed CPP app source files (`src/apps/menu.*`, `src/apps/file_browser.*`, etc.) stay on disk but are decoupled from build via removed `#include` lines
  - `data/` folder contents are uploaded to SPIFFS via `pio run -t uploadfs`
  - luainit.lua fallback chain: try `/sd/luainit.lua` (future SD), else `/luainit.lua` (SPIFFS built-in)

- **ATOMIC_TASKS:**

  - [x] TASK_1: Re-enable SPIFFS in `src/hal.cpp` + implement real `fs` module in `src/lua_vm.cpp`
    - **SUPERSEDED**: SPIFFS was implemented, tested on hardware, failed to mount reliably. Replaced by embedded Lua scripts (`lua_scripts.cpp/h`). All SPIFFS code purged. `fs` module and `executeFile()` removed.

  - [x] TASK_2: Add cooperative Lua frame-loop API to `src/lua_vm.h` and `src/lua_vm.cpp`
    - New functions in `LuaVM` namespace:
      ```cpp
      bool callGlobalFunction(const char* funcName);
      // Push global function by name, pcall(0,0), return success.
      // If function doesn't exist, silently return true (not an error).
      // On Lua error, set lastError and return false.

      bool callInputHandler(char key);
      // Push global "_input", push key as single-char string, pcall(1,0).
      // If "_input" doesn't exist, silently return true.
      // On Lua error, set lastError and return false.
      ```
    - Lua frame contract (documented in luainit.lua header comment):
      - `_init()` — called once after luainit.lua executes (optional)
      - `_update()` — called once per frame, for logic/timers (optional)
      - `_draw()` — called once per frame, for rendering. C++ does NOT call `gfx.clear()`/`gfx.send()` — Lua owns the full draw cycle (optional)
      - `_input(key)` — called for each just-pressed key as single-char string (optional)
    - Required signatures in `lua_vm.h`:
      ```cpp
      bool callGlobalFunction(const char* funcName);
      bool callInputHandler(char key);
      ```

  - [x] TASK_3: Add T9 engine Lua bindings to `src/lua_vm.cpp`
    - Register new `t9` Lua module in `init()` via `registerT9Module(L)`:
      ```lua
      t9.reset()           -- Reset engine state (clear buffer, cursor to 0)
      t9.input(key)        -- Feed a key char to T9 engine (handles multitap cycling)
      t9.update()          -- Call per frame (commits pending char on timeout)
      t9.getText()         -- Return current text buffer as string
      t9.setText(str)      -- Set text buffer content
      t9.getCursorByte()   -- Return cursor byte position (int)
      t9.getCursorChar()   -- Return cursor character position (int, via getUtf8Length of substring)
      t9.setCursor(pos)    -- Set cursor to character position
      t9.isPending()       -- Return bool: is there a pending multitap char
      t9.isShifted()       -- Return bool: shift/caps state
      t9.commit()          -- Force commit pending character
      t9.getCharCount()    -- Return total character count (UTF-8 aware)
      ```
    - Must `#include "t9_engine.h"` in `lua_vm.cpp`
    - Use global `engine` instance (already `extern T9Engine engine` in t9_engine.h)
    - Required Result: Lua scripts can call `t9.input(key)` to build text with T9 multitap

  - [x] TASK_4: Restructure `src/main.cpp` — two-state main loop (MODE_LUA ↔ MODE_SETTINGS)
    - Remove all CPP app includes except: `apps/settings.h`, `apps/key_tester.h`
    - Remove all CPP app instances except: `SettingsApp appSettings`, `KeyTesterApp appKeyTester`
    - Remove: `appMenu`, `appT9Editor`, `appGfxTest`, `appStopwatch`, `appFileBrowser`, `appLuaRunner`, `appClock`
    - Remove: `#include "app_transfer.h"`, all app transfer logic, T9 exit logic, menu switch logic
    - Add system mode enum and state:
      ```cpp
      enum SystemMode { MODE_LUA, MODE_SETTINGS };
      static SystemMode currentMode = MODE_LUA;
      static bool luaError = false;
      static String luaErrorMsg = "";
      ```
    - New `setup()` flow:
      1. `Serial.begin(115200)` + `setupHardware()` + `showBootSplash()` + `SystemClock::init()`
      2. `LuaVM::init()` — create Lua state, register all modules
      3. `LuaVM::executeFile("/luainit.lua")` — run startup script
      4. If executeFile fails: set `luaError = true`, `luaErrorMsg = LuaVM::getLastError()`
      5. `LuaVM::callGlobalFunction("_init")` — call optional init hook
      6. `currentMode = MODE_LUA`
    - New `loop()` frame logic:
      ```
      scanMatrix() → checkSleepMode() → if asleep return
      
      ESC handling (just-pressed only, before app/lua input):
        if MODE_LUA:  currentMode = MODE_SETTINGS; appSettings.start()
        if MODE_SETTINGS:
          if appSettings is in submenu (key tester): let it handle ESC
          else: appSettings.stop(); currentMode = MODE_LUA
      
      if MODE_LUA:
        if luaError: render error screen (show luaErrorMsg, "Press ENTER to retry")
          ENTER key → re-run luainit.lua, clear error
        else:
          for each just-pressed key (except ESC): LuaVM::callInputHandler(key)
          LuaVM::callGlobalFunction("_update")
          u8g2.clearBuffer()
          LuaVM::callGlobalFunction("_draw")
          u8g2.sendBuffer()
      
      if MODE_SETTINGS:
        route non-ESC input to appSettings.handleInput(key)
        appSettings.update()
        u8g2.clearBuffer()
        appSettings.render()
        u8g2.sendBuffer()
      ```
    - Sleep mode: keep existing `checkSleepMode()` / `enterSleep()` / `wakeUp()` unchanged
    - Boot splash: keep existing `showBootSplash()` unchanged
    - Required Result: device boots into Lua frame loop; ESC toggles to Settings and back

  - [x] TASK_5: Consolidate `src/apps/settings.cpp` and `src/apps/settings.h`
    - Settings menu items (in order):
      1. **Brightness** (0–255, displayed as %, editable with LEFT/RIGHT)
      2. **Sleep** (ON/OFF toggle)
      3. **Key Tester** (ENTER opens key tester subscreen — reuse existing `KeyTesterApp` logic or inline)
    - Remove **Contrast** setting (ST7920 has hardware pot, `setContrast()` is no-op — per TRUTH_HARDWARE.md)
    - Remove **Info** as a menu item — move system info to the header bar instead
    - Header bar (top line): `"HH:MM  XXk  SPIFFSXXk"` where:
      - `HH:MM` = system clock from `SystemClock::getTimeString()`
      - `XXk` = free RAM from `ESP.getFreeHeap() / 1024`
      - `SPIFFSXXk` = SPIFFS free space `(SPIFFS.totalBytes() - SPIFFS.usedBytes()) / 1024` if mounted, or `"NoFS"` if not
      - Future: `"BAT XX%"` placeholder when battery monitoring is wired
    - This keeps clock and system stats visible ONLY in Settings, freeing full 128x64 for Lua VM rendering
    - Key Tester subscreen:
      - When user selects "Key Tester" and presses ENTER → enter key tester mode
      - ESC from key tester → return to settings list (NOT exit settings entirely)
      - Reuse rendering logic from existing `key_tester.cpp` or inline equivalent
    - Keep: `systemBrightness` live preview via `ledcWrite(0, value)`
    - Keep: `sleepEnabled` toggle (extern from main.cpp or add to hal globals)
    - Required signatures (no change to App interface):
      ```cpp
      class SettingsApp : public App {
        void start() override;
        void stop() override;
        void update() override;
        void render() override;
        void handleInput(char key) override;
        bool isInSubmenu();  // Returns true when key tester is active (main.cpp checks for ESC routing)
      };
      ```

  - [x] TASK_6: Create `data/luainit.lua` — built-in Lua startup desktop
    - **SUPERSEDED**: SPIFFS replaced by embedded scripts. `LUA_DESKTOP` in `src/lua_scripts.cpp` serves as startup desktop.
    - Defines `_init()`, `_draw()`, `_update()`, `_input(key)` callbacks.
    - Minimal 4x2 grid desktop with cursor navigation.
    - Footer: "ESC:Settings"

  - [x] TASK_7: Remove dead CPP app code from build
    - In `src/main.cpp`: remove `#include` lines for: `apps/t9_editor.h`, `apps/gfx_test.h`, `apps/menu.h`, `apps/stopwatch.h`, `apps/file_browser.h`, `apps/yes_no_prompt.h`, `apps/lua_runner.h`, `apps/clock.h`, `app_transfer.h`
    - Remove corresponding global instances: `appT9Editor`, `appKeyTester` (if inlined into settings), `appGfxTest`, `appMenu`, `appStopwatch`, `appFileBrowser`, `appLuaRunner`, `appClock`
    - Remove `apps.h` include if no longer referenced
    - Do NOT delete the source files from `src/apps/` — they serve as reference for future Lua ports
    - Remove `app_transfer.cpp` and `app_transfer.h` from compilation (or just stop including them — linker will exclude unused objects)
    - Required Result: `pio run` compiles with only: main.cpp, hal.cpp, config.h, gui.cpp, clock.cpp, lua_vm.cpp, t9_engine.cpp, apps/settings.cpp, apps/key_tester.cpp (if kept separate)

  - [x] VERIFICATION:
    - Compile: `pio run -e lolin_s2_mini` — zero errors, zero warnings from modified files
    - Upload SPIFFS: `pio run -t uploadfs` — uploads `data/luainit.lua` + existing .lua scripts to flash
    - Flash firmware: `bash scripts/flash.sh` — upload success
    - Serial: `pio device monitor` — see: "SPIFFS mounted", "[LuaVM] Initialized", "Running /luainit.lua"
    - Display: Lua desktop visible (file list or welcome text)
    - Input: UP/DOWN navigates, ENTER runs a script, ESC opens Settings
    - Settings: Brightness slider works, Sleep toggle works, Key Tester subscreen works
    - Settings header: shows free RAM and "SPIFFS" mount status
    - ESC from Settings: returns to Lua desktop (state preserved — same scroll position, same screen)
    - Run a Lua script (e.g., hello.lua): script executes, output visible
    - After script ends: desktop resumes
    - Memory: check `ESP.getFreeHeap()` via serial — must be >50KB free after Lua init

---
**STATUS:** PENDING
**PRIORITY:** CRITICAL — This milestone transforms the firmware from a monolithic CPP app into a Lua-first coding environment
**PREVIOUS_MILESTONE:** Hardware port to ESP32-S2-Mini (COMPLETED — verified compile, flash, boot per SESSION_LOG 2026-03-28)
**ARCHITECTURE_SHIFT:** Menu + N CPP apps → Two C++ states (MODE_LUA ↔ MODE_SETTINGS) + Lua desktop
**NOTE:** CPP app source files are preserved in src/apps/ as reference for future Lua reimplementation. T9 engine stays in CPP but is now exposed to Lua via t9.* bindings.
