#pragma once
#include "logger.h"

#ifdef __GNUC__
#define debugBreak() __builtin_trap()
#else
#define debugBreak() __debugbreak()
#endif

// Checks whether the given expression is true
#define GRASSERT(expr) {if (expr){} else { _FATAL("Assertion fail: %s, File: %s:%i", #expr, __FILE__, __LINE__); debugBreak();}}
// Checks whether the given expression is true
#define GRASSERT_MSG(expr, message) {if (expr){} else { _FATAL("Assertion fail: %s, Message: %s, File: %s:%i", #expr, message, __FILE__, __LINE__); debugBreak();}}

#ifdef DEBUG
// Checks whether the given expression is true
#define GRASSERT_DEBUG(expr) {if (expr){} else { _FATAL("Assertion fail: %s, File: %s:%i", #expr, __FILE__, __LINE__); debugBreak();}}
#else
#define GRASSERT_DEBUG(expr)
#endif