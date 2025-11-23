#pragma once
#include "defines.h"

#ifndef PROFILING

void _InitializeProfiler();
void _ShutdownProfiler();

#define INITIALIZE_PROFILER() _InitializeProfiler()
#define SHUTDOWN_PROFILER() _ShutdownProfiler()

void _StartScope(const char* name);
void _EndScope();

#define START_SCOPE(name) _StartScope(name)
#define END_SCOPE() _EndScope()

#else

#define INITIALIZE_PROFILER()
#define SHUTDOWN_PROFILER()

#define START_SCOPE(name)
#define END_SCOPE()

#endif

