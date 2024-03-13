#include "timer.h"

#include "core/platform.h"


Timer g_timer = {};
f64 g_previousFrameTime = 0;
f64 g_deltaTime = 0;


void StartOrResetTimer(Timer* timer)
{
	timer->startTime = PlatformGetTime();
}

f64 TimerSecondsSinceStart(Timer timer)
{
	return PlatformGetTime() - timer.startTime;
}

f64 TimerMilisecondsSinceStart(Timer timer)
{
	return (PlatformGetTime() - timer.startTime) * 0.001;
}
