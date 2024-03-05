#include <stdio.h>
#include "defines.h"
#include "core/meminc.h"
#include "core/logger.h"
#include "core/asserts.h"

int main()
{
    // ============================================ Startup ============================================
	START_MEMORY_DEBUG_SUBSYS();
    InitializeMemory(4 * MiB);

    // ============================================ Run ============================================
    _ERROR("Testing beef: %i", 34);
    

    // ============================================ Shutdown ============================================
    ShutdownMemory();
	SHUTDOWN_MEMORY_DEBUG_SUBSYS();

    return 0;
}