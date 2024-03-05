#include "logger.h"

//#include "platform/platform.h"
#include "asserts.h"
#include "meminc.h"
// needed for string formatting and variadic args
#include <stdio.h>
#include <stdarg.h>


// If the log string exceeds this size it will log a warning and the full message will not get logged
#define MAX_LOG_CHARS 4000

// NOTE: this is without the null terminator on purpose
#define LOG_LEVEL_STRING_SIZE 10

#define MAX_USER_LOG_CHARS (MAX_LOG_CHARS - LOG_LEVEL_STRING_SIZE)

// Log level prefix strings
static const char* logLevels[MAX_LOG_LEVELS] =
{
	"\n[FATAL]: ",
	"\n[ERROR]: ",
	"\n[WARN]:  ",
	"\n[INFO]:  ",
	"\n[DEBUG]: ",
	"\n[TRACE]: ",
};

// Log file object
static FILE* file = nullptr;

void WriteLogsToFile()
{
	_INFO("Writing logs to file...");

	if (file != nullptr)
		fclose(file);
}

void Log(log_level level, const char* message, ...)
{
	// Big stack array that will hold the formatted message
	// It is static since MAX_LOG_CHARS is a define constant
	char final_message[MAX_LOG_CHARS];

	// Puts the appropriate log level prefix in the beginning of the final message array
	MemoryCopy(final_message, (void*)logLevels[level], LOG_LEVEL_STRING_SIZE);

	// Getting a pointer to after the log level prefix
	char* user_message = final_message + LOG_LEVEL_STRING_SIZE;

	// Getting the variadic args
	va_list args;
	va_start(args, message);

	// Reading the variadic args into the user message (which is the part of final_message after the log level prefix)
	// result is the amount of characters written (or would have been written if it is more than MAX_USER_LOG_CHARS)
	i32 result = vsnprintf(user_message, MAX_USER_LOG_CHARS, message, args);

	va_end(args);

	// Writing the log to the log file (and creating a log file object if it doesn't exist yet)
	if (file == nullptr)
		file = fopen("console.log", "w+");
	fprintf(file, final_message);

	// Actually printing to console and platform specific console
	//PlatformLogString(level, final_message);TODO: this once platform file is here
	printf(final_message);

	//if (result >= MAX_USER_LOG_CHARS || result < 0)
		//PlatformLogString(LOG_LEVEL_FATAL, "\nLogging failed, too many characters or formatting error");
}
