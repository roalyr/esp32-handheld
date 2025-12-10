// [Revision: v1.0] [Path: src/app_interface.h] [Date: 2025-12-10]
// Description: Abstract base class defining the standard interface for all applications.

#ifndef APP_INTERFACE_H
#define APP_INTERFACE_H

#include "hal.h"

class App {
  public:
    virtual ~App() {}

    // Called when the app becomes active
    virtual void start() = 0;

    // Called when the app is being switched away from
    virtual void stop() = 0;

    // Main logic loop (physics, timers, etc.)
    virtual void update() = 0;

    // Drawing logic
    virtual void render() = 0;

    // Input handling (only called on just-pressed events)
    virtual void handleInput(char key) = 0;
};

#endif