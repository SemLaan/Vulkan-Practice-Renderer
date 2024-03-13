#pragma once
#include "defines.h"



typedef struct Timer
{
	f64 startTime;
} Timer;

extern Timer g_timer;
extern f64 g_deltaTime;

void StartOrResetTimer(Timer* timer);

f64 TimerSecondsSinceStart(Timer timer);
f64 TimerMilisecondsSinceStart(Timer timer);
