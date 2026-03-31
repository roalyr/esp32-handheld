# TACTICAL_TODO.md
<!-- Machine-readable Implementation Contract -->

## CURRENT GOAL: SD Card Filesystem + Lua File Manager + Hot-Plug Safety

- **TARGET_FILES:** `src/config.h`, `src/hal.cpp`, `src/hal.h`, `src/lua_vm.cpp`, `src/lua_scripts.cpp`, `src/lua_scripts.h`, `src/apps/settings.cpp`, `src/apps/settings.h`, `src/main.cpp`
- **TRUTH_RELIANCE:**
  - `TRUTH_HARDWARE.md` Section 0 — ESP32-S2-Mini, 4MB Flash, 2MB PSRAM
  - `TRUTH_HARDWARE.md` Section 0.1 — SPI bus: MOSI=GPIO 35, MISO=GPIO 37, SCK=GPIO 36
  - `TRUTH_HARDWARE.md` Section 1 — LCD ST7920: CS=GPIO 38, shares MOSI+SCK on SPI bus
  - `TRUTH_HARDWARE.md` Section 3 — SD card module: CS=GPIO 39, shares SPI bus
- **TECHNICAL_CONSTRAINTS:**
  - ESP32-S2-Mini: 4MB flash, 2MB PSRAM
  - **Shared SPI bus** — LCD and SD card share MOSI (GPIO 35) and SCK (GPIO 36). SD card adds MISO (GPIO 37). Each device has its own CS (LCD=38, SD=39).
  - **LCD must switch from SW_SPI to HW_SPI** — Currently `U8G2_ST7920_128X64_F_SW_SPI` bit-bangs GPIO pins. Once `SPI.begin()` claims pins for the hardware SPI peripheral, `digitalWrite()` no longer controls pin output. Both devices must use the hardware SPI peripheral with CS-based multiplexing. Constructor changes to `U8G2_ST7920_128X64_F_HW_SPI(U8G2_R0, PIN_CS)`.
  - **SPI init order:** `SPI.begin(SCK, MISO, MOSI)` → `u8g2.begin()` → `SD.begin(SD_CS)`. U8G2 HW_SPI uses the already-configured Arduino SPI instance. Single-threaded loop — no SPI bus contention.
  - **SD card partition: FAT32** — Most common SD card format. Arduino `SD.h` library (built-in to ESP32 Arduino framework, wraps `ff` FAT filesystem). No additional lib_deps needed.
  - **Hot-plug safe:** SD card may be removed/inserted at any time. All SD operations must check mount status first, handle failures gracefully (return error, never crash), and allow re-mount.
  - Lua VM (libLua) is already linked; `LuaVM` namespace has gfx/input/sys/t9 bindings
  - Embedded Lua scripts are in `lua_scripts.cpp` — no SPIFFS (purged in previous milestone)
  - Architecture: two C++ states (MODE_LUA ↔ MODE_SETTINGS) + Lua desktop (unchanged)
  - Lua desktop currently shows a 4×2 empty grid — must be upgraded to show detected apps
  - **Desktop app detection:** Each embedded Lua script that should appear on the desktop must have `--@DESKTOP:AppName` as its first line. C++ scans for this prefix and builds a registry. The desktop queries this registry to populate its grid.

