#pragma once
#include "defines.h"



typedef struct Timer
{
	f64 startTime;
} Timer;


void StartOrResetTimer(Timer* timer);

f64 TimerSecondsSinceStart(Timer timer);
f64 TimerMilisecondsSinceStart(Timer timer);
