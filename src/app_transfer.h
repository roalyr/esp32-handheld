// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/app_transfer.h
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TACTICAL_TODO TASK_2
// LOG_REF: 2026-04-27
// Description: Cross-app transfer area for editor/viewer launch state and prompt workflows.

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

enum AppTransferEditorOpenMode {
    APP_TRANSFER_EDITOR_DEFAULT = 0,
    APP_TRANSFER_EDITOR_READ_WRITE = 1,
    APP_TRANSFER_EDITOR_READ_ONLY = 2
};

enum AppTransferDocumentSourceKind {
    APP_TRANSFER_SOURCE_DEFAULT = 0,
    APP_TRANSFER_SOURCE_BUFFER = 1,
    APP_TRANSFER_SOURCE_PAGED_PLACEHOLDER = 2
};

extern App* appTransferCaller;   // App to return to when operation completes
extern int appTransferAction;    // One of AppTransferAction
extern bool appTransferBool;     // Yes/No result storage
extern String appTransferString; // Generic string payload (filename or file content)
extern String appTransferPath;   // Path (used for file paths)
extern String appTransferLabel;  // Display label/title for editor or prompt flows
extern int appTransferEditorMode;
extern int appTransferSourceKind;

#endif