- **ATOMIC_TASKS:**

  - [x] TASK_1: Update `src/config.h` — add SD card CS pin and SPI bus pin constants
    - Add `#define PIN_SD_CS 39` — SD card chip select (TRUTH_HARDWARE Section 3)
    - Add `#define PIN_SPI_MISO 37` — MISO line for SD card (TRUTH_HARDWARE Section 0.1)
    - Rename `PIN_SPI_SID` to `PIN_SPI_MOSI` for clarity (value stays 35)
    - Keep `PIN_SPI_SCLK` = 36 (already defined)
    - Keep `PIN_CS` = 38 (LCD chip select, already defined)
    - Remove the "SD card pins — NOT YET WIRED" comment block
    - Required Result: All SPI bus and SD card pins have named constants

  - [x] TASK_2: Update `src/hal.cpp` and `src/hal.h` — switch LCD to HW_SPI, init shared SPI bus, add SD card mount/unmount
    - **hal.h changes:**
      - Add `#include <SPI.h>` (needed for HW_SPI display type)
      - Change display extern from `U8G2_ST7920_128X64_F_SW_SPI` to `U8G2_ST7920_128X64_F_HW_SPI`
      - Add SD state exports:
        ```cpp
        bool mountSD();       // Attempt to (re)mount SD card, return success
        void unmountSD();     // Safely unmount SD card
        bool isSDMounted();   // Check current mount status
        uint64_t sdTotalBytes();  // Total SD card space (0 if not mounted)
        uint64_t sdUsedBytes();   // Used SD card space (0 if not mounted)
        ```
    - **hal.cpp changes:**
      - Add `#include <SPI.h>` and `#include <SD.h>`
      - Change display constructor:
        ```cpp
        U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R0, PIN_CS);
        ```
      - Add SD state:
        ```cpp
        static bool sdMounted = false;
        ```
      - In `setupHardware()`, BEFORE `u8g2.begin()`:
        ```cpp
        SPI.begin(PIN_SPI_SCLK, PIN_SPI_MISO, PIN_SPI_MOSI);
        ```
      - In `setupHardware()`, AFTER `u8g2.begin()`:
        ```cpp
        sdMounted = SD.begin(PIN_SD_CS);
        if (sdMounted) Serial.println("[HAL] SD card mounted");
        else Serial.println("[HAL] SD card not found");
        ```
      - Implement `mountSD()`:
        ```cpp
        bool mountSD() {
            if (sdMounted) { SD.end(); sdMounted = false; }
            sdMounted = SD.begin(PIN_SD_CS);
            return sdMounted;
        }
        ```
      - Implement `unmountSD()`:
        ```cpp
        void unmountSD() {
            if (sdMounted) { SD.end(); sdMounted = false; }
        }
        ```
      - Implement `isSDMounted()`: return `sdMounted`
      - Implement `sdTotalBytes()`: return `sdMounted ? SD.totalBytes() : 0`
      - Implement `sdUsedBytes()`: return `sdMounted ? SD.usedBytes() : 0`
    - **CRITICAL:** This changes LCD communication from software bit-bang to hardware SPI. Must verify display still works after this change before proceeding to further tasks. If HW_SPI fails, fallback: manage SPI dynamically (`SPI.begin()`/`SPI.end()` around SD operations, keep SW_SPI for LCD).

  - [ ] TASK_3: Add `sd` Lua module to `src/lua_vm.cpp`
    - Add `#include <SD.h>` at top (for `File` type access)
    - Register new `sd` Lua module in `init()` via `registerSDModule(L)`:
      ```lua
      sd.mount()           -- Attempt to mount SD card, return bool success
      sd.unmount()         -- Safely unmount SD card
      sd.isMounted()       -- Return bool: is SD card currently mounted
      sd.list(path)        -- Return table: {{name="file.txt", isDir=false, size=1234}, ...}
                           -- Returns nil, "SD not mounted" on failure
      sd.exists(path)      -- Return bool: does file/dir exist on SD
      sd.read(path)        -- Read entire file as string (max 32KB guard to prevent OOM)
                           -- Returns nil, "error message" on failure
      sd.info()            -- Return table: {total=bytes, used=bytes, free=bytes}
                           -- Returns nil, "SD not mounted" if not mounted
      sd.mkdir(path)       -- Create directory, return bool success
      ```
    - **Hot-plug safety:** Every sd.* function that accesses the card must:
      1. Check `isSDMounted()` first — return nil + "SD not mounted" if false
      2. If any SD.* call fails unexpectedly, call `unmountSD()` to mark as disconnected
      3. Close all File handles before returning (even on error paths)
      4. Return nil + error string on failure (never throw, never crash)
    - `sd.list(path)` implementation:
      ```cpp
      if (!isSDMounted()) { lua_pushnil(L); lua_pushstring(L, "SD not mounted"); return 2; }
      File dir = SD.open(path);
      if (!dir || !dir.isDirectory()) {
          if (dir) dir.close();
          lua_pushnil(L); lua_pushstring(L, "not a directory"); return 2;
      }
      lua_newtable(L);
      int idx = 1;
      File entry = dir.openNextFile();
      while (entry) {
          lua_newtable(L);
          lua_pushstring(L, entry.name()); lua_setfield(L, -2, "name");
          lua_pushboolean(L, entry.isDirectory()); lua_setfield(L, -2, "isDir");
          lua_pushinteger(L, entry.size()); lua_setfield(L, -2, "size");
          lua_rawseti(L, -2, idx++);
          entry.close();
          entry = dir.openNextFile();
      }
      dir.close();
      return 1;
      ```
    - `sd.read(path)` — Open file, check size ≤ 32768, read into buffer, push as Lua string, close. Return nil + error if file too large or open fails.
    - Required Result: Lua scripts can safely browse and read SD card contents

  - [ ] TASK_4: Add embedded Lua file manager script to `src/lua_scripts.cpp` and `src/lua_scripts.h`
    - **lua_scripts.h:** Add `extern const char LUA_FILE_MANAGER[];`
    - **lua_scripts.cpp:** Add `const char LUA_FILE_MANAGER[] = R"LUA(...)LUA";`
    - **First line of LUA_FILE_MANAGER source must be:** `--@DESKTOP:Files`
      This is the flag the desktop uses to detect it as a desktop app (see TASK_5).
    - Script functionality:
      - State: `currentPath` (starts at "/"), `entries` table, `selectedIndex`, `scrollOffset`
      - `_init()`: check `sd.isMounted()`, if mounted call `refreshList()`
      - `refreshList()`: call `sd.list(currentPath)`, sort dirs first then files, populate entries
      - `_draw()`:
        - Header bar (inverted): current path (truncated from left if too long)
        - If SD not mounted: centered message "No SD card" + "TAB:Retry"
        - If mounted: scrollable file list with ">" prefix for dirs, file size for files
        - Scrollbar if entries exceed visible area (~4 lines)
        - Footer: "ESC:Back BKSP:Up ENTER:Open"
      - `_input(key)`:
        - UP/DOWN: move selection, adjust scroll offset if needed
        - ENTER on directory: append name + "/" to currentPath, refreshList()
        - ENTER on file: (no action for now — future: view/run)
        - BKSP: go up one directory level (strip last path component), refreshList()
        - TAB: attempt `sd.mount()` + refreshList() (manual retry for hot-plug)
        - ESC: set `_APP_EXIT = true` to signal return to desktop
      - Error handling: wrap all sd.* calls, display error messages inline, never crash
    - Required Result: A working read-only file browser for SD card contents

  - [ ] TASK_5: Add app registry system + update desktop to detect and launch apps
    - **lua_scripts.cpp:** Add app registry infrastructure:
      ```cpp
      struct EmbeddedApp {
          const char* name;     // Display name (extracted from --@DESKTOP:Name)
          const char* source;   // Full Lua source code
      };
      
      // All embedded scripts that could be apps:
      static const char* ALL_SCRIPTS[] = { LUA_FILE_MANAGER /*, future scripts */ };
      static const int ALL_SCRIPTS_COUNT = sizeof(ALL_SCRIPTS) / sizeof(ALL_SCRIPTS[0]);
      
      // Parsed at startup — only scripts with --@DESKTOP: prefix
      static EmbeddedApp desktopApps[16];
      static int desktopAppCount = 0;
      
      void initAppRegistry(); // Scan ALL_SCRIPTS for --@DESKTOP: prefix, populate desktopApps
      int getDesktopAppCount();
      const EmbeddedApp* getDesktopApp(int index);
      ```
      - `initAppRegistry()` iterates ALL_SCRIPTS, checks if first line starts with `--@DESKTOP:`, extracts name after colon (trimmed), adds to desktopApps array.
    - **lua_scripts.h:** Add struct definition, `initAppRegistry()`, `getDesktopAppCount()`, `getDesktopApp()` declarations
    - **lua_vm.cpp:** Add `scripts` Lua module registered in `init()`:
      ```lua
      scripts.count()         -- Return number of desktop apps (int)
      scripts.getName(index)  -- Return name of app at 1-based index (string)
      scripts.load(index)     -- Execute app source code in current Lua state, return bool success
      ```
      - `scripts.load(index)` calls `LuaVM::executeString(app->source, app->name)` internally
    - **main.cpp:** Call `initAppRegistry()` in `setup()` after serial init (before LuaVM::init is fine)
    - **lua_scripts.cpp:** Rewrite `LUA_DESKTOP` to use the app registry:
      - `_init()`: query `scripts.count()` and build list of app names
      - List-based layout (not grid) — show app names in a scrollable vertical list
      - If no apps: show "No apps found"
      - ENTER on selected app:
        1. Save desktop callbacks: `desktop_draw = _draw; desktop_update = _update; desktop_input = _input`
        2. Call `scripts.load(selectedIndex)` — which redefines _draw/_update/_input globals
        3. If load fails: restore desktop callbacks, show error
      - `_update()`: check `_APP_EXIT` flag — if true, restore desktop callbacks and clear flag
      - Footer: "ESC:Settings ENTER:Open"
      - UP/DOWN: navigate app list
    - Required Result: Desktop shows "Files" app, ENTER launches it, ESC from app returns to desktop

  - [ ] TASK_6: Update `src/apps/settings.cpp` — show SD card mount status and space info
    - In `renderSettingsList()`, modify the system info line (currently "RAM XXk") to include SD status:
      ```cpp
      int freeRamK = ESP.getFreeHeap() / 1024;
      char buf[40];
      if (isSDMounted()) {
          uint64_t totalMB = sdTotalBytes() / (1024*1024);
          uint64_t usedMB = sdUsedBytes() / (1024*1024);
          snprintf(buf, sizeof(buf), "RAM %dk SD %llu/%lluM", freeRamK, (unsigned long long)usedMB, (unsigned long long)totalMB);
      } else {
          snprintf(buf, sizeof(buf), "RAM %dk SD:none", freeRamK);
      }
      u8g2.drawStr(2, y, buf);
      ```
    - `hal.h` already included via `app_interface.h` — `isSDMounted()`, `sdTotalBytes()`, `sdUsedBytes()` accessible
    - Required Result: Settings screen shows SD card status and capacity alongside RAM info

  - [ ] TASK_7: Hot-plug safety verification pass
    - **hal.cpp `mountSD()`:** Always call `SD.end()` before re-trying `SD.begin()` to clean up stale state (already specified in TASK_2)
    - **lua_vm.cpp `sd.*` functions:** Verify every function:
      - Returns nil + error string if `!isSDMounted()` (except `sd.mount()` and `sd.isMounted()`)
      - Wraps SD file operations in null checks (File object validity)
      - Closes all File handles before returning (even on error paths)
      - If an SD operation fails unexpectedly on a "mounted" card, calls `unmountSD()` to flag card as removed
    - **lua_scripts.cpp `LUA_FILE_MANAGER`:** Verify:
      - All `sd.*` return values checked for nil
      - Shows user-friendly error messages on failure
      - TAB key triggers `sd.mount()` + `refreshList()` for manual recovery
      - Never crashes on nil/empty results from SD operations
    - **main.cpp:** No periodic SD polling needed (saves CPU). Mount check is on-demand:
      - Settings: `isSDMounted()` checked each render (lightweight bool read via `sdTotalBytes()`/`sdUsedBytes()`)
      - File manager: checks on init and on explicit TAB retry
      - Apps degrade gracefully to "SD not found" state without crashes
    - Required Result: SD card can be safely removed and re-inserted at any time without crashes or hangs

  - [ ] VERIFICATION:
    - **Compile:** `pio run -e lolin_s2_mini` — zero errors, zero warnings from modified files
    - **Flash:** `bash scripts/flash.sh` — upload success
    - **CRITICAL — LCD test:** Display must still work after SW_SPI → HW_SPI switch. If display is blank or garbled: check `SPI.begin()` is called BEFORE `u8g2.begin()`, check ST7920 serial clock speed (U8G2 default should be ≤1MHz). If unfixable, fall back to SPI.begin/end management with SW_SPI.
    - **SD mount:** Insert FAT32-formatted SD card → serial shows "[HAL] SD card mounted"
    - **SD not present:** Boot without SD card → serial shows "[HAL] SD card not found", device functions normally
    - **Settings:** ESC → Settings shows "RAM XXk SD XX/XXM" (with card) or "RAM XXk SD:none" (without)
    - **Desktop:** Shows "Files" app in list (detected via `--@DESKTOP:Files` flag)
    - **Launch app:** Select "Files" + ENTER → file manager loads, shows SD card root listing
    - **File browser:** UP/DOWN scrolls, ENTER opens folders, BKSP goes up, path shown at top
    - **Hot-plug remove:** Remove SD card while file manager is open → next sd.list() returns nil → shows "No SD card" message
    - **Hot-plug insert:** Press TAB → `sd.mount()` succeeds → file list refreshes
    - **ESC from app:** ESC in file manager → `_APP_EXIT = true` → desktop restores, grid visible
    - **Memory:** `ESP.getFreeHeap()` via serial — must be >40KB free after Lua init + SD mount

---
**STATUS:** PENDING
**PRIORITY:** CRITICAL — Adds external storage and first user-launchable Lua app via desktop registry
**PREVIOUS_MILESTONE:** Two-State Architecture — Cooperative Lua Runtime (COMPLETED — all 7 tasks verified per SESSION_LOG 2026-03-28)
**KEY_RISK:** HW_SPI switch for ST7920 LCD — display may require tuning (SPI clock speed, mode). If HW_SPI fails on hardware, fallback: manage SPI peripheral dynamically (`SPI.begin()`/`SPI.end()` around SD operations, keep SW_SPI for LCD between SD accesses).
**NOTE:** No `lib_deps` additions needed — `SD.h`, `SPI.h`, `FS.h` are built into ESP32 Arduino framework. CPP app source files remain in src/apps/ for reference.
