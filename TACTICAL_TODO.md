## CURRENT GOAL: Lua File Browser Read-Only Viewer Handoff
- TARGET_FILE: src/main.cpp
- TRUTH_RELIANCE: TRUTH_HARDWARE.md Section 0 (ESP32-S2-Mini memory limits), Section 0.1 and Section 3 (dedicated SD SPI bus and SD CS pin), Section 1 (128x64 ST7920 display constraints), Section 2 (4x5 key matrix and special keys); TRUTH_FLASHING.md Quick Reference + Flashing Procedure
- TECHNICAL_CONSTRAINTS:
  - TRUTH_PROJECT.md is not present in workspace; no additional constraints may be inferred beyond explicit Truth files.
- ATOMIC_TASKS:
  - [x] TASK_1: Bridge read-only viewer sessions from `MODE_LUA` into the canonical C++ viewer without breaking ownership or return-to-Lua flow.
    Required signatures:
    - src/app_control.h:
      - Declare a helper that enters a specific C++ app from `MODE_LUA` and marks the session to return to Lua when that app exits.
    - src/main.cpp:
      - Implement the helper and extend the `appT9Editor.exitRequested` return path so transfer-launched viewer sessions can restore `MODE_LUA` instead of defaulting to `appSettings`.
      - Preserve existing `ESC` semantics for normal Lua/Desktop and Settings flows.
    - src/lua_vm.cpp:
      - Register one minimal Lua binding that loads an SD file into the existing `appTransfer` payload and launches `appT9Editor` in `APP_TRANSFER_EDITOR_READ_ONLY` mode through the new helper.
    - src/lua_vm.h:
      - Add declarations only for new runtime entrypoints that are strictly required by the viewer-launch binding.
    Required behavior:
    - Viewer sessions launched from Lua enter `MODE_SETTINGS` only for the duration of the viewer session.
    - Exiting that viewer returns directly to `MODE_LUA`, not to the Settings root.
    - The canonical `T9EditorApp` remains the only text-view implementation; no parallel Lua text viewer is introduced.

  - [x] TASK_2: Replace the built-in Lua file browser's regular-file open stub with real read-only viewer dispatch while keeping discovery semantics unchanged.
    Required signatures:
    - src/lua_scripts.cpp:
      - Replace the regular-file path inside `file_browser:open_selected()` so it calls the new read-only viewer binding instead of showing the current stub toast.
      - Preserve directory navigation, selection memory, current view-mode state, and the existing desktop discovery contract.
    - src/lua_scripts.h:
      - Remain unchanged unless the implementation explicitly splits the embedded desktop/browser payloads further.
    Required behavior:
    - `ENTER` on a directory still navigates into that directory; `LEFT` and `BKSP` still navigate to parent.
    - `ENTER` on a regular file opens the canonical read-only viewer instead of a stub message.
    - Returning from the viewer preserves the current file-browser path, selected entry, and current `TAB` view mode.
    - Desktop discovery remains the only path that launches SD desktop apps; opening a `.lua` file from the browser views it as text rather than executing it.

  - [x] VERIFICATION: Build, flash, and fixture-backed on-device viewer handoff checklist
    Success criteria/tests to run:
    - `pio run -e lolin_s2_mini` completes with zero errors.
    - Flash via `bash scripts/flash.sh` using the DFU flow from TRUTH_FLASHING.md.
    - With an SD card containing the mirrored fixture set from `data_backup/SD card contents/`, `blank_app (bad icon).lua` does not appear on the desktop, while `blank_app (bad code).lua` still appears and still triggers the existing crash-popup path when launched from the desktop.
    - In the built-in Lua file browser, `New Folder/` opens as a directory, `New Empty File` opens in the canonical read-only viewer, and `blank_app (bad code).lua` opens as text in the canonical read-only viewer rather than executing.
    - Exiting the viewer returns directly to the Lua desktop/browser, preserving the current path, selection, and view mode.
    - `ESC` still transitions into Settings cleanly when no viewer session is active.
