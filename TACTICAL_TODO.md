# TACTICAL_TODO.md
<!-- Machine-readable Implementation Contract -->

## CURRENT GOAL: ESP32-S2-Mini Hardware Port — Compile, Flash, Boot

- **TARGET_FILE:** `platformio.ini`, `src/config.h`, `src/hal.cpp`, `src/hal.h`
- **TRUTH_RELIANCE:** `TRUTH_HARDWARE.md` — Sections 0 (Main MCU), 1 (Screen), 2 (Keyboard), 3 (SD — not wired), 4 (Buzzers — not wired), 5 (Free pins)
- **TECHNICAL_CONSTRAINTS:**
  - MCU changed: ESP32-S2-Mini (4MB Flash, 2MB PSRAM) replaces ESP32-S3-N16-R8
  - Display changed: ST7920 128x64 SPI serial mode replaces ST7565 ERC12864 4-wire HW SPI
  - ST7920 VCC at 5V (VBUS), GPIO signals at 3.3V from ESP32-S2
  - ST7920 RST tied to 3V3 (no GPIO reset line) — use `U8X8_PIN_NONE`
  - ST7920 PSB tied to GND (hardware SPI mode select)
  - ST7920 contrast controlled by hardware pot on V0 — `u8g2.setContrast()` is a no-op (keep calls to minimize cascading changes)
  - Use U8g2 SW_SPI for ST7920 (more reliable than HW_SPI for ST7920 protocol)
  - Native USB CDC for serial monitor and firmware upload (no UART bridge chip)
  - Fresh PlatformIO on Debian — udev rules required for USB device access
  - SD card module: NOT YET WIRED — disable SD init in `setupHardware()`
  - Buzzers: NOT YET WIRED — comment out pin defines, keep as placeholders
  - Display rotation: default `U8G2_R0`; adjust after first visual test if needed
  - All prior SESSION_LOG entries reference obsolete S3 hardware — treat as historical only

- **ATOMIC_TASKS:**

  - [ ] TASK_1: Install PlatformIO udev rules and verify USB device visibility
    - Run: `curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules | sudo tee /etc/udev/rules.d/99-platformio-udev.rules`
    - Run: `sudo udevadm control --reload-rules && sudo udevadm trigger`
    - Verify with: `lsusb | grep -i espressif` (expect VID `303a`)
    - Verify with: `ls -la /dev/ttyACM*` or `ls -la /dev/ttyUSB*`
    - If no device visible: user must hold BOOT button, press RESET, release BOOT to enter download mode
    - Required Result: ESP32-S2-Mini USB device visible to OS

  - [ ] TASK_2: Rewrite `platformio.ini` for ESP32-S2-Mini board target
    - Required Content:
      ```ini
      [env:lolin_s2_mini]
      platform = espressif32
      board = lolin_s2_mini
      framework = arduino
      monitor_speed = 115200
      board_build.filesystem = spiffs
      build_flags =
          -DBOARD_HAS_PSRAM
          -DARDUINO_USB_CDC_ON_BOOT=1
      lib_deps =
          olikraus/U8g2 @ ^2.35.9
          chris--a/Keypad @ ^3.1.1
          https://github.com/DECE2183/libLua.git
      ```
    - Removed: `board_upload.flash_size`, `board_build.arduino.memory_type`, `board_build.partitions` (S2 Mini defaults handle 4MB flash)
    - Env name changes from `esp32-s3-devkitc-1` → `lolin_s2_mini`

  - [ ] TASK_3: Update `src/config.h` pin definitions per TRUTH_HARDWARE.md
    - Display pins:
      - `#define PIN_CS 38` ← LCD pin 4 (RS) — Chip Select
      - `#define PIN_SPI_SCLK 36` ← LCD pin 6 (E) — Serial Clock
      - `#define PIN_SPI_SID 35` ← LCD pin 5 (R/W) — Serial Data In (MOSI)
      - `#define PIN_BACKLIGHT 40` ← LCD pin 19 (BLA) — PWM backlight anode
      - Remove: `PIN_DC` (ST7920 SPI has no DC line)
      - Remove: `PIN_RST` (tied to 3V3 in hardware)
    - Keyboard matrix (TRUTH_HARDWARE Section 2):
      - Row pins top→bottom: `{3, 5, 7, 9}` (1st=GPIO3, 2nd=GPIO5, 3rd=GPIO7, 4th=GPIO9)
      - Col pins left→right: `{1, 2, 4, 6, 8}` (5th=GPIO1, 4th=GPIO2, 3rd=GPIO4, 2nd=GPIO6, 1st=GPIO8)
    - SD card pins: comment out with `// NOT YET WIRED` marker
    - Buzzer pins: comment out with `// NOT YET WIRED` marker

  - [ ] TASK_4: Rewrite `src/hal.cpp` for ST7920 display driver + new pin mapping
    - Display constructor (SW SPI):
      `U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, PIN_SPI_SCLK, PIN_SPI_SID, PIN_CS);`
    - Remove: `#define PIN_SPI_SCK 12` and `#define PIN_SPI_MOSI 11` (now in config.h)
    - Remove: `SPI.begin(...)` call (SW SPI needs no hardware SPI init)
    - Remove: `#include <SD.h>` and `SD.begin(...)` block (SD not wired)
    - Remove: `#include <SPI.h>` (not needed for SW SPI)
    - Update row pins: `byte rowPins[ROWS] = {3, 5, 7, 9};`
    - Update col pins: `byte colPins[COLS] = {1, 2, 4, 6, 8};`
    - Keep: `u8g2.setContrast(systemContrast)` — harmless no-op on ST7920
    - Keep: SPIFFS init (uses internal flash, unaffected by hardware change)
    - Keep: `systemContrast` and `systemBrightness` globals (avoids cascading changes to settings.cpp)
    - Backlight PWM: keep `ledcSetup`/`ledcAttachPin` using `PIN_BACKLIGHT` (verify LEDC API compiles)

  - [ ] TASK_5: Update `src/hal.h` to match new display type declaration
    - Change: `extern U8G2_ST7565_ERC12864_F_4W_HW_SPI u8g2;` → `extern U8G2_ST7920_128X64_F_SW_SPI u8g2;`
    - Keep: All other exports unchanged (`systemContrast`, `systemBrightness`, key scan functions)

  - [ ] VERIFICATION:
    - Compile: `pio run -e lolin_s2_mini` — zero errors
    - Flash: `pio run -e lolin_s2_mini -t upload` — upload success
    - Serial: `pio device monitor` — boot messages visible ("SPIFFS mounted", etc.)
    - Display: boot splash text visible and readable
    - Keyboard: Key Tester app responds to all 20 keys with correct labels
    - If display orientation wrong: note rotation fix (`U8G2_R0` → `U8G2_R2`) for next sprint
    - If LEDC API compile error: switch to `ledcAttach(PIN_BACKLIGHT, 5000, 8)` (Arduino-ESP32 v3.x API)

---
**STATUS:** PENDING
**PRIORITY:** CRITICAL — No feature work possible until hardware port is verified and board communicates
**PREVIOUS_HARDWARE:** ESP32-S3-N16-R8 + ST7565 ERC12864
**NEW_HARDWARE:** ESP32-S2-Mini + ST7920 128x64
**NOTE:** All SESSION_LOG entries predate this hardware switch and reference obsolete S3 pin assignments
