import * from ui

#define WINVER         0x0500   // Windows 2000
#define _WIN32_WINDOWS 0x0500
#define _WIN32_WINNT   0x0500
#define _WIN32_IE      0x0401   // IE 4.01 (common controls)

#include <windows.h>

#ifndef GetWindowLongPtr
//#error Platform SDK is too old (Feb 2003 recommended)
#define GetWindowLongPtr GetWindowLong
#define SetWindowLongPtr SetWindowLong
#define GWLP_USERDATA GWL_USERDATA
#define GWLP_WNDPROC GWL_WNDPROC
#define LONG_PTR LONG
#define MK_XBUTTON1 0
#define MK_XBUTTON2 0
#endif

// Vista and Windows 7 gesture input.
#ifndef WM_TABLET_QUERYSYSTEMGESTURESTATUS
#define WM_TABLET_QUERYSYSTEMGESTURESTATUS 716
#define TABLET_DISABLE_PRESSANDHOLD        0x00000001
#define TABLET_DISABLE_PENTAPFEEDBACK      0x00000008
#define TABLET_DISABLE_PENBARRELFEEDBACK   0x00000010
#define TABLET_DISABLE_TOUCHUIFORCEON      0x00000100
#define TABLET_DISABLE_TOUCHUIFORCEOFF     0x00000200
#define TABLET_DISABLE_TOUCHSWITCH         0x00008000
#define TABLET_DISABLE_FLICKS              0x00010000
#define TABLET_DISABLE_SMOOTHSCROLLING     0x00080000
#define TABLET_DISABLE_FLICKFALLBACKKEYS   0x00100000
#endif

extern HINSTANCE g_hInstance; // see app_win.c

// Window Hooks:

// When a window is hooked, we replace its WNDPROC and USERDATA with the
// hook proc and hook object respectively. When we do this, we save the
// old WNDPROC and USERDATA in the UI_WindowHook struct in the hook object.
// The hook proc will get its attached object and locate its UI_WindowHook
// struct. When it calls the old proc, it cannot get its USERDATA.

// So, a dispatch proc calls the first hook proc (UI_WindowHook pointer
// in the USERDATA) and that calls the next hook in the chain.

typedef struct UI_WindowHook UI_WindowHook;
typedef LRESULT (*UI_WindowProc)(UI_WindowHook* hook, UINT message, WPARAM wParam, LPARAM lParam);
struct UI_WindowHook { UI_WindowProc proc; UI_WindowHook* next; };

bool ui_hook_window(UI_Window* w, UI_WindowHook* hook);
bool ui_unhook_window(UI_Window* w, UI_WindowHook* hook);

// Window

#define UI_WIN_FIELDS  UI_WindowHook hook; HWND handle; void* context; UI_EventFunc events

typedef struct UI_PaintRequest { UI_PaintEvent e; HDC dc; } UI_PaintRequest;

// Components

struct SurfaceData;

struct UI_Window { UI_WIN_FIELDS; };
struct UI_AppWindow { UI_WIN_FIELDS; string caption; };
struct UI_ScrollView { UI_WIN_FIELDS; iPair extent; iPair scroll; };

// Utils

ATOM ui_registerWindowClass(LPCTSTR cls, HBRUSH bg, int iconId, DWORD style);
typedef struct UI_CreateParams { UI_Window* self; UI_WindowProc proc; } UI_CreateParams;

LRESULT UI_SendEventsProc(UI_WindowHook* hook, UINT message, WPARAM wParam, LPARAM lParam);
void ui_no_events(void* obj, UI_Event* e);

POINT ui_pointFromParam(HWND hWnd, LPARAM lParam);
void ui_focusFirstTabStop(HWND hWnd);
void ui_fillRect(HDC dc, int left, int top, int right, int bottom, COLORREF col);
