#include "profiler.h"

#include "../containers/darray.h"
#include "meminc.h"
#include "engine.h"
#include "timer.h"
#include "asserts.h"
#include <string.h>

#define MAX_SCOPE_DEPTH 16


typedef struct Scope
{
	const char* name;
	f64 startTime;
	//u32 depth; to add in the future if desired
} Scope;

DEFINE_DARRAY_TYPE(Scope);

typedef struct ProfilerState
{
	Timer perfTimer;
	ScopeDarray* scopesDarray;
	u32 scopeDepth;
} ProfilerState;

static ProfilerState state;


void _InitializeProfiler()
{
	StartOrResetTimer(&state.perfTimer);
	state.scopesDarray = ScopeDarrayCreate(MAX_SCOPE_DEPTH, GetGlobalAllocator());
	state.scopeDepth = 0;
}

void _ShutdownProfiler()
{
	DarrayDestroy(state.scopesDarray);
}

void _StartScope(const char* name)
{
	GRASSERT(state.scopeDepth < MAX_SCOPE_DEPTH)

	Scope scope = {};
	scope.name = name;
	scope.startTime = TimerSecondsSinceStart(state.perfTimer);

	ScopeDarrayPushback(state.scopesDarray, &scope);

	state.scopeDepth++;
}

void _EndScope()
{
	state.scopeDepth--;
	GRASSERT(state.scopeDepth >= 0 && state.scopeDepth != UINT32_MAX);

	Scope scope = state.scopesDarray->data[state.scopeDepth];
	_DEBUG("Profiler: Scope \"%s\", took %f seconds.", scope.name, TimerSecondsSinceStart(state.perfTimer) - scope.startTime);

	DarrayPop(state.scopesDarray);
}


