// [Revision: v1.0] [Path: emulator_mocks/main.cpp] [Date: 2026-06-09]
// Description: Main entry point for the desktop emulator, handling raw terminal IO and frames.

#include "Arduino.h"
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <iostream>
#include <thread>
#include <atomic>
#include "../src/config.h"

extern void setup();
extern void loop();

struct termios orig_termios;
std::atomic<bool> emulatorRunning(true);

void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    std::cout << "\033[?25h"; // Show terminal cursor
    std::cout << "\nEmulator closed. Terminal restored.\n";
}

void setup_terminal() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(restore_terminal);
    
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON); // Disable buffering and echo
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    std::cout << "\033[?25l"; // Hide terminal cursor
}

void input_thread() {
    char c;
    while (emulatorRunning) {
        int n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) continue;
        
        if (c == '\033') { // Escape sequence (e.g. arrows) or Esc
            char seq[2];
            int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
            fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
            
            int r1 = read(STDIN_FILENO, &seq[0], 1);
            int r2 = read(STDIN_FILENO, &seq[1], 1);
            
            fcntl(STDIN_FILENO, F_SETFL, flags); // Restore blocking
            
            if (r1 > 0 && r2 > 0 && seq[0] == '[') {
                char arrowKey = 0;
                if (seq[1] == 'A') arrowKey = KEY_UP;
                else if (seq[1] == 'B') arrowKey = KEY_DOWN;
                else if (seq[1] == 'C') arrowKey = KEY_RIGHT;
                else if (seq[1] == 'D') arrowKey = KEY_LEFT;
                
                if (arrowKey) {
                    setEmulatorKey(arrowKey, true);
                    std::this_thread::sleep_for(std::chrono::milliseconds(80));
                    setEmulatorKey(arrowKey, false);
                }
            } else {
                setEmulatorKey(KEY_ESC, true);
                std::this_thread::sleep_for(std::chrono::milliseconds(80));
                setEmulatorKey(KEY_ESC, false);
            }
        } else if (c == '\n' || c == '\r') {
            setEmulatorKey(KEY_ENTER, true);
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            setEmulatorKey(KEY_ENTER, false);
        } else if (c == 127 || c == '\b') {
            setEmulatorKey(KEY_BKSP, true);
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            setEmulatorKey(KEY_BKSP, false);
        } else if (c == '\t') {
            setEmulatorKey(KEY_TAB, true);
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            setEmulatorKey(KEY_TAB, false);
        } else if (c == 's') { // 's' taps Shift
            setEmulatorKey(KEY_SHIFT, true);
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            setEmulatorKey(KEY_SHIFT, false);
        } else if (c == 'S') { // 'S' toggles Shift (useful for selection mode)
            bool cur = emulatorKeys[KEY_SHIFT];
            setEmulatorKey(KEY_SHIFT, !cur);
        } else if (c == 'a') { // 'a' taps Alt
            setEmulatorKey(KEY_ALT, true);
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            setEmulatorKey(KEY_ALT, false);
        } else if (c == 'A') { // 'A' toggles Alt
            bool cur = emulatorKeys[KEY_ALT];
            setEmulatorKey(KEY_ALT, !cur);
        } else if (c >= '0' && c <= '9') {
            setEmulatorKey(c, true);
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            setEmulatorKey(c, false);
        } else if (c == 'q' || c == 'Q') {
            emulatorRunning = false;
        }
    }
}

int main() {
    // Make sure stdout is clean
    std::cout << "\033[2J\033[H"; // Clear screen
    std::cout << "Starting ESP32 Handheld Desktop Emulator...\n";
    std::cout << "Controls:\n";
    std::cout << "  - Arrow keys: Up/Down/Left/Right\n";
    std::cout << "  - Enter: Select / Confirm\n";
    std::cout << "  - Backspace (or Del): Delete / Backspace\n";
    std::cout << "  - Escape: Go Back / Exit App\n";
    std::cout << "  - Tab: Tab key\n";
    std::cout << "  - 0-9: Multi-tap input\n";
    std::cout << "  - 's': Tap Shift | 'S': Toggle Shift hold\n";
    std::cout << "  - 'a': Tap Alt   | 'A': Toggle Alt hold\n";
    std::cout << "  - 'q' / 'Q': Quit emulator\n\n";
    std::cout << "Loading firmware..." << std::endl;

    // Create virtual SD directory if it doesn't exist
    mkdir("./emulator_sd", 0777);
    mkdir("./emulator_sd/apps", 0777);

    // Copy live Lua scripts and settings if they exist in standard project directories
    // to populate the virtual SD card immediately.
    system("cp -rf data/* ./emulator_sd/ 2>/dev/null");

    setup_terminal();
    std::thread keyThread(input_thread);

    setup();

    // Run loop continuously with a 1ms sleep to keep CPU usage low
    // while ensuring pollMatrix is called constantly.
    while (emulatorRunning) {
        loop();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    keyThread.join();
    return 0;
}
