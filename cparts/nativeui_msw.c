#include "defs.h"
#include "typeref.h"
#include "ui_win_impl.h"
#include "str.h"


// Common window properties.

static bool ui_get_show(UI_Window* w) {
	return GetWindowLong(w->handle, GWL_STYLE) & WS_VISIBLE;
}
static bool ui_get_visible(UI_Window* w) {
	return IsWindowVisible(w->handle);
}
static iPair ui_get_position(UI_Window* w) {
	iPair pr; RECT rect;
	GetWindowRect(w->handle, &rect);
	pr.x = rect.left; pr.y = rect.top;
	return pr;
}
static iPair ui_get_size(UI_Window* w) {
	iPair pr; RECT rect;
	GetWindowRect(w->handle, &rect);
	pr.x = rect.right - rect.left; pr.y = rect.bottom - rect.top;
	return pr;
}
static iRect ui_get_rect(UI_Window* w) {
	iRect r; RECT rect;
	GetWindowRect(w->handle, &rect);
	r.left = rect.left; r.top = rect.top;
	r.right = rect.right; r.bottom = rect.bottom;
	return r;
}
static int ui_get_width(UI_Window* w) { iPair pr = ui_get_size(w); return pr.x; }
static int ui_get_height(UI_Window* w) { iPair pr = ui_get_size(w); return pr.y; }
static int ui_get_left(UI_Window* w) { iPair pr = ui_get_position(w); return pr.x; }
static int ui_get_top(UI_Window* w) { iPair pr = ui_get_position(w); return pr.y; }
static int ui_get_right(UI_Window* w) { iRect rect = ui_get_rect(w); return rect.right; }
static int ui_get_bottom(UI_Window* w) { iRect rect = ui_get_rect(w); return rect.bottom; }
static string ui_get_text(UI_Window* w) {
	int len = GetWindowTextLength(w->handle);
	if (len < 1) return str_empty();
	{stringbuf sb = stringbuf_new(len);
	 len = GetWindowText(w->handle, stringbuf_reserve(&sb, len), len);
	 stringbuf_commit(&sb, len);
	 return stringbuf_finish(&sb);
	}
}

static void ui_set_show(UI_Window* w, bool visible) {
	ShowWindow(w->handle, visible ? SW_SHOW : SW_HIDE);
}
static void ui_set_position(UI_Window* w, iPair pos) {
	RECT rect; GetWindowRect(w->handle, &rect);
	MoveWindow(w->handle, pos.x, pos.y, rect.right-rect.left, rect.bottom-rect.top, TRUE);
}
static void ui_set_size(UI_Window* w, iPair size) {
	RECT rect; GetWindowRect(w->handle, &rect);
	MoveWindow(w->handle, rect.left, rect.top, size.x, size.y, TRUE);
}
static void ui_set_rect(UI_Window* w, iRect rect) {
	MoveWindow(w->handle, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, TRUE);
}
static void ui_set_width(UI_Window* w, int val) {
	RECT rect; GetWindowRect(w->handle, &rect);
	MoveWindow(w->handle, rect.left, rect.top, val, rect.bottom-rect.top, TRUE);
}
static void ui_set_height(UI_Window* w, int val) {
	RECT rect; GetWindowRect(w->handle, &rect);
	MoveWindow(w->handle, rect.left, rect.top, rect.right-rect.left, val, TRUE);
}
static void ui_set_left(UI_Window* w, int val) {
	RECT rect; GetWindowRect(w->handle, &rect);
	MoveWindow(w->handle, val, rect.top, rect.right-rect.left, rect.bottom-rect.top, TRUE);
}
static void ui_set_top(UI_Window* w, int val) {
	RECT rect; GetWindowRect(w->handle, &rect);
	MoveWindow(w->handle, rect.left, val, rect.right-rect.left, rect.bottom-rect.top, TRUE);
}
static void ui_set_right(UI_Window* w, int val) {
	RECT rect; GetWindowRect(w->handle, &rect);
	MoveWindow(w->handle, rect.left, rect.top, val-rect.left, rect.bottom-rect.top, TRUE);
}
static void ui_set_bottom(UI_Window* w, int val) {
	RECT rect; GetWindowRect(w->handle, &rect);
	MoveWindow(w->handle, rect.left, rect.top, rect.right-rect.left, val-rect.top, TRUE);
}
static void ui_set_text(UI_Window* w, string val) {
	SetWindowText(w->handle, str_cstr(val));
}

