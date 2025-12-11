// [Revision: v1.1] [Path: src/lua_vm.h] [Date: 2025-12-11]
// Description: Lua VM wrapper for embedded scripting on ESP32 handheld.

#ifndef LUA_VM_H
#define LUA_VM_H

#include <Arduino.h>

// Forward declaration to avoid including lua headers everywhere
struct lua_State;

namespace LuaVM {

// --------------------------------------------------------------------------
// INITIALIZATION
// --------------------------------------------------------------------------

/**
 * Initialize the Lua VM with all registered bindings.
 * Must be called before any other Lua functions.
 * @return true if successful, false if memory allocation failed
 */
bool init();

/**
 * Shut down the Lua VM and free all resources.
 */
void shutdown();

/**
 * Check if the VM is initialized and ready.
 */
bool isReady();

// --------------------------------------------------------------------------
// SCRIPT EXECUTION
// --------------------------------------------------------------------------

/**
 * Execute a Lua script from a string.
 * @param script The Lua source code to execute
 * @param name Optional name for error messages (default: "chunk")
 * @return true if execution was successful
 */
bool executeString(const char* script, const char* name = "chunk");

/**
 * Execute a Lua script from a file on SPIFFS.
 * @param path Path to the .lua file (e.g., "/scripts/test.lua")
 * @return true if execution was successful
 */
bool executeFile(const char* path);

// --------------------------------------------------------------------------
// ERROR HANDLING
// --------------------------------------------------------------------------

/**
 * Get the last error message from Lua execution.
 * @return Error message string, or empty string if no error
 */
const char* getLastError();

/**
 * Clear the last error message.
 */
void clearError();

// --------------------------------------------------------------------------
// MEMORY INFO
// --------------------------------------------------------------------------

/**
 * Get current Lua memory usage in bytes.
 */
size_t getMemoryUsage();

/**
 * Run Lua garbage collector.
 */
void collectGarbage();

} // namespace LuaVM

#endif // LUA_VM_H
