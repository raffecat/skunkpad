#include "defs.h"
#include "glview.h"
#include "graphics.h"
#include "ui_win_impl.h"
#include "gl_impl.h"

#include <mmsystem.h> // timeGetTime
#pragma comment(lib, "winmm.lib")


// TODO: fullscreen mode still assumes too much; this behaviour really
// should be an external controller applied to the top level window.

// TODO: mode switch fullscreen is not fully implemented: there is
// no way to specify which screen mode to select, and no logic to change
// the mode back on ALT+TAB and when the window is destroyed.

// disableAero = usingSwapBuffers && (windowRect == screenRect) &&
//   ((flags & WS_POPUP) || isTopMost)   -- by observation.
// PFD_SWAP_EXCHANGE | PFD_SUPPORT_COMPOSITION


static ATOM g_wndCls = 0;

struct GLView {
	UI_WIN_FIELDS;
	UI_Window* parent;
	HDC dc;
	HGLRC rc;
	GfxBackend backend;
	GLViewSceneFunc scene;
	void* scene_data;
	DWORD lastTime;
	bool active;
	bool fullscreen;
	bool modeSwitch; // change display mode when fullscreen.
	bool useTimer;
	WINDOWPLACEMENT placement;
	DWORD oldStyle;
	RECT oldRect;
};

static bool RenderGLView(GLView* view)
{
	RECT rect;
	if (!view->rc || !IsWindowVisible(view->handle) || IsIconic(view->handle) ||
		GetClipBox(view->dc, &rect) == NULLREGION) return false;
	if (!wglMakeCurrent(view->dc, view->rc)) {
		trace("+++ cannot make OpenGL context current");
		return false;
	}
	// // (*view->backend)->render(view->backend);
	if (view->scene)
		view->scene(view->scene_data);
	return !!SwapBuffers(view->dc);
}

static void ResizeGLView(GLView* view, int width, int height)
{
	if (width > 0 && height > 0) {
		glViewport(0, 0, (GLsizei)width, (GLsizei)height);
	}
	if (view->rc)
		(*view->backend)->sized(view->backend, width, height);
}

static void ReleaseGfxContext(GLView* view)
{
	(*view->backend)->final(view->backend);
	wglMakeCurrent( NULL, NULL );
	if (view->rc) wglDeleteContext(view->rc);
	view->rc = NULL;
}

static HWND FindTopWindow(HWND hWnd)
{
	HWND hParent;
	while ((hParent = GetParent(hWnd))) hWnd = hParent;
	return hWnd;
}

static LRESULT GLWndProc(UI_WindowHook* hook, UINT message, WPARAM wParam, LPARAM lParam)
{
	GLView* view = (GLView*) hook;
	switch (message)
	{
	case WM_KILLFOCUS: // minimize on ALT+TAB or popup.
		if (view->active && view->modeSwitch && view->fullscreen) {
			// use SW_SHOWMINNOACTIVE to avoid activating the next window
			// in the z-order, which ends the ALT+TAB action early and
			// also might result in message deadlock (see MSDN.)
			ShowWindow(FindTopWindow(view->handle), SW_SHOWMINNOACTIVE); trace("* minimized"); }
		return 0;
	case WM_SETFOCUS:
		if (view->active && view->modeSwitch && view->fullscreen) {
			ShowWindow(FindTopWindow(view->handle), SW_RESTORE); trace("* restored"); }
		return 0;
	case WM_DESTROY:
		if (view) {
			if (view->useTimer) KillTimer(view->handle, 1);
			ReleaseGfxContext(view); }
		return 0;

	case WM_ERASEBKGND:
		return TRUE; // never erase.

	/*
	case WM_SETCURSOR: // need a policy.
		HT_CLIENT?
		SetCursor(NULL);
		return TRUE;
	*/

	case WM_PAINT:
		if (view) {
			if (view->active && RenderGLView(view)) {
				//trace("-- rendered in paint");
				ValidateRect(view->handle, NULL);
				if (view->useTimer) SetTimer(view->handle, 1, 1, NULL); // 16 = ~60 fps
				return 0;
			} else {
				//trace("-- nothing to render in paint");
				if (view->useTimer) KillTimer(view->handle, 1); // stop timer until WM_PAINT.
			}
		}
		break; // default

	case WM_TIMER:
		if (view->useTimer) glview_render(view);
		return 0;

	case WM_SYSKEYDOWN:
		if (wParam == VK_RETURN) {
			// ALT + ENTER: switch between fullscreen and windowed mode.
			if (view->fullscreen) glview_windowed(view);
			else glview_fullscreen(view);
			return 0;
		}
		break; // pass through otherwise.

	case WM_KEYDOWN:
		if (wParam == VK_F11) {
			// F11: switch between fullscreen and windowed mode.
			if (view->fullscreen) glview_windowed(view);
			else glview_fullscreen(view);
			return 0;
		}
		break; // pass through otherwise.
	}

	return UI_SendEventsProc(hook, message, wParam, lParam);
}

