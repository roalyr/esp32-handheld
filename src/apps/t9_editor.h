// [Revision: v1.0] [Path: src/apps/t9_editor.h] [Date: 2025-12-09]
// Description: Header for the T9 Editor Application.

#ifndef APP_T9_EDITOR_H
#define APP_T9_EDITOR_H

#include "../hal.h"
#include "../t9_engine.h"

// Renders the T9 text editor interface, including the text buffer,
// pending character candidate, timeout bar, and cursor.
void renderT9Editor();

#endif