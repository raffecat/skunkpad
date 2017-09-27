#include "defs.h"
#include "ui.h"
#include "ui_win_impl.h"


void ui_show_window(UI_Window* w, bool visible) {
	ShowWindow(w->handle, visible ? SW_SHOW : SW_HIDE);
}

void ui_size_window(UI_Window* w, int width, int height) {
	MoveWindow(w->handle, 0, 0, width, height, TRUE);
}

void ui_focus_window(UI_Window* w) {
	SetFocus(w->handle);
}

void ui_invalidate(UI_Window* w) {
	InvalidateRect(w->handle, NULL, TRUE);
}

void ui_invalidate_rect(UI_Window* w, int left, int top, int right, int bottom) {
	RECT rect = { left, top, right, bottom };
	InvalidateRect(w->handle, &rect, TRUE);
	UpdateWindow(w->handle);
}

void ui_commit_window(UI_Window* w) {
	if (w->handle) // avoid redrawing the desktop.
		RedrawWindow(w->handle, NULL, NULL, RDW_UPDATENOW);
}

void ui_scroll_window(UI_Window* w, int dx, int dy) {
	ScrollWindow(w->handle, dx, dy, NULL, NULL);
}

void ui_reparent_window(UI_Window* w, UI_Window* parent) {
	SetParent(w->handle, parent->handle);
}

void ui_destroy_window(UI_Window* w) {
	DestroyWindow(w->handle);
	cpart_free(w);
}

void ui_center_window(UI_Window* w) {
	RECT win = {0}, work = {0};
	if (GetWindowRect(w->handle, &win) &&
		SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0)) {
		int left = (work.right - work.left)/2 - (win.right - win.left)/2;
		int top = (work.bottom - work.top)/2 - (win.bottom - win.top)/2;
		if (left < 0) left = 0;  if (top < 0) top = 0;
		MoveWindow(w->handle, left, top,
			win.right - win.left, win.bottom - win.top, TRUE);
	}
}

iPair ui_get_pointer_pos() {
	// check if processing a window message,
	// which has its own position at the time of the message.
	iPair pos;
	DWORD lParam = GetMessagePos();
    pos.x = (short)LOWORD(lParam);
    pos.y = (short)HIWORD(lParam);
	return pos;
}

iPair ui_get_window_size(UI_Window* w) {
	RECT rect; GetWindowRect(w->handle, &rect);
	{iPair pr={rect.right-rect.left, rect.bottom-rect.top}; return pr;}
}

bool ui_hook_window(UI_Window* w, UI_WindowHook* hook) {
	UI_WindowHook* head = (UI_WindowHook*)
		GetWindowLongPtr(w->handle, GWLP_USERDATA);
	// check if the hook is already attached.
	UI_WindowHook* walk = head;
	do {
		if (walk == hook)
			return false; // hook already present.
		walk = walk->next;
	} while (walk);
	// attach the hook at the head of the chain.
	hook->next = head;
	SetWindowLongPtr(w->handle, GWLP_USERDATA, (LONG_PTR)hook);
	return true; // installed the hook.
}

bool ui_unhook_window(UI_Window* w, UI_WindowHook* hook) {
	UI_WindowHook* walk = (UI_WindowHook*)
		GetWindowLongPtr(w->handle, GWLP_USERDATA);
	if (walk == hook) {
		// remove the hook from the head of the chain.
		SetWindowLongPtr(w->handle, GWLP_USERDATA, (LONG_PTR)walk->next);
		return true; // removed the hook.
	} else if (walk) {
		// find the hook in the chain.
		do {
			if (walk->next == hook) {
				walk->next = hook->next;
				return true; // removed the hook.
			}
		}
		while (walk->next);
	}
	return false; // hook was not present.
}



// AppWindow

#define IDC_APPICON 2

static ATOM g_appWndCls = 0;

static LRESULT UI_AppWindowProc(UI_WindowHook* hook, UINT message, WPARAM wParam, LPARAM lParam)
{
	UI_AppWindow* w = (UI_AppWindow*) hook;
    switch (message)
    {
	case WM_ACTIVATE:
		if (wParam != WA_INACTIVE) ui_focusFirstTabStop(w->handle);
		return 0;
	case WM_DESTROY:
        PostQuitMessage(0);
		return 0;
    }
    return UI_SendEventsProc(hook, message, wParam, lParam);
}

UI_Window* ui_create_app_window(const char* title, UI_EventFunc events, void* context)
{
	UI_AppWindow* w = cpart_new(UI_AppWindow);
	UI_CreateParams params = { (UI_Window*)w, UI_AppWindowProc };
	int width, height; RECT work={0};
	w->events = events ? events : ui_no_events; w->context = context;
	if (!g_appWndCls)
		g_appWndCls = ui_registerWindowClass("AppWindowClass",
			(HBRUSH)(1+COLOR_APPWORKSPACE), IDC_APPICON, 0);
	SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
	width = work.right - work.left;
	height = work.bottom - work.top;
	if (width < 500 || height < 500) {
		width = CW_USEDEFAULT; height = 0;
	} else {
		width = width * 80 / 100; height = height * 85 / 100;
		if (width > 1280) width = 1280;
		if (height > 1024) height = 1024;
	}
	CreateWindowEx(0, "AppWindowClass", title,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE,
        CW_USEDEFAULT, 0, width, height,
        0, 0, g_hInstance, &params);
	return (UI_Window*)w;
}


