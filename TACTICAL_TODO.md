## CURRENT GOAL: Canonical T9 Editor/Viewer Rebase v1.4
- TARGET_FILE: src/apps/t9_editor.cpp
- TRUTH_RELIANCE: TRUTH_HARDWARE.md Section 0 (ESP32-S2-Mini memory/display limits), Section 1 (128x64 ST7920 display), Section 2 (4x5 keyboard and special keys); TRUTH_FLASHING.md Quick Reference + Flashing Procedure
- TECHNICAL_CONSTRAINTS:
  - TRUTH_PROJECT.md is not present in workspace; no additional constraints may be inferred beyond explicit Truth files.
- ATOMIC_TASKS:
  - [ ] TASK_1: Reactivate the dormant editor app as the single live editor/viewer owner.
    Required signatures:
    - platformio.ini:
      - Re-enable src/apps/t9_editor.cpp, src/apps/yes_no_prompt.cpp, and src/app_transfer.cpp in build_src_filter.
      - Keep src/apps/file_browser.cpp excluded in this milestone.
    - src/main.cpp:
      - Add concrete T9EditorApp instance ownership.
      - Implement switchApp(App* newApp).
      - Route MODE_SETTINGS input, update, and render through the active app pointer.
    - src/apps/settings.cpp:
      - Launch T9EditorApp via app-transfer state instead of entering the inline in-settings editor path.
    Required behavior:
    - Entering T9 Editor from Settings launches T9EditorApp in scratch read-write mode.
    - Returning from T9EditorApp lands back in Settings cleanly.
    - The inline Settings-based editor is no longer the active edit path.

  - [ ] TASK_2: Rebase the canonical editor app onto the current T9 UX and add the scalable viewer contract.
    Required signatures:
    - src/apps/t9_editor.h:
      - Add explicit read-write vs read-only open state.
      - Add a document/source seam that supports in-memory buffers now and future paged providers.
      - Add four TAB view modes matching the live editor UX.
    - src/app_transfer.h:
      - Carry editor/viewer open mode, payload buffer, and label/path metadata.
    - src/apps/t9_editor.cpp:
      - Reuse the predictive/editor behavior currently implemented in src/apps/settings.cpp and src/t9_predict.cpp.
    Required behavior:
    - Tap 0 inserts space; hold 0 inserts a single literal 0.
    - Key 1 symbol cycling is reordered so common coding symbols appear earlier.
    - TAB cycles VIEW_FULL -> VIEW_FULL_LINENO -> VIEW_MIN_LINENO -> VIEW_MIN.
    - Read-only mode preserves cursor movement, TAB cycling, and scrolling but blocks text mutation.

  - [ ] VERIFICATION: Build and interaction checklist
    Success criteria/tests to run:
    - pio run -e lolin_s2_mini completes with zero errors.
    - Launching T9 Editor from Settings opens T9EditorApp and returns cleanly.
    - A buffer exceeding 64 wrapped or logical lines scrolls correctly past line 64.
    - Line-number gutter widens for 2-digit and 3-digit counts with no overdraw.
    - Read-write mode preserves the current 0/1/TAB behavior contract.
    - Read-only mode blocks edits while preserving navigation and view cycling.
