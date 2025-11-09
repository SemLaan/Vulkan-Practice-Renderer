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
	f64 time;
	u32 count;
	u32 depth;
} Scope;

DEFINE_DARRAY_TYPE(Scope);

typedef struct ProfilerState
{
	Timer perfTimer;
	ScopeDarray* frameScopes;
	const char* frameName;
	u32 scopeDepth;
	u32 currentScopeStartTimes[MAX_SCOPE_DEPTH];
	bool recordingFrame;
} ProfilerState;

static ProfilerState state;


void InitializeProfiler()
{
	state.recordingFrame = false;
	StartOrResetTimer(&state.perfTimer);
	state.frameScopes = ScopeDarrayCreate(100, GetGlobalAllocator());
}

void ShutdownProfiler()
{
	state.recordingFrame = false;
	DarrayDestroy(state.frameScopes);
}

void RecordFrame(const char* frameScopeName)
{
	GRASSERT_MSG((state.recordingFrame || state.frameName), "There is a frame already being recorded");
	state.recordingFrame = false;
	state.frameName = frameScopeName;
	state.scopeDepth = 0;
	state.frameScopes->size = 0; // Resetting the frame scopes darray
}

void StartScope(const char* name)
{
	if (!state.recordingFrame && state.frameName == nullptr)
		return;

	if (!state.recordingFrame && 0 == strncmp(state.frameName, name, strlen(state.frameName)))
	{
		state.recordingFrame = true;
	}

	Scope scope = {};
	scope.name = name;
	scope.count = 1;
	scope.depth = state.scopeDepth;

	ScopeDarrayPushback(state.frameScopes, &scope);

	state.scopeDepth++;
}

void EndScope()
{
	
}