// Utils

static LRESULT CALLBACK UI_DispatchProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// Get the UI_Window instance attached to the window handle and dispatch
	// the message to the first UI_WindowHook in the window's chain.
	UI_WindowHook* hook = (UI_WindowHook*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
	return hook->proc(hook, message, wParam, lParam);
}

static LRESULT CALLBACK UI_InitProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// Wait for the create message, which contains a pointer to the UI_Window
	// instance. Attach instance to window and redirect to DispatchProc.
	if (message == WM_CREATE) {
		UI_CreateParams* params = ((LPCREATESTRUCT)lParam)->lpCreateParams;
		params->self->hook.proc = params->proc;
		params->self->hook.next = 0;
		params->self->handle = hWnd;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)&params->self->hook);
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)UI_DispatchProc);
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

ATOM ui_registerWindowClass(LPCTSTR cls, HBRUSH bg, int iconId, DWORD style)
{
    WNDCLASSEX wcex;
    wcex.cbSize         = sizeof(WNDCLASSEX);
    wcex.style          = style;
    wcex.lpfnWndProc    = UI_InitProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInstance;
    wcex.hIcon          = iconId ? LoadIcon(g_hInstance, (LPCTSTR)iconId) : NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = bg;
    wcex.lpszMenuName   = (LPCTSTR)NULL;
    wcex.lpszClassName  = cls;
    wcex.hIconSm        = wcex.hIcon;
    return RegisterClassEx(&wcex);
}

LRESULT UI_SendEventsProc(UI_WindowHook* hook, UINT message, WPARAM wParam, LPARAM lParam)
{
	UI_Window* w = (UI_Window*) hook;
    switch (message)
    {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(w->handle, &ps);
		UI_PaintRequest p;
		RECT c = ps.rcPaint;
		//GetClientRect(w->handle, &c); // TODO: ps.rcPaint instead.
		p.e.event = ui_event_paint;
		p.e.rect.left = c.left; p.e.rect.top = c.top;
		p.e.rect.right = c.right; p.e.rect.bottom = c.bottom;
		p.dc = dc;
		w->events(w->context, (UI_Event*)&p);
        EndPaint(w->handle, &ps); }
        return 0;

	case WM_SETFOCUS: {
		UI_Event e = { ui_event_focus };
		w->events(w->context, &e);
		return 0; }

    case WM_SIZE: {
		// NB. caller might have adjusted client size (e.g. scrollbars)
		RECT rect; GetClientRect(w->handle, &rect);
		{UI_SizeEvent e = { ui_event_size, rect.right, rect.bottom };
		w->events(w->context, (UI_Event*)&e);
		return 0; }}

    case WM_CLOSE: {
		UI_Event e = { ui_event_close };
		w->events(w->context, &e);
		break; /* allow destroy */ }

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        SetCapture(w->handle);
        {UI_PointerEvent e; e.event = ui_event_pointer;
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
		{UI_PointerEvent e; e.event = ui_event_pointer;
        e.pos.x = (short)LOWORD(lParam);
        e.pos.y = (short)HIWORD(lParam);
        e.buttons =
            (wParam & (MK_LBUTTON | MK_RBUTTON)) |
            ((wParam & (MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2)) << 2);
		w->events(w->context, (UI_Event*)&e);
        return 0; }

    case WM_MOUSEWHEEL:
		{POINT pt; UI_WheelEvent e; e.event = ui_event_wheel;
        e.delta = (short)HIWORD(wParam);
        pt = ui_pointFromParam(w->handle, lParam);
        e.pos.x = pt.x;
        e.pos.y = pt.y;
        e.buttons =
            (wParam & (MK_LBUTTON | MK_RBUTTON)) |
            ((wParam & (MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2)) << 2);
		w->events(w->context, (UI_Event*)&e);
        return 0; }

    case WM_KEYDOWN:
		{UI_KeyEvent e; e.event = ui_event_key;
        e.is_down = true;
        e.key = wParam;
		w->events(w->context, (UI_Event*)&e);
        return 0; }

    case WM_KEYUP:
		{UI_KeyEvent e; e.event = ui_event_key;
        e.is_down = false;
        e.key = wParam;
		w->events(w->context, (UI_Event*)&e);
        return 0; }
	}
	return DefWindowProc(w->handle, message, wParam, lParam);
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

void ui_fill_rect(UI_PaintEvent* e, int left, int top, int right, int bottom, RGBA col)
{
	HDC dc = ((UI_PaintRequest*)e)->dc;

	// does not include right and bottom edge.
	// draws nothing if right-left<=1 or bottom-top<=1
	RECT rect = { left, top, right, bottom };

	COLORREF oldcr = SetBkColor(dc, RGB(col.r, col.g, col.b));
    ExtTextOut(dc, 0, 0, ETO_OPAQUE, &rect, "", 0, 0);
    SetBkColor(dc, oldcr);
}

void ui_report_error(const char* title, const char* message)
{
	HWND hWnd = GetActiveWindow(); // if there is one.
	MessageBox(hWnd, message, title, MB_OK);
}
