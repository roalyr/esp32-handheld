// [Revision: v1.0] [Path: src/apps/yes_no_prompt.cpp] [Date: 2025-12-10]
// Description: Yes/No prompt implementation. Uses `app_transfer` to store result.

#include "yes_no_prompt.h"
#include "../app_transfer.h"
#include "../app_control.h"
#include "../hal.h"

YesNoPromptApp yesNoPrompt;

YesNoPromptApp::YesNoPromptApp() {
    caller = nullptr;
    message = "";
    selection = false;
    active = false;
}

void YesNoPromptApp::startPrompt(App* callerApp, const String& msg) {
    caller = callerApp;
    message = msg;
    selection = false;
    active = true;
    // Ensure app_transfer caller is set by caller prior to switching
}

void YesNoPromptApp::start() {
    u8g2.setContrast(systemContrast);
}

void YesNoPromptApp::stop() {}

void YesNoPromptApp::update() {}

void YesNoPromptApp::handleInput(char key) {
    // Use 4/6 for left/right selection, 5 to confirm, D to cancel
    if (key == '4') selection = true;
    if (key == '6') selection = false;
    if (key == 'D') {
        // Cancel and return false
        appTransferBool = false;
        active = false;
        App* ret = caller;
        caller = nullptr;
        appTransferCaller = nullptr;
        appTransferAction = ACTION_NONE;
        switchApp(ret);
        return;
    }

    if (key == '5') {
        appTransferBool = selection;
        active = false;
        App* ret = caller;
        caller = nullptr;
        // Do not clear action/path — caller will process
        appTransferCaller = nullptr; // caller will be switched back
        switchApp(ret);
        return;
    }
}

void YesNoPromptApp::render() {
    u8g2.setFont(FONT_SMALL);
    // Dim background
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 64);
    u8g2.setDrawColor(1);

    // Message box
    u8g2.drawFrame(10, 12, 108, 40);
    u8g2.drawStr(14, 24, message.c_str());

    // Options
    if (selection) {
        // YES selected: draw left box and render YES in black on white, NO in white on black
        u8g2.drawBox(22, 38, 36, 12);
        u8g2.setDrawColor(0);
        u8g2.drawStr(28, 48, "YES");
        u8g2.setDrawColor(1);
        u8g2.drawStr(78, 48, "NO");
    } else {
        // NO selected: draw right box and render YES in white on black, NO in black on white
        u8g2.drawBox(72, 38, 36, 12);
        u8g2.setDrawColor(1);
        u8g2.drawStr(28, 48, "YES");
        u8g2.setDrawColor(0);
        u8g2.drawStr(78, 48, "NO");
        u8g2.setDrawColor(1);
    }
}
