## CURRENT GOAL: Shift Selection + Clipboard/Undo Editing
- TARGET_FILE: src/apps/t9_editor.cpp
- TRUTH_RELIANCE: TRUTH_HARDWARE.md Section 0 (ESP32-S2-Mini memory and PSRAM budget for bounded editor state), Section 0.1 and Section 3 (dedicated SD SPI bus and SD CS pin for existing clipboard/history storage), Section 1 (128x64 ST7920 display constraints for footer density and lower-third popup layout), Section 2 (4x5 key matrix including Shift and arrow navigation); TRUTH_FLASHING.md Quick Reference + Flashing Procedure
- TECHNICAL_CONSTRAINTS:
  - TRUTH_PROJECT.md is not present in workspace; no additional technical constraints may be inferred beyond explicit Truth files.
- ATOMIC_TASKS:
  - [x] TASK_1: Turn the existing long-press Shift stub in the canonical editor into a real selection mode without breaking short-press Shift case cycling.
    Required signatures:
    - src/apps/t9_editor.h:
      - Keep `bool selectionMode;` and `bool shiftTapPending;` as the existing selection-mode toggle surface owned by `T9EditorApp`.
      - Keep `EditorOpenMode`, `DocumentSourceKind`, `void requestExit(bool saveRequested);`, and the current clipboard/history helper signatures intact.
      - Add explicit selection anchor/range state and any local helper methods needed to extend, clear, and query the active selection entirely inside `T9EditorApp`.
    - src/apps/t9_editor.cpp:
      - Long-press Shift must toggle selection mode on and off; short-tap Shift must continue to cycle case on release exactly as it does now.
      - When selection mode is active, `UP/DOWN` must extend the selection endpoint through the existing vertical cursor-movement path, and `LEFT/RIGHT` must extend the selection endpoint by characters from the original cursor anchor.
      - Selection must track the real document byte range across wrapped lines and newlines rather than acting as a visual-only cursor flag.
      - Leaving selection mode via long-press Shift must return the editor to normal navigation/input behavior and clear transient selection-only UI state safely.
      - The editor footer must switch to a compact Shift-mode hint describing arrow-select plus `1/2/3` copy/cut/paste and `7/9` undo/redo, truncated safely to fit the active system font on the 128x64 screen.
    Required behavior:
    - Selection mode starts from the current cursor position at the moment long-press Shift is recognized.
    - While selection mode is active, arrow keys must not fall through to their normal cursor/navigation/editor-mode side effects.
    - Holding Shift again exits selection mode and restores normal editor behavior.

  - [ ] TASK_2: Add selection actions, a lower-third clipboard chooser, and bounded session undo/redo on top of the existing editor/clipboard infrastructure.
    Required signatures:
    - src/apps/t9_editor.h:
      - Keep `bool writeClipboardSlot(const String& content, int& writtenSlot, String& error);` and `bool readClipboardSlot(int slot, String& content, String& error) const;` as the canonical clipboard persistence surface.
      - Keep `bool recordPageSnapshot(const char* reason, String& error);` for save-history behavior, but do not repurpose SD save history as interactive per-action undo/redo.
      - Add explicit session-local undo/redo state and clipboard-popup state inside `T9EditorApp` rather than introducing a second app or external modal owner.
    - src/apps/t9_editor.cpp:
      - In selection mode, key `1` must copy the active selection into the rotating clipboard store without changing the document.
      - In selection mode, key `2` must cut the active selection: copy first, remove the selected range, collapse the cursor to the resulting insertion point, and mark the document dirty.
      - In selection mode, key `3` must open a clipboard popup in the lower section of the editor screen, ordered newest-first, while keeping the currently edited line and cursor visible above it; `ENTER` pastes the highlighted entry and closes the popup, `ESC`/`BKSP` cancels it.
      - Paste must insert at the cursor when no selection is active and replace the active selection when one exists.
      - Key `7` must undo and key `9` must redo bounded in-session edits for RW documents, including selection-driven cut/paste side effects where applicable.
      - Copy/cut/paste/undo/redo must be blocked safely in RO mode.
      - Reuse existing GUI metrics/fonts/footer layout where practical; do not add a fullscreen modal that hides the active editing line for clipboard choice.
    Required behavior:
    - The clipboard chooser must show the latest clipboard entry at the top.
    - After a paste commit, the clipboard chooser must close immediately and return to the normal editor view.
    - Undo and redo must be deterministic for capped RW sessions and must not exceed the existing 16 KB editor cap.
    - Existing whole-file SD save history must remain intact for save persistence, separate from interactive undo/redo.

  - [ ] VERIFICATION: Build with ~/.platformio/penv/bin/pio run -e lolin_s2_mini; user manually flashes with bash scripts/flash.sh per TRUTH_FLASHING.md; on-device verify that short-tap Shift still cycles case, long-press Shift toggles selection mode, the footer switches to the new Shift-mode command hint, `UP/DOWN` extend selection through lines, `LEFT/RIGHT` extend selection through characters, key `1` copies, key `2` cuts, key `3` opens a lower-third newest-first clipboard popup without hiding the active line/cursor, `ENTER` pastes and closes the popup, `ESC`/`BKSP` cancels it, key `7` undoes, key `9` redoes, normal editor behavior returns after a second long-press Shift, RO sessions stay non-destructive, and capped RW sessions still save correctly afterward.
