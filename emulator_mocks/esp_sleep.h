// [Revision: v1.0] [Path: emulator_mocks/esp_sleep.h] [Date: 2026-06-09]
// Description: ESP32 sleep SDK mock interface for the emulator.

#ifndef EMULATOR_ESP_SLEEP_H
#define EMULATOR_ESP_SLEEP_H

#include <cstdint>

inline void esp_sleep_enable_timer_wakeup(uint64_t time_in_us) {}

#endif // EMULATOR_ESP_SLEEP_H
