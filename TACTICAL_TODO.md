# TACTICAL_TODO.md
<!-- Machine-readable Implementation Contract -->

## CURRENT GOAL: GPIO Pin Configuration Alignment

- **TARGET_FILE:** `src/config.h`, `src/hal.cpp`
- **TRUTH_RELIANCE:** `TRUTH_GPIO_PINS_ASSIGNEMNT.md` - Sections 1 (Screen), 2 (Keyboard)
- **TECHNICAL_CONSTRAINTS:**
  - ESP32-S3-N16-R8 pinout limitations
  - GPIO38 broken (lifted pad) - must remain unused
  - GPIOs 19, 20 (USB), 48 (RGB LED) system reserved
  - Display uses HW SPI (shared bus with future SD card)

- **ATOMIC_TASKS:**
  - [x] TASK_1: Update `src/config.h` display pin definitions to match TRUTH
    - Change `PIN_DC` from `5` → `13`
    - Change `PIN_RST` from `4` → `14`
    - Add `PIN_BACKLIGHT` as `9` (for future PWM control)
    - Required Signatures: `#define PIN_DC 13`, `#define PIN_RST 14`, `#define PIN_BACKLIGHT 9`
    - **REVERTED**: TRUTH file incorrect. Working values: DC=5, RST=4
  
  - [x] TASK_2: Update `src/hal.cpp` keyboard column pins to match TRUTH wiring
    - Change `colPins[COLS]` from `{1, 2, 6, 7, 15}` → `{1, 2, 45, 44, 43}`
    - Order: 1st=GPIO1, 2nd=GPIO2, 3rd=GPIO45, 4th=GPIO44(RX), 5th=GPIO43(TX)
    - Required Signature: `byte colPins[COLS] = {1, 2, 45, 44, 43};`
  
  - [x] TASK_3: Add SD card pin definitions to `src/config.h` (prepare for future wiring)
    - Add `#define PIN_SD_CS 7`
    - Add `#define PIN_SD_MISO 8`
    - Note: SCK(12) and MOSI(11) shared with display SPI
    - Required Signatures: `#define PIN_SD_CS 7`, `#define PIN_SD_MISO 8`
  
  - [x] TASK_4: Add buzzer pin definitions to `src/config.h` (prepare for future wiring)
    - Add `#define PIN_BUZZER_1 4`
    - Add `#define PIN_BUZZER_2 5`
    - Add `#define PIN_BUZZER_3 6`
    - Required Signatures: `#define PIN_BUZZER_1 4`, `#define PIN_BUZZER_2 5`, `#define PIN_BUZZER_3 6`

  - [ ] VERIFICATION:
    - Compile succeeds: `pio run -e esp32-s3-devkitc-1`
    - All pin definitions in `config.h` match `TRUTH_GPIO_PINS_ASSIGNEMNT.md`
    - Keyboard matrix responds correctly after flash (test with Key Tester app)
    - Display initializes without artifacts

---
**STATUS:** PENDING
**PRIORITY:** CRITICAL (Hardware foundation must be correct before feature work)
**ESTIMATED_EFFORT:** 15 minutes