static void DestroyGLWindow(GLView* view)
{
	if (view->handle) {
		DestroyWindow(view->handle);
		view->handle = 0; view->dc = 0;
	}
}

static int GetDisplayDepth()
{
	DEVMODE devMode;
	devMode.dmSize = sizeof(DEVMODE);
	devMode.dmDriverExtra = 0;
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devMode);
	return (int) devMode.dmBitsPerPel;
}

static bool CreateOGLContext(GLView* view)
{
	PIXELFORMATDESCRIPTOR pfd;
	int nPixelFormat;

	// only permitted to do this once per window handle.
	memset( &pfd, 0, sizeof(pfd) );
	pfd.nSize      = sizeof(pfd);
	pfd.nVersion   = 1;
	pfd.dwFlags    = PFD_DOUBLEBUFFER |
					 PFD_SUPPORT_OPENGL |
					 PFD_DRAW_TO_WINDOW;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = GetDisplayDepth(); // match display.
	pfd.cDepthBits = 0;
	pfd.cAccumBits = 0;
	pfd.cStencilBits = 0;
	pfd.iLayerType = PFD_MAIN_PLANE;

	nPixelFormat = ChoosePixelFormat( view->dc, &pfd );
	if( nPixelFormat == 0 ) {
		return false;
	}

	memset( &pfd, 0, sizeof(pfd) );
	if (!DescribePixelFormat( view->dc, nPixelFormat, sizeof(pfd), &pfd )) {
		return false;
	}

	if (!SetPixelFormat( view->dc, nPixelFormat, &pfd )) {
		return false;
	}

	// create the rendering context.
	view->rc = wglCreateContext( view->dc );
	if( !view->rc ) {
		return false;
	}

	/* get the active context for sharing.
	HGLRC rcPrev;
	rcPrev = wglGetCurrentContext();
	if (rcPrev && !wglShareLists(rcPrev, view->rc)) {
		trace("*** cannot share OpenGL contexts");
	}*/

	// make the rendering context current.
	if (!wglMakeCurrent( view->dc, view->rc )) {
		return false;
	}

	(*view->backend)->init(view->backend);

	return true;
}

void CreateGLWindow(GLView* w, HWND hParent, int x, int y, int width, int height)
{
	UI_CreateParams params = { (UI_Window*)w, GLWndProc };
	CreateWindowEx(WS_EX_NOPARENTNOTIFY, "GLView_WindowClass", "",
		WS_VISIBLE | WS_CHILD | WS_TABSTOP, x, y, width, height, hParent,
		(HMENU)(100), g_hInstance, &params);
	w->dc = GetDC(w->handle); // have CS_OWNDC.
	if (w->handle)
		CreateOGLContext(w);
}

