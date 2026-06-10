<!--
PROJECT: ESP32-Handheld
MODULE: TRUTH_PROJECT.md
STATUS: [Level 1 - Core Truth]
-->

# Project Stack And Context
- **Platform**: ESP32-S2 Mini (Production), ESP32-S3 (Bring-up/Future)
- **Framework**: PlatformIO (Arduino framework core)
- **Language**: C++17 (Firmware), Lua 5.4 (Embedded Applications)
- **Display**: LCD 12864-20M 128X64 ST7920 (Monochrome)
- **Input**: 4x5 Matrix Keyboard
- **Storage**: SPI SD Card
- **Emulation**: Native SDL2-based emulator (`pio run -e emulator`) for rapid UI/Lua iteration without hardware flashing.

# Workflow And Scope Boundary
1. **Platform Independence**: Game logic and UI should be written in Lua (`data/*.lua`) and tested via the emulator. Core systems and hardware bindings are in C++.
2. **Emulation vs Hardware**: Emulator (`env:emulator`) is authoritative for UI layout and app logic. Hardware (`env:lolin_s2_mini`) is authoritative for pin timing, matrix scanning, and SPI reliability.

# Validation Boundary
1. **Manual Validation**: Required for screen refresh rates, matrix keyboard responsiveness, and overall system feel. Run via emulator first, then hardware.
2. **Automated Limits**: Do not build automated testing for visual output; visual layout and font metrics (`gfx` / `ui`) are manually verified.

# Global Technical Constraints
1. Avoid dynamic memory allocation in the main firmware loop.
2. The emulator must mimic hardware constraints as much as possible, including input polling during low FPS conditions.
3. Keep the firmware modular so the ESP32-S2 and ESP32-S3 paths remain clean and isolated.
