#include "timer.h"

#include "core/platform.h"


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
