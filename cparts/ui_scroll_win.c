#include "defs.h"
#include "ui.h"
#include "ui_win_impl.h"

static ATOM g_scrollWndCls = 0;

static void ui_sv_send_event(UI_ScrollView* w)
{
	LONG ox, oy;

	// automatic centering is an app policy.
	/*
	GetClientRect(w->handle, &rect);
	width = rect.right; height = rect.bottom;

	if (width > w->extent.x) ox = -(width - (LONG)w->extent.x) / 2;
	else ox = GetScrollPos(w->handle, SB_HORZ);

	if (height > w->extent.y) oy = -(height - (LONG)w->extent.y) / 2;
	else oy = GetScrollPos(w->handle, SB_VERT);
	*/

	// automatic content scrolling is an app policy.
	// rationale: 3D views, content with fixed overlays...
	// ScrollWindow(w->handle, ox - (LONG)w->origin.x, oy - (LONG)w->origin.y, NULL, NULL);

	ox = GetScrollPos(w->handle, SB_HORZ);
	oy = GetScrollPos(w->handle, SB_VERT);

	if (ox != (LONG)w->scroll.x || oy != (LONG)w->scroll.y) {
		w->scroll.x = ox; w->scroll.y = oy;
		{ UI_SizeEvent e = { ui_event_scroll, ox, oy };
		  w->events(w->context, (UI_Event*)&e); }
	}
}

static void ui_sv_scroll(UI_ScrollView* w, int bar, WPARAM wParam)
{
	SCROLLINFO si;
	int pos, limit;

	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
	GetScrollInfo(w->handle, bar, &si);

	pos = si.nPos;
	limit = si.nMax - si.nPage + 1;

	switch (LOWORD(wParam))
	{
	case SB_TOP: pos = 0; break;
	case SB_BOTTOM: pos = limit; break;
	case SB_LINEUP: pos -= 20; break;
	case SB_LINEDOWN: pos += 20; break;
	case SB_PAGEUP: pos -= si.nPage; break;
	case SB_PAGEDOWN: pos += si.nPage; break;
	case SB_THUMBPOSITION:
	case SB_THUMBTRACK:
		pos = HIWORD(wParam); // FIXME: won't work above 65535.
		break;
	}

	if (pos < 0) pos = 0;
	else if (pos > limit) pos = limit;

	si.fMask = SIF_POS;
	si.nPos = pos;
	SetScrollInfo(w->handle, bar, &si, TRUE);

	// detect scroll change and send event.
	ui_sv_send_event(w);
}

static void ui_sv_updateBar(UI_ScrollView* w, int bar, int pos)
{
	// it turns out that strange things happen to scroll bars unless
	// you set all three attributes at once: bars don't get re-enabled
	// sometimes and the range gets out of sync (or perhaps they just
	// didn't work properly inside LockWindowUpdate.)
	int page, size;
	RECT rect;
	SCROLLINFO si;

	// we really do need to get this once for each bar, in case
	// updating the previous bar caused it to show or hide.
	GetClientRect(w->handle, &rect);

	if (bar == SB_HORZ) {
		size = (LONG)w->extent.x - 1;
		page = rect.right;
	}
	else {
		size = (LONG)w->extent.y - 1;
		page = rect.bottom;
	}

	// normally SetScrollInfo clips nPos and nPage to [nMin,nMax] but
	// better safe than sorry with the scrolling APIs.
	if (pos < 0) pos = 0;
	if (size < 0) size = 0;

	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;
	si.nPos = pos;
	si.nMin = 0;
	si.nMax = size;
	si.nPage = page;
	SetScrollInfo(w->handle, bar, &si, TRUE);
}

LRESULT UI_ScrollViewProc(UI_WindowHook* hook, UINT message, WPARAM wParam, LPARAM lParam)
{
	UI_ScrollView* w = (UI_ScrollView*) hook;
    switch (message)
	{
	// This causes bar bounce (and clamping scroll events) when the app
	// uses the size event to change the extent and/or scroll position.
    case WM_SIZE:
		/*
        ui_sv_updateBar(w, SB_HORZ);
        ui_sv_updateBar(w, SB_VERT);
        ui_sv_reposition(w); */
        break; // send size event.

    case WM_VSCROLL:
        ui_sv_scroll(w, SB_VERT, wParam);
        return 0;

    case WM_HSCROLL:
        ui_sv_scroll(w, SB_HORZ, wParam);
        return 0;
    }
    return UI_SendEventsProc(hook, message, wParam, lParam);
}

UI_Window* ui_create_scroll_view(UI_Window* parent, UI_EventFunc events, void* context)
{
	UI_ScrollView* w = cpart_new(UI_ScrollView);
	UI_CreateParams params = { (UI_Window*)w, UI_ScrollViewProc };
	w->events = events ? events : ui_no_events; w->context = context;
	if (!g_scrollWndCls)
		g_scrollWndCls = ui_registerWindowClass("ScrollViewClass", NULL, 0, 0);
    CreateWindowEx(0, "ScrollViewClass", "",
        WS_CHILD | WS_CLIPSIBLINGS | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP | WS_VISIBLE,
        0, 0, 0, 0,
        parent->handle, 0, g_hInstance, &params);
	return (UI_Window*)w;
}

void ui_scroll_view_set_extent(UI_Window* win, iPair extent)
{
	UI_ScrollView* w = (UI_ScrollView*)win; // TODO FIXME no type check.

	// we only really need to update position and range, but strange
	// things happen unless we update everything at the same time.
	w->extent = extent;
	ui_sv_updateBar(w, SB_HORZ, w->scroll.x);
	ui_sv_updateBar(w, SB_VERT, w->scroll.y);

	// detect scroll change and send event; scroll is caused when
	// the new extent is smaller and clamps the scroll position.
	ui_sv_send_event(w);
}

void ui_scroll_view_set_pos(UI_Window* win, iPair pos)
{
	UI_ScrollView* w = (UI_ScrollView*)win; // TODO FIXME no type check.

	// we only really need to update positions, but strange
	// things happen unless we update everything at the same time.
	ui_sv_updateBar(w, SB_HORZ, pos.x);
	ui_sv_updateBar(w, SB_VERT, pos.y);

	// detect scroll change and send event.
	ui_sv_send_event(w);
}

void ui_scroll_view_set_extent_pos(UI_Window* win, iPair extent, iPair pos)
{
	UI_ScrollView* w = (UI_ScrollView*)win; // TODO FIXME no type check.

	// change extent and scroll position at the same time to avoid
	// clamping of the old scroll position, which would generate a
	// spurious scroll event.
	w->extent = extent;
	ui_sv_updateBar(w, SB_HORZ, pos.x);
	ui_sv_updateBar(w, SB_VERT, pos.y);

	// detect scroll change and send event; scroll is caused either
	// by the new scroll position or by clamping to the extent.
	ui_sv_send_event(w);
}
