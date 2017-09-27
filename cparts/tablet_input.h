#ifndef CPART_TABLET_INPUT
#define CPART_TABLET_INPUT

// TabletInput
//
// Accepts input from any WinTab device.


struct UI_Window; // depends on UI
struct UI_PointerEvent;

typedef struct TabletInput TabletInput;

typedef enum Tablet_EventEnum {
	tablet_event_input = 1,  // pen input received.
	tablet_event_begin = 2,  // beginning of input events.
	tablet_event_end   = 3,  // end of input events.
} Tablet_EventEnum;

typedef struct Tablet_Event { Tablet_EventEnum event; } Tablet_Event;
typedef struct Tablet_InputEvent { Tablet_EventEnum event; double x, y; double pressure; float tilt, rotation; int buttons; } Tablet_InputEvent;

typedef void (*Tablet_EventFunc)(void* obj, Tablet_Event* e);


TabletInput* tablet_input_create(
	struct UI_Window* targetWindow,
	Tablet_EventFunc events, void* context);

void tablet_input_destroy(TabletInput* input);
void tablet_activate(TabletInput* input);

// process pointer events from the target window.
void tablet_input_process(TabletInput* t, struct UI_PointerEvent* e);


#endif
