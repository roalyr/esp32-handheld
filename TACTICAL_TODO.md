# TACTICAL_TODO.md
<!-- Machine-readable Implementation Contract -->

## CURRENT GOAL: T9 Editor Overhaul + UX Polish v1.1.1

- **TARGET_FILES:** `src/config.h`, `src/hal.cpp`, `src/apps/settings.cpp`, `src/apps/settings.h`, `src/t9_predict.cpp`, `src/t9_predict.h`
- **TRUTH_RELIANCE:**
  - `TRUTH_HARDWARE.md` Section 0 — ESP32-S2-Mini, 4MB Flash, 2MB PSRAM
  - `TRUTH_HARDWARE.md` Section 1 — LCD ST7920 128x64, backlight on GPIO 40 (PWM)
  - `TRUTH_HARDWARE.md` Section 2 — Key matrix: 4×5, layout includes SHIFT, ALT, arrows, 0-9
- **TECHNICAL_CONSTRAINTS:**
  - ESP32-S2-Mini: 4MB flash, 2MB PSRAM
  - LCD: ST7920 128×64, backlight via LEDC PWM channel 0 (8-bit, 0-255)
  - Key matrix: 4 rows × 5 cols. Special keys: ESC(27), BKSP(8), TAB(9), ENTER(13), SHIFT(14), ALT(15), UP(16), DOWN(17), LEFT(18), RIGHT(19)
  - T9 dictionary: PROGMEM `t9_dict.h` — 705 digit sequences, 760 words, binary search via `T9Predict`
  - T9 editor currently lives in `settings.cpp` as a submenu using `T9Predict` (predictive) + inline multi-tap (ABC mode)
  - `T9Predict` only does exact-match lookup (digit sequence → words). No prefix matching exists.
  - Key repeat infrastructure exists in `hal.cpp` via `isRepeating()` + `keyRepeatMap[]`
  - Single-threaded 30 FPS loop; all state is global or in `SettingsApp` class
  - Text buffer is Arduino `String` with byte-index cursor; newlines are `\n`

