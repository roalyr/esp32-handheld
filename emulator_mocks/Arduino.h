// [Revision: v1.0] [Path: emulator_mocks/Arduino.h] [Date: 2026-06-09]
// Description: Mock Arduino header for running the ESP32 firmware on a desktop environment.

#ifndef EMULATOR_ARDUINO_H
#define EMULATOR_ARDUINO_H

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cmath>

#define HIGH 0x1
#define LOW  0x0

#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2

#define PROGMEM
#define F(x) x

#include <algorithm>
#include <stdarg.h>
using std::min;
using std::max;
 
#define INPUT_PULLDOWN 0x3

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

typedef uint8_t byte;
typedef bool boolean;

// String class implementation mirroring Arduino String
class String {
public:
    std::string val;

    String() : val("") {}
    String(const char* s) : val(s ? s : "") {}
    String(const std::string& s) : val(s) {}
    String(char c) : val(1, c) {}
    String(int n) : val(std::to_string(n)) {}
    String(unsigned int n) : val(std::to_string(n)) {}
    String(long n) : val(std::to_string(n)) {}
    String(unsigned long n) : val(std::to_string(n)) {}
    String(long long n) : val(std::to_string(n)) {}
    String(unsigned long long n) : val(std::to_string(n)) {}
    String(double n) : val(std::to_string(n)) {}

    String(unsigned long n, int base) {
        if (base == 16) {
            char buf[32];
            sprintf(buf, "%lx", n);
            val = buf;
        } else if (base == 8) {
            char buf[32];
            sprintf(buf, "%lo", n);
            val = buf;
        } else if (base == 2) {
            std::string s = "";
            do {
                s = (n & 1 ? "1" : "0") + s;
                n >>= 1;
            } while (n > 0);
            val = s;
        } else {
            val = std::to_string(n);
        }
    }
    String(int n, int base) : String(static_cast<unsigned long>(n), base) {}
    String(unsigned int n, int base) : String(static_cast<unsigned long>(n), base) {}

    const char* c_str() const { return val.c_str(); }
    size_t length() const { return val.length(); }
    bool isEmpty() const { return val.empty(); }

    char operator[](size_t index) const { return val[index]; }
    char& operator[](size_t index) { return val[index]; }

    String& operator+=(const String& rhs) { val += rhs.val; return *this; }
    String& operator+=(const char* rhs) { val += (rhs ? rhs : ""); return *this; }
    String& operator+=(char rhs) { val += rhs; return *this; }
    String& operator+=(int rhs) { val += std::to_string(rhs); return *this; }

    bool operator==(const String& rhs) const { return val == rhs.val; }
    bool operator==(const char* rhs) const { return val == (rhs ? rhs : ""); }
    bool operator!=(const String& rhs) const { return val != rhs.val; }
    bool operator!=(const char* rhs) const { return val != (rhs ? rhs : ""); }
    bool operator<(const String& rhs) const { return val < rhs.val; }

    String substring(int left, int right = -1) const {
        if (left < 0) left = 0;
        if (left >= (int)val.length()) return String("");
        if (right == -1 || right > (int)val.length()) right = (int)val.length();
        if (right <= left) return String("");
        return String(val.substr(left, right - left));
    }

    int indexOf(char c, int fromIdx = 0) const {
        if (fromIdx < 0) fromIdx = 0;
        size_t res = val.find(c, fromIdx);
        return (res == std::string::npos) ? -1 : (int)res;
    }

    int indexOf(const String& str, int fromIdx = 0) const {
        if (fromIdx < 0) fromIdx = 0;
        size_t res = val.find(str.val, fromIdx);
        return (res == std::string::npos) ? -1 : (int)res;
    }

    int lastIndexOf(char c) const {
        size_t res = val.rfind(c);
        return (res == std::string::npos) ? -1 : (int)res;
    }

    bool startsWith(const String& prefix) const {
        if (prefix.val.length() > val.length()) return false;
        return val.compare(0, prefix.val.length(), prefix.val) == 0;
    }