/* link table and designer metadata - confused mashup?

static const iPair c_ipair_zero = { 0, 0 };
static const iRect c_irect_empty = { 0, 0, 0, 0 };
static const bool c_bool_true = true;
static const int c_int_zero = 0;

typeref_property ui_common_attrs[] = {
	typeref_prop_bool("show", ui_get_show, ui_set_show, &c_bool_true),
	typeref_prop_bool("visible", ui_get_visible, 0, 0),
	typeref_prop_ipair("position", ui_get_position, ui_set_position, &c_ipair_zero),
	typeref_prop_ipair("size", ui_get_size, ui_set_size, &c_ipair_zero),
	typeref_prop_irect("rect", ui_get_rect, ui_set_rect, &c_irect_empty),
	typeref_prop_int("width", ui_get_width, ui_set_width, &c_int_zero),
	typeref_prop_int("height", ui_get_height, ui_set_height, &c_int_zero),
	typeref_prop_int("left", ui_get_left, ui_set_left, &c_int_zero),
	typeref_prop_int("top", ui_get_top, ui_set_top, &c_int_zero),
	typeref_prop_int("right", ui_get_right, ui_set_right, &c_int_zero),
	typeref_prop_int("bottom", ui_get_bottom, ui_set_bottom, &c_int_zero),
	typeref_prop_str("text", ui_get_text, ui_set_text, 0),
//	typeref_prop_obj("icon", ui_get_icon, ui_set_icon, 0),
	{0}
};
*/


// AppWindow.

static bool ui_get_minimized(UI_Window* w) { return w->flags & ui_f_minimized; }
static bool ui_get_maximized(UI_Window* w) { return w->flags & ui_f_maximized; }

static void ui_showstate(UI_Window* w) {
	int cmd;
	if (w->flags & ui_f_show) {
		if (w->flags & ui_f_minimized) cmd = SW_SHOWMINIMIZED;
		else if (w->flags & ui_f_maximized) cmd = SW_SHOWMAXIMIZED;
		else cmd = SW_SHOWNORMAL;
	} else cmd = SW_HIDE;
	ShowWindow(w->handle, cmd);
}

static void ui_set_minimized(UI_Window* w, bool minimize) {
	if (minimize) w->flags |= ui_f_minimized; else w->flags &= ~ui_f_minimized;
	ui_showstate(w);
}

static void ui_set_maximized(UI_Window* w, bool maximize) {
	if (maximize) w->flags |= ui_f_maximized; else w->flags &= ~ui_f_maximized;
	ui_showstate(w);
}

static UI_Window* WP(HWND hWnd) {
	return (UI_Window*)GetWindowLongPtr(hWnd, GWL_USERDATA);
}

static LRESULT CALLBACK ui_appwindow_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
	case WM_MOVE: {
		UI_Window* w = WP(hWnd);
		RECT rect; GetWindowRect(hWnd, &rect);
		// ignore move when window becomes minimized.
		if (rect.left > -32000 && rect.top > -32000) {
			w->position.x = rect.left;
			w->position.y = rect.top;
			// TODO: move event.
		}
		return 0; }
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED) {
			UI_Window* w = WP(hWnd);
			RECT rect; GetWindowRect(hWnd, &rect);
			w->size.x = rect.right - rect.left;
			w->size.y = rect.bottom - rect.top;
			// TODO: size event.
		}
		return 0;
	case WM_CLOSE:
		// TODO: close event.
		return 0;
	case WM_ACTIVATE:
		if (wParam != WA_INACTIVE) ui_focusFirstTabStop(hWnd);
		return 0;
	case WM_DESTROY: // TODO: remove.
        PostQuitMessage(0);
		return 0;
    }

    return UI_SendEventsProc(hWnd, message, wParam, lParam);
}

#define IDC_APPICON 2

static ATOM g_appWndCls = 0;

static void ui_init_appwindow(UI_AppWindow* w) {
	UI_CreateParams params = { w, ui_appwindow_proc };
	const char* caption = w->caption ? w->caption->data : "";
	w->events = ui_no_events;
	if (!g_appWndCls)
		g_appWndCls = ui_registerWindowClass("AppWindowClass",
			(HBRUSH)(1+COLOR_APPWORKSPACE), IDC_APPICON, 0);

	w->handle = CreateWindowEx(0, "AppWindowClass", caption,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        w->position.x, w->position.y, w->size.x, w->size.y,
        0, 0, g_hInstance, &params);
	ui_showstate((UI_Window*)w);
}

generic_func nativeui_appWindow[] = {
	// destroy protocol
	ui_destroy_window,
	// widget protocol
	ui_set_position,
	ui_get_position,
	ui_set_size,
	ui_get_size,
	ui_set_visible,
	ui_get_visible,
	ui_set_text,
	ui_get_text,
	ui_set_icon,
	ui_get_icon,
	// window protocol
	ui_set_placement,
	ui_get_placement,
};

void registerAppWindow(module m)
{
	typeref t = typeref_register(m, "AppWindow", sizeof(UI_AppWindow));
	ui_reg_win_common(t);
	typeref_prop_str(t, "caption", ui_set_caption, ui_get_caption, "");
	typeref_prop_bool(t, "minimized", ui_set_minimized, ui_get_minimized, false);
	typeref_prop_bool(t, "maximized", ui_set_maximized, ui_get_maximized, false);
	typeref_init_func(t, ui_init_appwindow);
}


