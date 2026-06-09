## CURRENT GOAL: ESP32-S3 N16R8 Bring-Up - Standalone GPIO Blink Diagnostic
- TARGET_FILE: platformio.ini
- TRUTH_RELIANCE: TRUTH_HARDWARE.md Section 0, Section 0.1, Section 1, Section 2, and Section 3 define the current ESP32-S2-Mini production wiring and therefore justify keeping the first ESP32-S3 step isolated from the live firmware path; TRUTH_FLASHING.md Quick Reference, Why DFU (Not Serial), and Architecture Notes are explicitly LOLIN S2 Mini specific and must not be reused as the ESP32-S3 flashing contract for this milestone.
- TECHNICAL_CONSTRAINTS:
  - TRUTH_PROJECT.md is not present in workspace; no project-global technical constraints are available beyond the explicit TRUTH_*.md files.
- ATOMIC_TASKS:
  - [x] TASK_1: Add a dedicated non-default ESP32-S3 diagnostic build target that preserves the working ESP32-S2 environment and compiles only a standalone GPIO probe program.
    Required signatures:
    - platformio.ini:
      - Keep `[env:lolin_s2_mini]` intact as the current production firmware target.
      - Add `[env:esp32-s3-n16r8_diag]` using `board = esp32-s3-devkitc-1`, `board_upload.flash_size = 16MB`, `board_build.arduino.memory_type = qio_opi`, `board_build.partitions = default_16MB.csv`, `-DBOARD_HAS_PSRAM`, `-DARDUINO_USB_CDC_ON_BOOT=1`, and a dedicated compile-time define for the diagnostic build.
      - The diagnostic environment must use `build_src_filter` or an equivalent mechanism so it does not link the current LCD, Lua, SD, or Settings application stack.
    - Source layout:
      - Add a standalone diagnostic entry translation unit for the ESP32-S3 probe path instead of reusing the full application boot path in `src/main.cpp`.
      - The diagnostic pin manifest must be explicit and data-driven in code as a candidate-pin table with skip metadata, not handwritten as repeated one-off `pinMode()` calls.
    Required behavior:
    - Recover the historical ESP32-S3 project pin assignments from git history first and treat them as occupied reference pins, not as the complete safe-to-blink list.
    - Preserve the existing ESP32-S2 build as the default known-good path while the ESP32-S3 probe target is being brought up.

  - [x] TASK_2: Implement the standalone ESP32-S3 GPIO blink loop so each probe-safe candidate pin is announced over USB CDC serial, blinked in isolation, and then returned to a neutral state before the next candidate.
    Required signatures:
    - Standalone diagnostic source:
      - `void setup()` must initialize `Serial` and any probe-state needed for the candidate sweep.
      - `void loop()` must own the full diagnostic cycle and must not call the normal firmware `setupHardware()` or enter the Lua or Settings runtime.
      - Add local helper functions for candidate initialization, per-pin blink execution, and serial reporting; keep these helpers scoped to the diagnostic translation unit.
    Required behavior:
    - Exclude flash, PSRAM, USB, and other non-probe-safe ESP32-S3 pins from the active blink table; if a GPIO is deliberately skipped, the serial output must state the skip reason instead of silently omitting it.
    - Blink one GPIO at a time with a clearly human-observable dwell interval and print the GPIO number before each cycle.
    - Return each completed GPIO to `INPUT` or another documented neutral safe state before advancing.
    - Keep the diagnostic independent of LCD, SD card, keypad, and buzzer assumptions so it remains usable even if those circuits or header holes are damaged.

  - [ ] VERIFICATION: `~/.platformio/penv/bin/pio run -e esp32-s3-n16r8_diag` succeeds; user manually flashes the ESP32-S3 board with their chosen method; serial output enumerates the candidate table in order and reports any intentional skips; manual probing confirms which GPIO pads still toggle cleanly before any full ESP32-S3 application port or flashing-script update is attempted.
