// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/lua_vm.h
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TACTICAL_TODO TASK_1/TASK_2/TASK_3
// LOG_REF: 2026-03-28

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

// --------------------------------------------------------------------------
// COOPERATIVE FRAME-LOOP API
// --------------------------------------------------------------------------

/**
 * Check if a global Lua function exists.
 */
bool hasFunction(const char* funcName);

/**
 * Call a global Lua function by name with no arguments.
 * If the function doesn't exist, silently returns true (not an error).
 * On Lua error, sets lastError and returns false.
 */
bool callGlobalFunction(const char* funcName);

/**
 * Call the global "_input" Lua function with a single key character.
 * If _input doesn't exist, silently returns true.
 * On Lua error, sets lastError and returns false.
 */
bool callInputHandler(char key);

} // namespace LuaVM

#endif // LUA_VM_H