// Utils

static LRESULT CALLBACK ui_initProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_CREATE) {
		UI_CreateParams* params = ((LPCREATESTRUCT)lParam)->lpCreateParams;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)params->self);
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)params->proc); }
	return DefWindowProc(hWnd, message, wParam, lParam);
}

ATOM ui_registerWindowClass(LPCTSTR cls, HBRUSH bg, int iconId, DWORD style)
{
    WNDCLASSEX wcex;
    wcex.cbSize         = sizeof(WNDCLASSEX);
    wcex.style          = style;
    wcex.lpfnWndProc    = ui_initProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInstance;
    wcex.hIcon          = iconId ? LoadIcon(g_hInstance, (LPCTSTR)iconId) : NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = bg;
    wcex.lpszMenuName   = (LPCSTR)NULL;
    wcex.lpszClassName  = cls;
    wcex.hIconSm        = wcex.hIcon;
    return RegisterClassEx(&wcex);
}

LRESULT CALLBACK UI_SendEventsProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
		{ UI_Window* w = (UI_Window*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hWnd, &ps);
		UI_PaintRequest p;
		RECT c;
		GetClientRect(hWnd, &c); // TODO: ps.rcPaint instead.
		p.e.event = ui_event_paint;
		p.e.rect.left = c.left; p.e.rect.top = c.top;
		p.e.rect.right = c.right; p.e.rect.bottom = c.bottom;
		p.dc = dc;
		w->events(w->context, (UI_Event*)&p);
        EndPaint(hWnd, &ps); }
        return 0;

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        SetCapture(hWnd);
		{ UI_Window* w = (UI_Window*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        UI_PointerEvent e; e.event = ui_event_pointer;
        e.pos.x = (short)LOWORD(lParam);
        e.pos.y = (short)HIWORD(lParam);
        e.buttons =
            (wParam & (MK_LBUTTON | MK_RBUTTON)) |
            ((wParam & (MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2)) << 2);
		w->events(w->context, (UI_Event*)&e);
        return 0; }

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
        ReleaseCapture();
        // fall through...
    case WM_MOUSEMOVE:
		{ UI_Window* w = (UI_Window*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        UI_PointerEvent e; e.event = ui_event_pointer;
        e.pos.x = (short)LOWORD(lParam);
        e.pos.y = (short)HIWORD(lParam);
        e.buttons =
            (wParam & (MK_LBUTTON | MK_RBUTTON)) |
            ((wParam & (MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2)) << 2);
		w->events(w->context, (UI_Event*)&e);
        return 0; }

    case WM_MOUSEWHEEL:
		{ UI_Window* w = (UI_Window*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        POINT pt; UI_WheelEvent e; e.event = ui_event_wheel;
        // TODO: accumulate delta
        e.delta = (short)HIWORD(wParam) / (double)WHEEL_DELTA;
        pt = ui_pointFromParam(hWnd, lParam);
        e.pos.x = pt.x;
        e.pos.y = pt.y;
        e.buttons =
            (wParam & (MK_LBUTTON | MK_RBUTTON)) |
            ((wParam & (MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2)) << 2);
		w->events(w->context, (UI_Event*)&e);
        return 0; }

    case WM_KEYDOWN:
		{ UI_Window* w = (UI_Window*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        UI_KeyEvent e; e.event = ui_event_key;
        e.is_down = true;
        e.key = wParam;
		w->events(w->context, (UI_Event*)&e);
        return 0; }

    case WM_KEYUP:
		{ UI_Window* w = (UI_Window*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        UI_KeyEvent e; e.event = ui_event_key;
        e.is_down = false;
        e.key = wParam;
		w->events(w->context, (UI_Event*)&e);
        return 0; }
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

void ui_no_events(void* obj, UI_Event* e) {}

POINT ui_pointFromParam(HWND hWnd, LPARAM lParam)
{
	POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
	MapWindowPoints(NULL, hWnd, &pt, 1); // screen to client.
	return pt;
}

void ui_focusFirstTabStop(HWND hWnd)
{
	HWND hWalk = GetWindow(hWnd, GW_CHILD);
	while (hWalk) {
		if (GetWindowLong(hWalk, GWL_STYLE) & WS_TABSTOP) {
			SetFocus(hWalk); return; }
		hWalk = GetWindow(hWalk, GW_HWNDNEXT); }
}

void ui_fillRect(HDC dc, int left, int top, int right, int bottom, COLORREF col)
{
	RECT rect = { left, top, right, bottom };
	// does not include right and bottom edge.
	// draws nothing if right-left<=1 or bottom-top<=1

	COLORREF oldcr = SetBkColor(dc, col);
    ExtTextOut(dc, 0, 0, ETO_OPAQUE, &rect, "", 0, 0);
    SetBkColor(dc, oldcr);
}
