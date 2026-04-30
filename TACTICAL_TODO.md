## CURRENT GOAL: Simplified RO Pager + Capped RW Editor
- TARGET_FILE: src/apps/t9_editor.cpp
- TRUTH_RELIANCE: TRUTH_HARDWARE.md Section 0 (ESP32-S2-Mini memory and PSRAM), Section 0.1 and Section 3 (dedicated SD SPI bus and SD CS pin), Section 1 (128x64 ST7920 display constraints), Section 2 (4x5 key matrix and modifier/navigation keys); TRUTH_FLASHING.md Quick Reference + Flashing Procedure
- TECHNICAL_CONSTRAINTS:
  - TRUTH_PROJECT.md is not present in workspace; no additional constraints may be inferred beyond explicit Truth files.
- ATOMIC_TASKS:
  - [ ] TASK_1: Expose read-only versus read-write file opening clearly in the Lua file browser, enforce the new open policy, and move editor mode-cycling off raw Alt.
    Required signatures:
    - src/lua_vm.cpp:
      - Keep ui.viewFile(path, label) and ui.editFile(path, label) as the canonical RO/RW launch bindings.
      - Keep input.held(key) available to Lua so the browser can detect modifier-assisted launches.
    - src/lua_scripts.cpp:
      - Regular files must open in read-only mode on Enter and read-write mode on Alt+Enter.
      - File-browser footer and any related modal/help text must explicitly advertise the RO/RW mapping.
      - If an RW open is refused by the capped policy, show a clear message and leave the user in the browser.
    - src/apps/t9_editor.h:
      - Keep EditorOpenMode, DocumentSourceKind, and void requestExit(bool saveRequested);
      - Add explicit constants or helpers for the simplified RO page size and RW file-size cap only if needed by TASK_1 launch gating.
    - src/apps/t9_editor.cpp:
      - Replace raw Alt mode-cycling with Alt+Tab mode-cycling inside the editor.
      - Add a single global maximum editable-file-size constant for RW opens, initially 16 KB.
      - Reject RW opens above that cap with a clear error path while leaving RO opens available.
    Required behavior:
    - There must be an obvious user-visible way to open files in RO versus RW mode.
    - Enter opens regular files in RO mode.
    - Alt+Enter opens regular files in RW mode.
    - Raw Alt must stop cycling editor input modes by itself.
    - Alt+Tab must cycle T9/ABC/123 modes inside the editor.
    - Files larger than the global RW cap must be refused for RW open and remain available in RO mode.

  - [ ] TASK_2: Replace the current paged-session core with a simple split architecture: a fixed-size RO pager for large files and a whole-buffer RW editor with whole-file SD snapshots.
    Required signatures:
    - src/apps/t9_editor.h:
      - Simplify state so RO paging and capped RW whole-buffer editing use distinct helpers instead of the current `/.t9sys/work/` chunk-session model.
      - Keep history and clipboard helpers only as required by the simplified design.
    - src/apps/t9_editor.cpp:
      - Implement a true RO pager using a Settings-selectable fixed page size (2048, 1024, 512, or 256 bytes; default 512) and direct source-file reads.
      - RO mode must allow vertical scrolling within the current page independently from page-to-page navigation.
      - RW mode must load the whole file into RAM only when the file is within the 16 KB cap.
      - RW save/exit-save must write the whole text file back in one pass; page-level save prompts must not control RW behavior anymore.
      - Replace chunk-local history with whole-file SD snapshots for capped RW files.
      - Treat the current `/.t9sys/work/` session-chunk design as superseded by this simplified milestone.
    - src/lua_scripts.cpp:
      - Keep browser refresh behavior correct after RO exit, RW cancel, and RW save.
      - Remove stale prompts that still imply raw Alt behavior inconsistent with TASK_1.
    Required behavior:
    - Large files must be openable in RO mode without loading the entire file into RAM.
    - RO paging must use the currently selected fixed page size from Settings (2048, 1024, 512, or 256 bytes), defaulting to 512 bytes.
    - RW editing must be limited by a global 16 KB file-size cap.
    - RW history must use whole-file snapshots on SD with sequence-based naming and compact manifests.
    - The broken page-save transition/footer corruption path from the current paged-session prototype must be removed from the active RW flow.

  - [ ] VERIFICATION: Build with ~/.platformio/penv/bin/pio run -e lolin_s2_mini; flash with bash scripts/flash.sh per TRUTH_FLASHING.md; on-device verify that the file browser clearly shows Enter=RO and Alt+Enter=RW, large files open reliably in RO mode with the selected fixed page size and usable in-page scrolling, Settings exposes RO page size choices of 2048/1024/512/256 bytes with 512 as the default, RW opens are refused above the 16 KB cap with a clear message, capped RW files save correctly as whole files, raw Alt no longer cycles modes by itself, Alt+Tab cycles T9/ABC/123 modes, whole-file RW history snapshots are created on SD, and the earlier failures around invisible RO mode, broken page-save transitions, page-split mismatch, and missing final save persistence are resolved.
