// [Revision: v1.0] [Path: src/apps/yes_no_prompt.h] [Date: 2025-12-10]
// Description: Yes/No prompt App used by other apps for confirmations.

#ifndef APP_YES_NO_PROMPT_H
#define APP_YES_NO_PROMPT_H

#include "../app_interface.h"
#include <Arduino.h>

class YesNoPromptApp : public App {
  private:
    App* caller;        // App to return to after prompt
    String message;     // Message to display
    bool selection;     // true = YES, false = NO
    bool active;        // Is prompt active

  public:
    YesNoPromptApp();
    void startPrompt(App* callerApp, const String& msg);

    // App interface
    void start() override;
    void stop() override;
    void update() override;
    void render() override;
    void handleInput(char key) override;
};

extern YesNoPromptApp yesNoPrompt;

#endif
