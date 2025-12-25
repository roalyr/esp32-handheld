// [Revision: v2.0] [Path: src/apps/yes_no_prompt.cpp] [Date: 2025-12-11]
// Description: Yes/No prompt - refactored to use unified GUI module.

#include "yes_no_prompt.h"
#include "../gui.h"
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
}

void YesNoPromptApp::start() {
    u8g2.setContrast(systemContrast);
}

void YesNoPromptApp::stop() {}

void YesNoPromptApp::update() {}

void YesNoPromptApp::handleInput(char key) {
    // Use LEFT/RIGHT for selection, ENTER to confirm, ESC to cancel
    if (key == KEY_LEFT) selection = true;
    if (key == KEY_RIGHT) selection = false;
    
    if (key == KEY_ESC) {
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

    if (key == KEY_ENTER) {
        appTransferBool = selection;
        active = false;
        App* ret = caller;
        caller = nullptr;
        appTransferCaller = nullptr;
        switchApp(ret);
        return;
    }
}

void YesNoPromptApp::render() {
    // Dim background
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, GUI::SCREEN_WIDTH, GUI::SCREEN_HEIGHT);
    u8g2.setDrawColor(1);

    // Use unified Yes/No dialog
    GUI::drawYesNoDialog(message.c_str(), selection);
}