- **ATOMIC_TASKS:**

  - [x] TASK_1: Version bump + default brightness
    - **`src/config.h`**: Change `FIRMWARE_VERSION` from `"1.1.0"` to `"1.1.1"`
    - **`src/hal.cpp`**: Change `systemBrightness = 255` to `systemBrightness = 38` (15% of 255 ≈ 38)
    - Required Result: Boot splash shows v1.1.1. Backlight starts at ~15%.

  - [x] TASK_2: Add prefix-matching to `T9Predict` for incremental word suggestion
    - **`src/t9_predict.h`**: Add new public methods:
      ```cpp
      int getPrefixCandidateCount() const;
      const char* getPrefixCandidate(int index) const;
      const char* getSelectedPrefixWord() const;
      void nextPrefixCandidate();
      void prevPrefixCandidate();
      ```
      Add private state:
      ```cpp
      int prefixCandidateCount;
      int prefixSelectedIdx;
      // Flat array of index positions for prefix matches (max 64 entries)
      struct PrefixMatch { int indexPos; int wordIdx; };
      PrefixMatch prefixMatches[64];
      void updatePrefixCandidates();
      ```
    - **`src/t9_predict.cpp`**: Implement `updatePrefixCandidates()`:
      - Scan `t9_index[]` for all entries whose key starts with the current digits
      - A key K starts with digits D if: K divided by 10^(len(K)-len(D)) == D (integer math)
      - Collect up to 64 (indexPos, wordIdx) pairs into `prefixMatches[]`
      - Priority: exact-length matches first (sorted by frequency/position), then longer matches
      - For each prefix match, take only the first word (index 0) from that sequence to keep the list manageable
      - Call `updatePrefixCandidates()` from `updateCandidates()` after the exact-match search
    - **`src/t9_predict.cpp`**: Implement `getPrefixCandidate(int index)`:
      - Look up `prefixMatches[index]`, read word from `t9_word_pool` at that position
      - Return the word truncated/represented at the current digit length (i.e., show only the first N chars where N = digitCount)
      - Actually: return the FULL candidate word — the editor will decide how to display it
    - **`src/t9_predict.cpp`**: Implement cycling: `nextPrefixCandidate()`, `prevPrefixCandidate()`, `getSelectedPrefixWord()`
    - Required Result: After pushing digits "842", `getPrefixCandidate(0)` returns "that" (or the best prefix match). `getCandidateCount()` still works for exact matches. Both APIs coexist.

  - [x] TASK_3: T9 mode shows letter-priority preview instead of digits
    - **`src/apps/settings.cpp`** in `t9HandleInput()` and `renderT9Editor()`:
      - When in T9 predictive mode and digits are entered, the preview text should show the best prefix word truncated to `digitCount` characters, NOT the raw digit sequence
      - Logic: if `t9predict.getPrefixCandidateCount() > 0`, get `getSelectedPrefixWord()`, take first `digitCount` chars as preview
      - If no prefix candidates exist at all, fall back to showing raw digits (number mode)
      - UP/DOWN in T9 mode cycles `nextPrefixCandidate()`/`prevPrefixCandidate()` (replaces exact-match cycling)
      - ENTER in T9 mode: if prefix candidates exist, commit the prefix word truncated to `digitCount` chars. If no candidates, commit the raw digits as literal text.
    - Required Result: Typing 8-4-2 shows "tha" (not "842"). Typing 8-4-2-8 shows "that". If digits have no dictionary prefix match, show digits.

  - [x] TASK_4: Cursor movement / space / newline / arrows commit the in-progress T9 word
    - **`src/apps/settings.cpp`** in `t9HandleInput()`:
      - LEFT, RIGHT, UP, DOWN, TAB (newline), KEY_ENTER: if T9 prediction is active (`t9predict.hasInput()`), commit the current preview word (truncated to digitCount) before performing the navigation/insertion action
      - 0 (space): commit current T9 word, THEN insert space (current behavior already does this for exact match — update to use prefix-based commit)
      - After commit, `t9predict.reset()` so new word entry starts fresh
    - Required Result: Moving cursor mid-word finalizes whatever letters are shown. No word "follows" the cursor.

  - [x] TASK_5: Three-mode SHIFT cycling (lower → First Cap → ALL CAPS)
    - **`src/apps/settings.h`**: Add `int t9ShiftMode;` (0=lower, 1=firstCap, 2=allCaps). Remove `bool t9Shifted;`.
    - **`src/apps/settings.cpp`**:
      - SHIFT key: cycle `t9ShiftMode = (t9ShiftMode + 1) % 3`
      - When committing T9 prediction: mode 0 = all lowercase, mode 1 = capitalize first letter, mode 2 = all uppercase
      - When committing multi-tap (ABC mode): mode 0 = lowercase char, mode 1 = uppercase current char, mode 2 = uppercase current char
      - Update `t9GetMultiTapChar()` to use `t9ShiftMode` instead of `t9Shifted`
      - Update `renderT9Editor()` header: show "abc"/"Abc"/"ABC" for shift indicator instead of "^"
    - Required Result: SHIFT cycles through three states visually indicated in header.

  - [x] TASK_6: Button 1 in T9 mode behaves like ABC mode (symbol cycling)
    - **`src/apps/settings.cpp`** in `t9HandleInput()`:
      - In T9 predictive mode, when key '1' is pressed:
        - If T9 prediction is active, commit current word first
        - Then enter a temporary multi-tap cycle for key '1' using `multiTapMap[1]` = `".,?!1"`
        - Use the same `t9TapKey`/`t9TapIndex`/`t9TapTime` mechanism already used by ABC mode
        - On timeout or different key, commit the symbol
      - Currently key '1' just inserts "." — replace with full multi-tap cycling
    - Required Result: In T9 mode, pressing 1 cycles through `.` `,` `?` `!` `1` with multi-tap timing.

  - [x] TASK_7: Allow cursor UP and DOWN movement in T9 editor
    - **`src/apps/settings.cpp`** in `t9HandleInput()`:
      - Currently UP/DOWN only cycle T9 candidates. Change: when T9 has NO active input (`!t9predict.hasInput()` and no pending multi-tap), UP/DOWN move cursor vertically
      - Implement vertical cursor movement: find current line (scan t9Text for `\n` boundaries), move to same column on adjacent line
      - When T9 IS active, UP/DOWN still cycle candidates (existing behavior for exact match, updated to prefix cycling per TASK_3)
    - **`src/apps/settings.cpp`** in `renderT9Editor()`:
      - Ensure scroll offset adjusts when cursor moves vertically (auto-scroll to keep cursor visible)
    - Required Result: Arrow keys navigate freely when no word is being composed.

  - [x] TASK_8: Ensure newline is `\n` in buffer; UTF-8 encoding on read/write (verified — already correct)
    - **`src/apps/settings.cpp`**:
      - Verify TAB key inserts literal `'\n'` (currently does: `t9Text.substring(0,t9Cursor) + "\n" + ...`) — CONFIRMED OK
      - Arduino `String` is already byte-oriented (UTF-8 pass-through). No conversion needed for ASCII/Latin content.
      - Ensure that when `t9Text` is read from or written to `appTransferString`, no character mangling occurs (no `\r\n` conversion, no encoding issues)
      - This task is primarily verification — if current code already handles `\n` correctly, mark as pass. If any `\r` appears, strip it.
    - Required Result: Buffer uses `\n` for newlines, UTF-8 bytes pass through unmodified.

  - [x] TASK_9: Remove redundant digit display from T9 editor footer
    - **`src/apps/settings.cpp`** in `renderT9Editor()`:
      - In the candidate bar section, remove the right-aligned digit sequence display:
        ```cpp
        // REMOVE these lines:
        const char* digs = t9predict.getDigits();
        int dw = u8g2.getStrWidth(digs);
        u8g2.drawStr(128 - dw - 1, 62, digs);
        ```
      - Keep the left-aligned candidate word + index display
    - Required Result: Footer shows only the word suggestion and index, no redundant digit string on the right.

  - [x] TASK_10: Make arrow keys, backspace, and Enter repeatable
    - **`src/hal.cpp`** in `keyRepeatMap[][]`:
      - Change ENTER from `false` to `true` (row 1, col 4)
      - BKSP is already `true` (row 0, col 4)
      - UP is already `true` (row 2, col 4)
      - DOWN is already `true` (row 3, col 4)
      - LEFT is already `true` (row 3, col 1)
      - RIGHT is already `true` (row 3, col 3)
      - Verify current values; only ENTER needs changing
    - Required Result: Holding ENTER, arrows, or backspace triggers key repeat after 400ms delay at 100ms rate.

  - [x] TASK_11: ENTER in T9 mode commits word even with no dictionary match
    - **`src/apps/settings.cpp`** in `t9HandleInput()` for KEY_ENTER:
      - Current: if T9 has input but no candidate, nothing happens (no commit)
      - New: if `t9predict.hasInput()` and `getPrefixCandidateCount() == 0`, commit the raw digit string as literal text
      - This ensures the user can always "accept" whatever is shown (letters if match exists, digits if no match)
    - Required Result: Pressing ENTER always commits the current preview, whether it's a word or digits.

  - [x] TASK_12: ALT cycles between T9, ABC, and 123 modes
    - **`src/apps/settings.h`**: Add `enum T9InputMode { MODE_T9, MODE_ABC, MODE_123 }; T9InputMode t9InputMode;`. Remove `bool t9MultiTap;`.
    - **`src/apps/settings.cpp`**:
      - ALT key: cycle `t9InputMode` through `MODE_T9 → MODE_ABC → MODE_123 → MODE_T9`
      - Commit any pending input before switching (existing behavior)
      - `MODE_T9`: predictive mode (current non-multiTap behavior, updated per TASK_2/3)
      - `MODE_ABC`: multi-tap letter mode (current multiTap behavior)
      - `MODE_123`: direct digit insertion — pressing 0-9 immediately inserts that digit character. No multi-tap, no prediction. SHIFT/cycling irrelevant.
      - Update `renderT9Editor()` header to show "T9"/"ABC"/"123" for current mode
      - Update footer hints per mode
      - Replace all `t9MultiTap` checks with `t9InputMode == MODE_ABC` (or appropriate mode check)
    - Required Result: ALT cycles through three input modes, each with distinct behavior.

  - [ ] VERIFICATION: Compile + functional test
    - `pio run -e lolin_s2_mini` — zero errors, zero warnings (or only benign warnings)
    - Flash to device via `bash scripts/flash.sh`
    - **Test checklist:**
      1. Boot shows v1.1.1, screen starts at ~15% brightness
      2. Settings → Backlight starts at 15%
      3. T9 Editor: T9 mode — type 8-4-2-8 → preview shows "t","th","tha","that" progressively
      4. T9 Editor: pressing arrow/space/enter mid-word commits current preview
      5. T9 Editor: SHIFT cycles lower→Abc→ABC (visible in header)
      6. T9 Editor: key 1 in T9 mode cycles symbols (.,?!1)
      7. T9 Editor: UP/DOWN arrows move cursor between lines when no word active
      8. T9 Editor: newline inserts \n, text displays correctly
      9. T9 Editor: no digit string on right side of footer
      10. T9 Editor: ENTER commits digits as text when no dictionary match
      11. T9 Editor: ALT cycles T9→ABC→123
      12. Arrow keys + BKSP + ENTER repeat when held

  - [x] TASK_6: Update `src/apps/settings.cpp` — show SD card mount status and space info
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
