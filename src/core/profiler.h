#pragma once
#include "defines.h"

#ifndef PROFILING

void InitializeProfiler();
void ShutdownProfiler();

#define INITIALIZE_PROFILER() InitializeProfiler()
#define SHUTDOWN_PROFILER() ShutdownProfiler()

void StartScope(const char* name);
void EndScope();

#define START_SCOPE(name)
#define END_SCOPE()

#else

#define INITIALIZE_PROFILER()
#define SHUTDOWN_PROFILER()

#define START_SCOPE(name)
#define END_SCOPE()

#endif

