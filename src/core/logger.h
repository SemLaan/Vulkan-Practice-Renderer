#pragma once
#include "defines.h"

typedef enum log_level
{
	LOG_LEVEL_FATAL = 0,
	LOG_LEVEL_ERROR = 1,
	LOG_LEVEL_WARN = 2,
	LOG_LEVEL_INFO = 3,
	LOG_LEVEL_DEBUG = 4,
	LOG_LEVEL_TRACE = 5,
	MAX_LOG_LEVELS
} log_level;

// Should get called when the program closes
// Closes the logs file
void WriteLogsToFile();

// Logs a message
void Log(log_level level, const char* message, ...);


// Always define fatal and error
#define _FATAL(message, ...)	Log(LOG_LEVEL_FATAL, message, ##__VA_ARGS__)
#define _ERROR(message, ...)	Log(LOG_LEVEL_ERROR, message, ##__VA_ARGS__)

// Only in non dist builds
#ifndef DIST
#define _WARN(message, ...)	Log(LOG_LEVEL_WARN, message, ##__VA_ARGS__)
#define _INFO(message, ...)	Log(LOG_LEVEL_INFO, message, ##__VA_ARGS__)
#else
#define _WARN(message, ...)
#define _INFO(message, ...)
#endif

// Only in debug builds
#ifdef DEBUG
#define _DEBUG(message, ...)	Log(LOG_LEVEL_DEBUG, message, ##__VA_ARGS__)
#define _TRACE(message, ...)	Log(LOG_LEVEL_TRACE, message, ##__VA_ARGS__)
#else
#define _DEBUG(message, ...)
#define _TRACE(message, ...)
#endif