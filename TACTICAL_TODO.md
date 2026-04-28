## CURRENT GOAL: Lua SD Desktop Discovery + File Browser
- TARGET_FILE: src/lua_scripts.cpp
- TRUTH_RELIANCE: TRUTH_HARDWARE.md Section 0 (ESP32-S2-Mini memory limits), Section 0.1 and Section 3 (dedicated SD SPI bus and SD CS pin), Section 1 (128x64 ST7920 display constraints), Section 2 (4x5 key matrix and special keys); TRUTH_FLASHING.md Quick Reference + Flashing Procedure
- TECHNICAL_CONSTRAINTS:
  - TRUTH_PROJECT.md is not present in workspace; no additional constraints may be inferred beyond explicit Truth files.
- ATOMIC_TASKS:
  - [x] TASK_1: Restore the minimal Lua runtime surface needed for SD-backed desktop discovery and browsing.
    Required signatures:
    - src/lua_vm.cpp:
      - Register a read-only `fs` Lua module backed by the existing SdFat session API.
      - Register a minimal `ui` Lua module exposing standardized header/footer helpers that wrap the existing C++ GUI layer.
      - Keep SD access session-scoped via `sdBeginSession()` / `sdEndSession()`.
    - src/lua_vm.h:
      - Add declarations only for new runtime entrypoints that are strictly required by the above bindings.
    Required behavior:
    - `fs.list(path)` returns enough metadata for Lua to distinguish files vs directories and render a browser list.
    - `fs.read(path)` is available so the desktop can inspect or launch SD Lua apps.
    - `ui.header(title, rightText)` and `ui.footer(leftHint, rightHint)` render with the same visual conventions as the C++ apps.

  - [x] TASK_2: Turn the embedded Lua desktop into an app host and ship the first built-in Lua file browser.
    Required signatures:
    - src/lua_scripts.cpp:
      - Replace the minimal static grid desktop with a built-in app registry and desktop launcher flow.
      - Add a built-in Lua file-browser app and icon bitmap.
      - Implement SD desktop-app discovery using a global `APP_METADATA` table contract, with built-ins rendered first and discovered SD apps appended at the end.
    - src/lua_scripts.h:
      - Declare additional embedded Lua payloads only if desktop and file-browser code are split into separate script constants.
    Required behavior:
    - The desktop shows the built-in file-browser app as a selectable icon before any discovered SD apps.
    - SD scripts appear on the desktop only when they expose `APP_METADATA.desktop == true` plus the required metadata fields.
    - The built-in file browser lists SD files and directories, supports parent/child navigation, and stubs file opening instead of editing or executing files.
    - `TAB` cycles `VIEW_FULL -> VIEW_DETAILS -> VIEW_MIN -> VIEW_FULL`.

  - [ ] VERIFICATION: Build and on-device SD navigation checklist
    Success criteria/tests to run:
    - `pio run -e lolin_s2_mini` completes with zero errors.
    - Flash via `bash scripts/flash.sh` using the DFU flow from TRUTH_FLASHING.md.
    - With an SD card inserted, the Lua desktop renders the built-in file-browser icon and appends only tagged SD apps after built-ins.
    - In the built-in file browser, directories open, parent navigation works, regular files produce the stub action, and all three TAB view modes render legibly on the 128x64 display.
    - Returning from a launched app leaves the desktop usable, and `ESC` still transitions into Settings cleanly.