GLView* glview_create(UI_Window* parent, UI_EventFunc events, void* context)
{
	GLView* view = cpart_new(GLView);
	view->parent = parent;
	view->dc = 0; view->rc = 0;
	view->lastTime = 0;
	view->active = view->fullscreen = view->modeSwitch = false;
	view->useTimer = false;
	view->events = events ? events : ui_no_events; view->context = context;
	view->backend = createGfxOpenGLBackend();
	view->scene = 0;
	view->scene_data = 0;
	if (!g_wndCls)
		// include a background brush but do nothing in WM_ERASEBKGND;
		// this avoids flicker, but still renders a background when other
		// windows are dragged over the top (with compositing off.)
		g_wndCls = ui_registerWindowClass("GLView_WindowClass",
			(HBRUSH)GetStockObject(BLACK_BRUSH), 0,
			CS_OWNDC | CS_HREDRAW | CS_VREDRAW); // OpenGL requires OWNDC.
	CreateGLWindow(view, view->parent->handle, 0, 0, 0, 0);
	view->active = true;
	return view;
}

void glview_resize(GLView* view, int width, int height)
{
	if (view->active && /*!view->fullscreen &&*/
		view->handle && width > 0 && height > 0)
	{
		ResizeGLView(view, width, height);
		MoveWindow(view->handle, 0, 0, width, height, TRUE);
	}
}

/*
void glview_set_scene(GLView* view, GfxScene scene)
{
	(*view->backend)->setScene(view->backend, scene);
}
*/

void glview_set_scene(GLView* view, GLViewSceneFunc scene, void* data)
{
	view->scene = scene;
	view->scene_data = data;
}

