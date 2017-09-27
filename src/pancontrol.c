/* Copyright (c) 2011, Andrew Towers <atowers@gmail.com>
** All rights reserved.
*/

#ifndef CPART_PAN_CONTROL
#define CPART_PAN_CONTROL

extern bool panMode; // read only.

struct UI_KeyEvent;
bool panKeyInput(struct UI_KeyEvent* e);

struct Tablet_InputEvent;
void panPenInput(struct Tablet_InputEvent* e);

// inputs:

struct UI_Window;
extern struct UI_Window* scrollView;

struct iPair;
extern struct iPair scrollPos;

#endif

#include "defs.h"
#include "timer.h"
#include "ui.h"
#include "tablet_input.h"
#include "pancontrol.h"

bool panMode = false;

static bool panning = false;
static fPair lastPan = {0,0};
static fPair panVel = {0,0};
static fPair kickVel = {0,0}; // arrow keys.
static timer_t* g_panTimer = 0;

// TODO: write an animation scheduler.
static const long c_animInterval = 1000 / 30;
static const float c_kickSpeed = 26;

static void scrollCanvas(int dx, int dy)
{
    iPair pos = scrollPos;
    pos.x += dx; pos.y += dy;
    ui_scroll_view_set_pos(scrollView, pos);
}

static void cb_pan(void* data, timer_t* timer)
{
    int dx, dy;
    if (kickVel.x || kickVel.y) {
        panVel = kickVel;
    }
    dx = (int)panVel.x;
    dy = (int)panVel.y;
    if (dx || dy) {
        panVel.x *= 0.8f; panVel.y *= 0.8f;
        if (!panning)
            scrollCanvas(dx, dy);
    } else if (!panMode) {
        // this is for kick panning with arrow keys, since normally
        // the timer is stopped when we exit pan mode.
        panVel.x = 0; panVel.y = 0;
        timer_remove(g_panTimer);
        g_panTimer = 0;
    }
}

static void kickPan()
{
    // start the pan timer if not already running (e.g. outside pan mode)
    if (!g_panTimer)
        g_panTimer = timer_add(c_animInterval, c_animInterval, cb_pan, 0);
}

static void togglePanning(bool pan)
{
    if (panMode != pan) { // ignore auto-repeat.
        panMode = pan;
        if (pan) {
            // run animation timer the whole time in pan mode, so we can
            // decay the pen velocity while the pen is stationary.
            if (!g_panTimer)
                g_panTimer = timer_add(c_animInterval, c_animInterval, cb_pan, 0);
        } else {
            // stop panning immediately.
            panning = false;
            // clear any left-over panning velocity.
            panVel.x = 0; panVel.y = 0;
            // stop the panning animation timer.
            if (g_panTimer) timer_remove(g_panTimer);
            g_panTimer = 0;
        }
    }
}

bool panKeyInput(UI_KeyEvent* e) {
    bool down = e->is_down;
    switch (e->key) {
    case 32:
        togglePanning(down);
        return true;
    case 37:
        if (down) { kickVel.x = -c_kickSpeed; kickPan(); }
        else if (kickVel.x < 0) kickVel.x = 0;
        return true;
    case 39:
        if (down) { kickVel.x = c_kickSpeed; kickPan(); }
        else if (kickVel.x > 0) kickVel.x = 0;
        return true;
    case 38:
        if (down) { kickVel.y = -c_kickSpeed; kickPan(); }
        else if (kickVel.y < 0) kickVel.y = 0;
        return true;
    case 40:
        if (down) { kickVel.y = c_kickSpeed; kickPan(); }
        else if (kickVel.y > 0) kickVel.y = 0;
        return true;
    default:
        return false;
    }
}

void panPenInput(Tablet_InputEvent* e) {
    if (e->buttons) {
        if (!panning) {
            // pen went down - start panning.
            panning = true;
            panVel.x = 0; panVel.y = 0;
        }
        else {
            // pen still down - apply pan delta.
            float dx = lastPan.x - (float)e->x;
            float dy = lastPan.y - (float)e->y;
            scrollCanvas((int)dx, (int)dy);
            // update pan velocity for pen-up.
            panVel.x = 0.5f * (dx + panVel.x);
            panVel.y = 0.5f * (dy + panVel.y);
        }
        lastPan.x = (float)e->x;
        lastPan.y = (float)e->y;
    }
    else {
        // pen went up - apply remaining velocity.
        panning = false;
    }
}