    bool endsWith(const String& suffix) const {
        if (suffix.val.length() > val.length()) return false;
        return val.compare(val.length() - suffix.val.length(), suffix.val.length(), suffix.val) == 0;
    }

    void remove(size_t index, size_t count = 1) {
        if (index < val.length()) {
            val.erase(index, count);
        }
    }

    bool reserve(size_t size) {
        val.reserve(size);
        return true;
    }

    bool concat(const String& str) {
        val += str.val;
        return true;
    }
    bool concat(const char* str) {
        val += (str ? str : "");
        return true;
    }

    void replace(const String& find_str, const String& replace_str) {
        if (find_str.isEmpty()) return;
        size_t pos = 0;
        while ((pos = val.find(find_str.val, pos)) != std::string::npos) {
            val.replace(pos, find_str.val.length(), replace_str.val);
            pos += replace_str.val.length();
        }
    }

    int toInt() const {
        try {
            return std::stoi(val);
        } catch (...) {
            return 0;
        }
    }

    void toLowerCase() {
        for (char& c : val) c = std::tolower((unsigned char)c);
    }

    void toUpperCase() {
        for (char& c : val) c = std::toupper((unsigned char)c);
    }

    void trim() {
        size_t first = val.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            val = "";
            return;
        }
        size_t last = val.find_last_not_of(" \t\r\n");
        val = val.substr(first, last - first + 1);
    }
};

inline String operator+(String lhs, const String& rhs) { lhs += rhs; return lhs; }
inline String operator+(String lhs, const char* rhs) { lhs += rhs; return lhs; }
inline String operator+(String lhs, char rhs) { lhs += rhs; return lhs; }

inline void yield() {}

// ESP System Mock
class EspClass {
public:
    uint32_t getHeapSize() { return 327680; }
    uint32_t getFreeHeap() { return 250000; }
    uint32_t getMinFreeHeap() { return 200000; }
    uint32_t getMaxAllocHeap() { return 100000; }
    uint32_t getPsramSize() { return 2 * 1024 * 1024; }
    uint32_t getFreePsram() { return 1800000; }
    uint32_t getMinFreePsram() { return 1500000; }
    uint32_t getMaxAllocPsram() { return 1000000; }
    uint8_t getChipRevision() { return 3; }
    const char* getSdkVersion() { return "v4.4.4"; }
    uint64_t getEfuseMac() { return 0x112233445566ULL; }
    void restart() { exit(0); }
};

extern EspClass ESP;

// Serial Emulator
class SerialEmulator {
public:
    void begin(unsigned long speed) {}
    operator bool() { return true; }
    void print(const String& s) { std::cout << s.c_str(); std::cout.flush(); }
    void print(const char* s) { std::cout << (s ? s : ""); std::cout.flush(); }
    void print(char c) { std::cout << c; std::cout.flush(); }
    void print(int n) { std::cout << n; std::cout.flush(); }
    void print(unsigned int n) { std::cout << n; std::cout.flush(); }
    void print(unsigned long n) { std::cout << n; std::cout.flush(); }
    
    void println() { std::cout << "\n"; std::cout.flush(); }
    void println(const String& s) { std::cout << s.c_str() << "\n"; std::cout.flush(); }
    void println(const char* s) { std::cout << (s ? s : "") << "\n"; std::cout.flush(); }
    void println(char c) { std::cout << c << "\n"; std::cout.flush(); }
    void println(int n) { std::cout << n << "\n"; std::cout.flush(); }
    void println(unsigned int n) { std::cout << n << "\n"; std::cout.flush(); }
    void println(unsigned long n) { std::cout << n << "\n"; std::cout.flush(); }

    void printf(const char* format, ...);
};

extern SerialEmulator Serial;

// Arduino basic GPIO functions
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);

// ESP32 ledc stubs
void ledcSetup(uint8_t chan, uint32_t freq, uint8_t res);
void ledcAttachPin(uint8_t pin, uint8_t chan);
void ledcWrite(uint8_t chan, uint32_t val);

// Keyboard emulator input state
extern bool emulatorKeys[256];
void setEmulatorKey(char key, bool pressed);

#endif // EMULATOR_ARDUINO_H
