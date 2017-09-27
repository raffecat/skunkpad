#ifndef CPART_UI
#define CPART_UI

// UI Protocol
// This is the interface to native UI widgets on the platform.

// With dispatcher: handles type safety for different window structs,
// need macros or helpers to call conveniently (can hide dispatch)

// Without dispatcher: api same as helpers, must check types internally,
// but only one implementation of the protocol is possible at link time.

// In effect, there would be a direct api and a dispatch api,
// and either can be implemented on top of the other - although direct
// on dispatch allows more than one implementation underneath.

struct SurfaceData;
struct Renderer;

typedef struct UI_Window UI_Window;
typedef struct UI_AppWindow UI_AppWindow;
typedef struct UI_ScrollView UI_ScrollView;
typedef struct UI_RenderToView UI_RenderToView;

typedef enum UI_EventEnum { ui_event_paint, ui_event_size, ui_event_close,
	ui_event_pointer, ui_event_wheel, ui_event_key, ui_event_scroll,
	ui_event_focus } UI_EventEnum;

typedef struct UI_Event { UI_EventEnum event; } UI_Event;
typedef struct UI_SizeEvent { UI_EventEnum event; int width; int height; } UI_SizeEvent;
typedef struct UI_PaintEvent { UI_EventEnum event; iRect rect; } UI_PaintEvent;
typedef struct UI_PointerEvent { UI_EventEnum event; iPair pos; int buttons; } UI_PointerEvent;
typedef struct UI_WheelEvent { UI_EventEnum event; iPair pos; int buttons; int delta; } UI_WheelEvent;
typedef struct UI_KeyEvent { UI_EventEnum event; int key; bool is_down; } UI_KeyEvent;

typedef void (*UI_EventFunc)(void* obj, UI_Event* e);
typedef void (*UI_RenderFunc)(void* obj, struct Renderer* r, iRect rect);

/* -- dispatch api
typedef enum UI_Message {
	uiMsgShow,			// bool
	uiMsgMove,			// iPair
	uiMsgSize,			// iPair
	uiMsgMoveSize,		// iRect
	uiMsgScroll,		// iPair
	uiMsgScrollTo,		// iPair
	uiMsgSetExtent,		// iPair
	uiMsgInvalidate,	// [iRect]
	uiMsgSetText,		// string
	uiMsgReparent,		// UI_Window
} UI_Message;

typedef int (*UI_MessageFunc)(UI_Window* window, UI_Message msg, void* data);

#define ui_show_window(window, visible) \
	do { UI_Window* w=(window); bool v=(visible); w->message(w, uiMsgShow, &v); } while(0)
*/

UI_Window* ui_create_app_window(const char* title, UI_EventFunc events, void* context);
UI_Window* ui_create_scroll_view(UI_Window* parent, UI_EventFunc events, void* context);
UI_RenderToView* ui_create_render_to_view(UI_Window* w);

void ui_show_window(UI_Window* w, bool visible);
void ui_size_window(UI_Window* w, int width, int height);
void ui_focus_window(UI_Window* w);
void ui_invalidate(UI_Window* w);
void ui_invalidate_rect(UI_Window* w, int left, int top, int right, int bottom);
void ui_commit_window(UI_Window* w);
void ui_scroll_window(UI_Window* w, int dx, int dy);
void ui_center_window(UI_Window* w);
void ui_reparent_window(UI_Window* w, UI_Window* parent);
void ui_destroy_window(UI_Window* w);
iPair ui_get_pointer_pos(void);
iPair ui_get_window_size(UI_Window* w);

// set scroll extent and position at the same time, to avoid scroll bars
// jumping around and generating spurious scroll events.
void ui_scroll_view_set_extent_pos(UI_Window* win, iPair extent, iPair pos);
void ui_scroll_view_set_extent(UI_Window* w, iPair extent);
void ui_scroll_view_set_pos(UI_Window* w, iPair pos);

void ui_fill_rect(UI_PaintEvent* e, int left, int top, int right, int bottom, RGBA col);
//void ui_render_to_view(UI_RenderToView* r, UI_PaintEvent* e, struct SurfaceData* surf, iRect doc, iRect view, int scale, int anti);
void ui_render_to_view(UI_RenderToView* r, UI_PaintEvent* e, UI_RenderFunc draw, void* context);

void ui_report_error(const char* title, const char* message);

#endif