void glview_fullscreen(GLView* view)
{
	MONITORINFO mi = { sizeof(mi) };
	HWND hTopWnd;
	//int width, height;
	bool modeSwitch = view->modeSwitch;
	//bool restoreFocus;

	if (view->fullscreen)
		return;

	// find the top-level window containing this glview.
	hTopWnd = FindTopWindow(view->parent->handle);

	// save top-level window state and glview state.
	view->oldStyle = GetWindowLong(hTopWnd, GWL_STYLE);

	//GetWindowRect(view->handle, &view->oldRect);
	//MapWindowPoints(NULL, view->parent->handle, (LPPOINT)&view->oldRect, 2);

	view->placement.length = sizeof(view->placement);
	if (!GetWindowPlacement(hTopWnd, &view->placement))
		return;

	// get the screen dimensions.
	if (!GetMonitorInfo(MonitorFromWindow(hTopWnd,
			MONITOR_DEFAULTTOPRIMARY), &mi))
		return;

	// stop rendering while we rearrange things.
	// // view->active = false;

	// save the focus state so we can re-focus the view.
	//restoreFocus = modeSwitch && (GetFocus() == view->handle);

/*
	// hide the top-level window without redraw to avoid extra drawing.
	// on composited desktops this just hides the window normally.
	SetWindowPos(hTopWnd, NULL, 0, 0, 0, 0,
		SWP_HIDEWINDOW | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
*/

	// invalidate first to prevent spurious copying of screen contents,
	// unless the app wants to preserve window contents? (TODO a flag)
	//+InvalidateRect(hTopWnd, NULL, TRUE);

	if (modeSwitch)
	{
		// We need to choose a new pixel format, and for that we need
		// a new window (you can only set a window's pixel format once)
		//DestroyGLWindow(view);
	}

	// start to ingore size events before changing screen mode
	// or fiddling with the window style.
    view->fullscreen = true;

	// change top level window style (SetWindowPos per MSDN.)
	SetWindowLong(hTopWnd, GWL_STYLE,
		view->oldStyle & ~WS_OVERLAPPEDWINDOW);
	SetWindowPos(hTopWnd, HWND_TOP,
		mi.rcMonitor.left, mi.rcMonitor.top,
		mi.rcMonitor.right - mi.rcMonitor.left,
		mi.rcMonitor.bottom - mi.rcMonitor.top,
		//SWP_NOZORDER | 
		SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

	if (modeSwitch)
	{
		// create a new GL view covering the top level window.
		//CreateGLWindow(view, hTopWnd, 0, 0, width, height);
	}
	else
	{
		// since we are ignoring size events (active false),
		// manually resize the GL view window.
		// SetWindowPos(view->handle, NULL, 0, 0, width, height, SWP_NOZORDER);
		// // MoveWindow(view->handle, 0, 0, width, height, TRUE);
	}

	// allow rendering again.
	// // view->active = true;

	// update the viewport and notify the scene.
	//ResizeGLView(view, width, height);

	// invalidate again to redraw the new view.
	//InvalidateRect(hTopWnd, NULL, TRUE);

/*
	// show the top level window to start the paint cycle.
	ShowWindow(hTopWnd, SW_SHOW);
*/

	// restore focus to the new GL view window.
	//if (restoreFocus)
	//	SetFocus(view->handle);
}

void glview_windowed(GLView* view)
{
	HWND hTopWnd;
	//int width, height;
	bool modeSwitch = view->modeSwitch;
	//bool restoreFocus;

	if (!view->fullscreen) 
		return;

	// find the top-level window containing this glview.
	hTopWnd = FindTopWindow(view->parent->handle);

	// stop rendering while we rearrange things.
	//view->active = false;

	// save the focus state so we can re-focus the view.
	//restoreFocus = modeSwitch && (GetFocus() == view->handle);

	if (modeSwitch)
	{
		// We need to choose a new pixel format, and for that we need
		// a new window (you can only set a window's pixel format once)
		//DestroyGLWindow(view);
	}

	// invalidate first to prevent spurious copying of screen contents,
	// unless the app wants to preserve window contents? (TODO a flag)
	//InvalidateRect(hTopWnd, NULL, TRUE);

	// restore top level window style (SetWindowPos per MSDN.)
	SetWindowLong(hTopWnd, GWL_STYLE, view->oldStyle);
	SetWindowPlacement(hTopWnd, &view->placement);
	SetWindowPos(hTopWnd, NULL, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
		SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

	// restore the window position as it was before fullscreen.

	// TODO: it would be better to accept size events and just
	// save the size until we return from fullscreen mode.
	// * right now we can get out of sync if the app repositions
	// the gl view while we are in fullscreen mode - FIXME.
	// * the general problem here is that we should not be moving
	// the gl view window at all, unless requested by the app.
	//width = view->oldRect.right - view->oldRect.left;
	//height = view->oldRect.bottom - view->oldRect.top;

	if (modeSwitch)
	{
		// create a new GL view at the last windowed position.
		//CreateGLWindow(view, view->parent->handle,
		//	view->oldRect.left, view->oldRect.top, width, height);
	}
	else
	{
		// since we are ignoring size events (active false),
		// manually adjust the GL view size.
		//MoveWindow(view->handle, 0, 0, width, height, TRUE);
	}

	// start listening to size events again.
	view->fullscreen = false;

	// allow rendering again.
	//view->active = true;

	// update the viewport and notify the scene.
	//ResizeGLView(view, width, height);

	// invalidate again to draw the new view.
	//InvalidateRect(hTopWnd, NULL, TRUE);

	// restore focus to the new GL view window.
	//if (restoreFocus)
	//	SetFocus(view->handle);
}

void glview_render(GLView* view)
{
	if (view->active) {
		DWORD now = timeGetTime();
		DWORD delta = now - view->lastTime;
		if (delta > 0) {
			view->lastTime = now;
			(*view->backend)->update(view->backend, delta);
		}
		if (RenderGLView(view)) {
			//trace("-- rendered the view");
			ValidateRect(view->handle, NULL);
		}
		else {
			//trace("-- nothing to render");
			Sleep(10); // don't hog cpu.
		}
	}
}

void glview_destroy(GLView* view)
{
	DestroyGLWindow(view);
	free(view);
}

GfxContext glview_get_context(GLView* view)
{
	return (*view->backend)->getContext(view->backend);
}
