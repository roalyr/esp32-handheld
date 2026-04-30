// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/app_control.h
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TACTICAL_TODO TASK_1
// LOG_REF: 2026-04-28

#ifndef APP_CONTROL_H
#define APP_CONTROL_H

#include "app_interface.h"

// Implemented in main.cpp
void switchApp(App* newApp);
void launchLuaOwnedApp(App* newApp);

#endif
