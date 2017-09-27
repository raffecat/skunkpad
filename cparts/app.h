/* Copyright (c) 2010, Andrew Towers <atowers@gmail.com>
** All rights reserved.
*/

#ifndef CPART_APP
#define CPART_APP

typedef unsigned long (*app_run_func)(void* data);

void init(void);
void final(void);

// cleanly exit the app - begin shutdown.
void app_exit(void);

// register a scheduler callback.
void app_set_scheduler(app_run_func func, void* data);

// enable exhaustive heap checking in debug builds.
void app_heap_check();

#endif
