#include "event.h"
#include "containers/darray.h"
#include "core/memory/memory_subsys.h"
#include "core/asserts.h"


typedef struct EventState
{
	PFN_OnEvent* eventCallbacksDarrays[MAX_EVENTS];	// Array of Darrays for event listener functions
} EventState;

static EventState* state = nullptr;

bool InitializeEvent()
{
	GRASSERT_DEBUG(state == nullptr); // If this triggers init got called twice
	_INFO("Initializing event subsystem...");
	state = Alloc(GetGlobalAllocator(), sizeof(EventState), MEM_TAG_EVENT_SUBSYS);
	MemoryZero(state, sizeof(EventState));

	return true;
}

void ShutdownEvent()
{
	if (state == nullptr)
	{
		_INFO("Events startup failed, skipping shutdown");
		return;
	}
	else
	{
		_INFO("Shutting down events subsystem...");
	}

	for (u32 i = 0; i < MAX_EVENTS; ++i)
	{
		if (state->eventCallbacksDarrays[i])
		{
			DarrayDestroy(state->eventCallbacksDarrays[i]);
		}
	}
	Free(GetGlobalAllocator(), state);
}

void RegisterEventListener(EventCode type, PFN_OnEvent listener)
{
	if (!state->eventCallbacksDarrays[type])
	{
		state->eventCallbacksDarrays[type] = (PFN_OnEvent*)DarrayCreate(sizeof(PFN_OnEvent), 5, GetGlobalAllocator(), MEM_TAG_EVENT_SUBSYS);
	}

#ifndef DIST
	for (u32 i = 0; i < DarrayGetSize(state->eventCallbacksDarrays[type]); ++i)
	{
		GRASSERT_MSG(state->eventCallbacksDarrays[type][i] != listener, "Tried to insert duplicate listener");
	}
#endif // !DIST

	state->eventCallbacksDarrays[type] = (PFN_OnEvent*)DarrayPushback(state->eventCallbacksDarrays[type], &listener);
}

void UnregisterEventListener(EventCode type, PFN_OnEvent listener)
{
	GRASSERT_DEBUG(state->eventCallbacksDarrays[type]);

	for (u32 i = 0; i < DarrayGetSize(state->eventCallbacksDarrays[type]); ++i)
	{
		if (state->eventCallbacksDarrays[type][i] == listener)
		{
			DarrayPopAt(state->eventCallbacksDarrays[type], i);
			return;
		}
	}

	GRASSERT_MSG(false, "Tried to remove listener that isn't listening");
}

void InvokeEvent(EventCode type, EventData data)
{
	if (state->eventCallbacksDarrays[type])
	{
		for (u32 i = 0; i < DarrayGetSize(state->eventCallbacksDarrays[type]); ++i)
		{
			// PFN_OnEvent callbacks return true if the event is handled so then we don't need to call anything else
			if (state->eventCallbacksDarrays[type][i](type, data))
				return;
		}
	}
}
