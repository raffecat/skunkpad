/* Copyright (c) 2010, Andrew Towers <atowers@gmail.com>
** All rights reserved.
*/

#ifndef CPART_TIMER
#define CPART_TIMER

typedef struct timer_t timer_t;
typedef void (*timer_func)(void* data, timer_t* timer);

/** Schedule a timer for delayed and/or interval execution.
 *  Delay and interval are given in milliseconds.
 *  The timer will be one-shot if interval is zero.
 */
timer_t* timer_add(unsigned long delay, unsigned long interval, timer_func func, void* data);

/** Cancel a timer.
 */
void timer_remove(timer_t* timer);

/** Run callbacks for all timers that are due.
 *  Returns the number of milliseconds until the next timer is due.
 */
unsigned long timer_run(void* unused);


#endif
