## CURRENT GOAL: Lua SD File Edit Save-Back
- TARGET_FILE: src/main.cpp
- TRUTH_RELIANCE: TRUTH_HARDWARE.md Section 0 (ESP32-S2-Mini memory limits), Section 0.1 and Section 3 (dedicated SD SPI bus and SD CS pin), Section 1 (128x64 ST7920 display constraints), Section 2 (4x5 key matrix and special keys); TRUTH_FLASHING.md Quick Reference + Flashing Procedure
- TECHNICAL_CONSTRAINTS:
  - TRUTH_PROJECT.md is not present in workspace; no additional constraints may be inferred beyond explicit Truth files.
- ATOMIC_TASKS:
  - [x] TASK_1: Bridge read-write editor sessions from `MODE_LUA` into the canonical C++ editor and preserve the completed save/cancel payload long enough for Lua to consume it on return.
    Required signatures:
    - src/app_control.h:
      - Keep `launchLuaOwnedApp(App* newApp);` as the canonical Lua-owned app entry point.
    - src/main.cpp:
      - Extend the `appT9Editor.exitRequested` return path so Lua-owned read-write sessions do not clear the completed transfer payload before Lua resumes.
      - Preserve existing `ESC` semantics for normal Lua/Desktop and Settings flows.
    - src/lua_vm.cpp:
      - Add one minimal `ui.editFile(path, label)` launch binding and one one-shot result-consumption binding for resumed Lua code.
    - src/lua_vm.h:
      - Remain unchanged unless the new result bridge needs a strictly necessary declaration.
    Required behavior:
    - Editable sessions launched from Lua enter `MODE_SETTINGS` only for the duration of the editor session.
    - Exiting with save keeps the edited buffer, source path, and action metadata available until Lua consumes the result once.
    - Exiting without save returns to Lua without writing the file.
    - The canonical `T9EditorApp` remains the only text-editor implementation.

  - [x] TASK_2: Replace the built-in Lua file browser's regular-file open flow with edit/save-back handling for existing SD files while preserving current browsing semantics.
    Required signatures:
    - src/lua_scripts.cpp:
      - Replace the regular-file path inside `file_browser:open_selected()` so it launches the editable binding instead of the read-only viewer binding, and consume the returned editor result during the resumed Lua flow.
    - src/lua_vm.cpp:
      - Add the narrow SD write helper or binding strictly required for the built-in file browser to persist the returned buffer to the original path.
    - src/lua_scripts.h:
      - Remain unchanged unless the embedded payload split is explicitly required.
    Reference pattern:
    - src/apps/file_browser.cpp may be reused as a behavior template for editor-return handling, but it must not be revived as the implementation target.
    Required behavior:
    - `ENTER` on a directory still navigates into that directory; `LEFT` and `BKSP` still navigate to parent.
    - `ENTER` on a regular file opens the canonical read-write editor instead of the read-only viewer.
    - Saving from the editor writes the updated content back to the same SD path; canceling leaves the file unchanged.
    - Returning from the editor preserves the current file-browser path, selected entry, and current `TAB` view mode.
    - Desktop discovery remains the only path that launches SD desktop apps; opening a `.lua` file from the browser edits it as text rather than executing it.

  - [x] VERIFICATION: Build, flash, and fixture-backed on-device edit/save-back checklist
    Success criteria/tests to run:
    - `pio run -e lolin_s2_mini` completes with zero errors.
    - Flash via `bash scripts/flash.sh` using the DFU flow from TRUTH_FLASHING.md.
    - In the built-in Lua file browser, `New Folder/` still opens as a directory, `LEFT` and `BKSP` still return to parent, and `New Empty File` opens in the canonical read-write editor.
    - Editing an existing SD file and exiting with save persists the updated content when the file is reopened.
    - Canceling an edit session leaves the original file unchanged after reopening.
    - `blank_app (bad code).lua` still appears on the desktop and still follows the existing crash-popup path when launched from desktop discovery, but opening it from the browser edits it as text instead of executing it.
    - Exiting the editor returns directly to the Lua desktop/browser, preserving the current path, selection, and view mode.
    - `ESC` still transitions into Settings cleanly when no Lua-owned editor session is active.
    - Current limitation accepted for this milestone: FAT modified timestamps are not advanced yet because no RTC time source is installed; file content persistence is the required verification target for now.
