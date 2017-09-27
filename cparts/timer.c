#include "defs.h"
#include "timer.h"
#include "alloc.h"

// Design considerations:
//
// (a) it is better to take a (function, object) pair than an
//     object with a function pointer embedded; this permits much
//     greater flexibility and re-use of both functions and objects.
//
// (b) the callbacks can manipulate timers in any way: cancel any
//     timer including the one in callback, add new timers, remove
//     all timers; these operations must remain correct.
//
// (c) expose timer ids rather than pointers, since ids force
//     validation on each use and allow the internal timer data to
//     be moved in memory.

// TODO: please note that this does not actually free timer_t blocks
// and should do so (a) in run() when a timer has been removed, and
// (b) how do we clean up one-shot timers? *sigh*

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // mmsystem
#include <mmsystem.h> // timeGetTime
#pragma comment(lib,"winmm.lib")
typedef DWORD time_t;
#else
#include <sys/time.h> // gettimeofday, struct timeval
typedef unsigned long time_t;
#endif


enum timer_flags_e
{
	timerf_once			= 0x4000, // timer cancels itself after one tick
	timerf_cancelled	= 0x2000, // cancelled, will be removed asap
	timerf_removed		= 0x1000, // removed from the schedule
};

struct timer_t
{
	int flags;
	int index;
	unsigned long interval;
	timer_func func;
	void* data;
};


static int g_num_timers = 0;
static int g_alloc_timers = 0;
static time_t* g_timer_time = 0;
static timer_t** g_timer_ptr = 0;
static allocp g_timer_alloc = 0;


timer_t* timer_add(unsigned long delay, unsigned long interval, timer_func func, void* data)
{
	timer_t* timer;

	if (!g_timer_alloc)
		g_timer_alloc = mem_init(sizeof(timer_t));

	// Expand the timers table if there isn't enough room.
	if (g_num_timers == g_alloc_timers) {
		g_alloc_timers += 64;
		g_timer_time = (time_t*)realloc(g_timer_time, g_alloc_timers * sizeof(time_t));
		g_timer_ptr = (timer_t**)realloc(g_timer_ptr, g_alloc_timers * sizeof(timer_t*));
	}

	// Allocate a new timer
	timer = (timer_t*)mem_alloc(g_timer_alloc);

	timer->flags = 0;
	timer->func = func;
	timer->data = data;

	if (interval == 0)
	{
		// Schedule a one-time call
		timer->interval = 0;
		timer->flags |= timerf_once;
	}
	else
	{
		// Schedule an interval timer
		timer->interval = interval;
	}

	timer->index = g_num_timers;

	g_timer_time[g_num_timers] = timeGetTime() + delay;
	g_timer_ptr[g_num_timers] = timer;
	g_num_timers += 1;

	return timer;
}

void timer_remove(timer_t* timer)
{
	// Check if the timer has already been cancelled or removed
	if (!(timer->flags & (timerf_cancelled|timerf_removed)))
	{
		assert((timer->flags & timerf_removed) == 0); // double-remove
		assert(timer->index >= 0 && timer->index < g_num_timers); // bad index

		// Cancel the timer
		timer->flags |= timerf_cancelled;

		// Make the timer due now so it will be removed
		g_timer_time[timer->index] = timeGetTime();
	}
}

unsigned long timer_run(void* unused)
{
	long nextTimer = 10000;
	time_t startTime, endTime;
	int idx;

	// Mark the beginning of this cycle
	startTime = timeGetTime();

	// Check and execute all timers
	for (idx = 0; idx < g_num_timers; ++idx)
	{
		long timeDiff = (long)(startTime - g_timer_time[idx]);

		if (timeDiff >= 0)
		{
			// Timer is due to fire
			timer_t* timer = g_timer_ptr[idx];

			// Remove the timer if it has expired
			if (timer->flags & (timerf_once|timerf_cancelled))
			{
				// Remove the timer from the list
				g_num_timers -= 1;
				if (g_num_timers > idx)
				{
					// Move the timer at the end of the list down
					g_timer_time[idx] = g_timer_time[g_num_timers];
					g_timer_ptr[idx] = g_timer_ptr[g_num_timers];

					// Update the moved timer's index
					g_timer_ptr[idx]->index = idx;

					--idx; // process this index again
				}
				timer->flags |= timerf_removed;
			}
			else
			{
				// Schedule the next tick
				g_timer_time[idx] += timer->interval;

				// Timer will fire again in interval milliseconds or less.
				timeDiff -= (long)timer->interval;
				if (timeDiff >= 0) nextTimer = 0; // more ticks are overdue.
				else if( -timeDiff < nextTimer ) nextTimer = -timeDiff; // next tick in -timeDiff milliseconds.
			}

			// Execute the timer callable unless it was cancelled
			if (!(timer->flags & timerf_cancelled))
			{
				// Note: this can add new timers, but cannot remove timers!
				timer->func(timer->data, timer);
			}
		}
		else
		{
			// Timer will fire in -timeDiff milliseconds.
			if( -timeDiff < nextTimer ) nextTimer = -timeDiff;
		}
	}

	// Mark the end of this cycle
	endTime = timeGetTime();
	nextTimer -= (long)(endTime - startTime);
	if (nextTimer < 0) nextTimer = 0;

	return nextTimer;
}
