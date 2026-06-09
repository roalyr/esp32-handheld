// [Revision: v1.0] [Path: emulator_mocks/Arduino.cpp] [Date: 2026-06-09]
// Description: Implementation of mock Arduino functions and key matrix mapping for the emulator.

#include "Arduino.h"
#include <thread>
#include <chrono>
#include <stdarg.h>

SerialEmulator Serial;

static uint8_t pinModes[50] = {0};
static uint8_t pinStates[50] = {0};

bool emulatorKeys[256] = {false};

void setEmulatorKey(char key, bool pressed) {
    emulatorKeys[(unsigned char)key] = pressed;
}

void pinMode(uint8_t pin, uint8_t mode) {
    if (pin < 50) pinModes[pin] = mode;
}

void digitalWrite(uint8_t pin, uint8_t val) {
    if (pin < 50) pinStates[pin] = val;
}

int digitalRead(uint8_t pin) {
    if (pin < 50) {
        // Row pins in config.h key matrix: 3, 7, 5, 9
        int r = -1;
        if (pin == 3) r = 0;
        else if (pin == 7) r = 1;
        else if (pin == 5) r = 2;
        else if (pin == 9) r = 3;

        if (r >= 0 && pinModes[pin] == INPUT_PULLUP) {
            // colPins in config.h: 1, 2, 4, 6, 8
            uint8_t cols[] = {1, 2, 4, 6, 8};
            for (int c = 0; c < 5; c++) {
                uint8_t colPin = cols[c];
                if (pinModes[colPin] == OUTPUT && pinStates[colPin] == LOW) {
                    extern char keyMap[4][5];
                    char mappedKey = keyMap[r][c];
                    if (emulatorKeys[(unsigned char)mappedKey]) {
                        return LOW; // Connect to ground (LOW) when key is pressed
                    }
                }
            }
            return HIGH;
        }
        return pinStates[pin];
    }
    return HIGH;
}

void SerialEmulator::printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
}

unsigned long millis() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return (unsigned long)duration.count();
}

unsigned long micros() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
    return (unsigned long)duration.count();
}

void delay(unsigned long ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void delayMicroseconds(unsigned int us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

void ledcSetup(uint8_t chan, uint32_t freq, uint8_t res) {}
void ledcAttachPin(uint8_t pin, uint8_t chan) {}
void ledcWrite(uint8_t chan, uint32_t val) {}

EspClass ESP;
