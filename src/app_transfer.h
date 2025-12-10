// [Revision: v1.0] [Path: src/app_transfer.h] [Date: 2025-12-10]
// Description: Simple cross-app transfer area for prompt/editor workflows.

#ifndef APP_TRANSFER_H
#define APP_TRANSFER_H

#include <Arduino.h>
#include "app_interface.h"

// Transfer action codes
enum AppTransferAction {
    ACTION_NONE = 0,
    ACTION_DELETE_FILE = 1,
    ACTION_DELETE_FOLDER = 2,
    ACTION_REQUEST_FILENAME = 3,
    ACTION_CREATE_FILE = 4,
    ACTION_CREATE_FOLDER = 5,
    ACTION_RENAME = 6,
    ACTION_EDIT_FILE = 7
    , ACTION_VIEW_FILE = 8
};

extern App* appTransferCaller;   // App to return to when operation completes
extern int appTransferAction;    // One of AppTransferAction
extern bool appTransferBool;     // Yes/No result storage
extern String appTransferString; // Generic string payload (filename or file content)
extern String appTransferPath;   // Path (used for file paths)

#endif
