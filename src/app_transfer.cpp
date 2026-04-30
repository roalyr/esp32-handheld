// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/app_transfer.cpp
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TACTICAL_TODO TASK_2
// LOG_REF: 2026-04-27
// Description: Global transfer state used for editor/viewer and prompt workflows.

#include "app_transfer.h"

App* appTransferCaller = nullptr;
int appTransferAction = ACTION_NONE;
bool appTransferBool = false;
bool appTransferResultReady = false;
String appTransferString = "";
String appTransferPath = "";
String appTransferLabel = "";
int appTransferEditorMode = APP_TRANSFER_EDITOR_DEFAULT;
int appTransferSourceKind = APP_TRANSFER_SOURCE_DEFAULT;
