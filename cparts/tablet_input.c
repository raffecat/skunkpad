#include "defs.h"
#include "tablet_input.h"


#ifdef WINDOWS

#include "ui_win_impl.h" // to access UI_Window handle.

#include "WinTab\wintab.h"

// WINTAB packet layout
#define PACKETDATA	(PK_CONTEXT | PK_CURSOR | PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE)
#define PACKETMODE	0  //PK_BUTTONS // relative button mode
#include "WinTab\pktdef.h"

// COM - CoInitialize
#pragma comment(lib, "ole32.lib")

// Function pointers to Wintab functions exported from wintab32.dll. 
typedef UINT ( API * WTINFOA ) ( UINT, UINT, LPVOID );
typedef HCTX ( API * WTOPENA )( HWND, LPLOGCONTEXTA, BOOL );
typedef BOOL ( API * WTGETA ) ( HCTX, LPLOGCONTEXT );
typedef BOOL ( API * WTSETA ) ( HCTX, LPLOGCONTEXT );
typedef BOOL ( API * WTCLOSE ) ( HCTX );
typedef BOOL ( API * WTENABLE ) ( HCTX, BOOL );
typedef BOOL ( API * WTPACKET ) ( HCTX, UINT, LPVOID );
typedef BOOL ( API * WTOVERLAP ) ( HCTX, BOOL );
typedef BOOL ( API * WTSAVE ) ( HCTX, LPVOID );
typedef BOOL ( API * WTCONFIG ) ( HCTX, HWND );
typedef HCTX ( API * WTRESTORE ) ( HWND, LPVOID, BOOL );
typedef BOOL ( API * WTEXTSET ) ( HCTX, UINT, LPVOID );
typedef BOOL ( API * WTEXTGET ) ( HCTX, UINT, LPVOID );
typedef BOOL ( API * WTQUEUESIZESET ) ( HCTX, int );
typedef int  ( API * WTDATAPEEK ) ( HCTX, UINT, UINT, int, LPVOID, LPINT);
typedef int  ( API * WTPACKETSGET ) (HCTX, int, LPVOID);


// Loaded Wintab32 API functions.
HINSTANCE ghWintab = NULL;

WTINFOA gpWTInfoA = NULL;
WTOPENA gpWTOpenA = NULL;
//WTGETA gpWTGetA = NULL;
//WTSETA gpWTSetA = NULL;
WTCLOSE gpWTClose = NULL;
//WTPACKET gpWTPacket = NULL;
WTENABLE gpWTEnable = NULL;
WTOVERLAP gpWTOverlap = NULL;
//WTSAVE gpWTSave = NULL;
//WTCONFIG gpWTConfig = NULL;
//WTRESTORE gpWTRestore = NULL;
//WTEXTSET gpWTExtSet = NULL;
//WTEXTGET gpWTExtGet = NULL;
WTQUEUESIZESET gpWTQueueSizeSet = NULL;
//WTDATAPEEK gpWTDataPeek = NULL;
WTPACKETSGET gpWTPacketsGet = NULL;


#define GETPROCADDRESS(type, func) \
	gp##func = (type)GetProcAddress(ghWintab, #func); \
	if (!gp##func){ UnloadWintab(); return FALSE; }

void UnloadWintab()
{
	if ( ghWintab )
	{
		FreeLibrary( ghWintab );
		ghWintab = NULL;
	}

	gpWTOpenA			= NULL;
	gpWTClose			= NULL;
	gpWTInfoA			= NULL;
	//gpWTPacket			= NULL;
	gpWTEnable			= NULL;
	gpWTOverlap			= NULL;
	//gpWTSave			= NULL;
	//gpWTConfig			= NULL;
	//gpWTGetA			= NULL;
	//gpWTSetA			= NULL;
	//gpWTRestore			= NULL;
	//gpWTExtSet			= NULL;
	//gpWTExtGet			= NULL;
	gpWTQueueSizeSet	= NULL;
	//gpWTDataPeek		= NULL;
	gpWTPacketsGet		= NULL;
}

BOOL LoadWintab()
{
	CoInitialize(NULL);

	// // return FALSE; // TODO FIXME until ui_win changes are done.

	ghWintab = LoadLibraryA("Wintab32.dll");
	if ( !ghWintab )
		return FALSE;

	// Explicitly find the exported Wintab functions in which we are interested.
	// We are using the ASCII, not unicode versions (where applicable).
	GETPROCADDRESS( WTOPENA, WTOpenA );
	GETPROCADDRESS( WTINFOA, WTInfoA );
	//GETPROCADDRESS( WTGETA, WTGetA );
	//GETPROCADDRESS( WTSETA, WTSetA );
	//GETPROCADDRESS( WTPACKET, WTPacket );
	GETPROCADDRESS( WTCLOSE, WTClose );
	GETPROCADDRESS( WTENABLE, WTEnable );
	GETPROCADDRESS( WTOVERLAP, WTOverlap );
	//GETPROCADDRESS( WTSAVE, WTSave );
	//GETPROCADDRESS( WTCONFIG, WTConfig );
	//GETPROCADDRESS( WTRESTORE, WTRestore );
	//GETPROCADDRESS( WTEXTSET, WTExtSet );
	//GETPROCADDRESS( WTEXTGET, WTExtGet );
	GETPROCADDRESS( WTQUEUESIZESET, WTQueueSizeSet );
	//GETPROCADDRESS( WTDATAPEEK, WTDataPeek );
	GETPROCADDRESS( WTPACKETSGET, WTPacketsGet );

	return TRUE;
}

#endif // WINDOWS


struct TabletInput {
	UI_WindowHook hook; // must be first member.
	UI_Window* window;
	Tablet_EventFunc event; void* context;
	double press_scale;
	bool proximity;
#ifdef WINDOWS
	HCTX ctx;
	LOGCONTEXT lc;
#endif // WINDOWS
};


static bool init_tablet(TabletInput* t)
{
	//UINT ndevices, ncursors;

	if (!gpWTInfoA) {
		if (!LoadWintab())
			return false; // wintab not installed, or random.
	}

	// check if wintab is available.
	if (!gpWTInfoA(0, 0, NULL))
	{
		trace("WinTab: WTInfo reports unavailable.");
		return false;
	}

	//gpWTInfoA(WTI_INTERFACE, IFC_NDEVICES, &ndevices);
    //gpWTInfoA(WTI_INTERFACE, IFC_NCURSORS, &ncursors);

	// ask for the default system context
	if (!gpWTInfoA(WTI_DEFSYSCTX, 0, &t->lc))
	{
		trace("WinTab: couldn't retrieve system context information.");
		return false;
	}

	// set the packet data mask
	t->lc.lcPktData = PACKETDATA;
	// absolute x, y, pressure; relative buttons
	t->lc.lcPktMode = PACKETMODE;
	// what sort of movement?
	//t->lc.lcMoveMask = PK_X | PK_Y;
	// want up events to match down events.
    t->lc.lcBtnDnMask = ~0;
	t->lc.lcBtnUpMask = ~0;
	// have the context send messages to the window
	t->lc.lcOptions |= CXO_MESSAGES;
	t->lc.lcMsgBase = WT_DEFBASE;
	// change output resolution to match input
	/*
	t->lc.lcOutOrgX = 0;
	t->lc.lcOutOrgY = 0;
	t->lc.lcOutExtX = t->lc.lcInExtX - t->lc.lcInOrgX;
	t->lc.lcOutExtY = t->lc.lcInExtY - t->lc.lcInOrgY;
	*/

	// Open the Wintab context
	t->ctx = gpWTOpenA(t->window->handle, &t->lc, TRUE);
	if (!t->ctx)
	{
		trace("WinTab: couldn't open the context.");
		return false;
	}

	// Get tablet count
	{UINT num=0; gpWTInfoA(WTI_INTERFACE, IFC_NDEVICES, &num);
	trace("WinTab: %d devices supported", (int)num);}

	{UINT num=0; gpWTInfoA(WTI_INTERFACE, IFC_NCONTEXTS, &num);
	trace("WinTab: %d contexts supported", (int)num);}

	{UINT num=0; gpWTInfoA(WTI_INTERFACE, IFC_NCURSORS, &num);
	trace("WinTab: %d cursors supported", (int)num);}

	{UINT num=0; gpWTInfoA(WTI_STATUS, STA_CONTEXTS, &num);
	trace("WinTab: %d contexts open", (int)num);}

	{UINT num=0; gpWTInfoA(WTI_STATUS, STA_SYSCTXS, &num);
	trace("WinTab: %d system contexts open", (int)num);}

	// get maximum tablet pressure.
	{AXIS pressure;
	if (gpWTInfoA(WTI_DEVICES, DVC_NPRESSURE, &pressure) &&
		pressure.axMax > 0)
	{
		t->press_scale = 1.0 / (double) pressure.axMax;
	}
	else
	{
		// no pressure support, do not scale.
		t->press_scale = 1.0;
	}}

	trace("WinTab: maximum pressure: %f", 1.0 / t->press_scale);
	return true;
}

static void tablet_read_packets(TabletInput* t)
{
	PACKET pkt[32];
	int packets = gpWTPacketsGet(t->ctx, 32, pkt);
	if (packets > 0)
	{
		Tablet_InputEvent p;
		RECT rect;
		int i;

		// notify listener that a batch of events is pending.
		{ Tablet_Event e; e.event = tablet_event_begin;
		  t->event(t->context, &e); }

		GetWindowRect(t->window->handle, &rect);

		p.event = tablet_event_input;
		p.tilt = 0; p.rotation = 0;

		do
		{
			//trace("read %d packets", (int)packets);

			for (i=0; i<packets; ++i)
			{
				double pressure;

				// convert tablet space to screen space
				double x = (pkt[i].pkX - t->lc.lcOutOrgX) * (double) t->lc.lcSysExtX / (double) t->lc.lcOutExtX;
				double y = t->lc.lcSysExtY - (pkt[i].pkY - t->lc.lcOutOrgY) * (double) t->lc.lcSysExtY / (double) t->lc.lcOutExtY;

				//trace("sample %d, %d, %d (%d)", (int)pkt[i].pkX, (int)pkt[i].pkY, (int)pkt[i].pkNormalPressure, (int)pkt[i].pkButtons);

				// map screen space to client area
				x -= rect.left;
				y -= rect.top;

				pressure = pkt[i].pkNormalPressure * t->press_scale;

				p.x = x; p.y = y;
				p.pressure = pressure;
				p.buttons = pkt[i].pkButtons;
				t->event(t->context, (Tablet_Event*)&p);
			}

			packets = gpWTPacketsGet(t->ctx, 32, pkt);
		}
		while (packets > 0);

		// notify listener that no more events are pending.
		{ Tablet_Event e; e.event = tablet_event_end;
		  t->event(t->context, &e); }
	}
}

static LRESULT TabletHookProc(UI_WindowHook* hook, UINT message, WPARAM wParam, LPARAM lParam)
{
	TabletInput* t = (TabletInput*) hook;
    switch (message)
    {
	case WM_TABLET_QUERYSYSTEMGESTURESTATUS:
		// This little gem disables all the windows touch input
		// and gesture stuff so we can receive tablet input.
		return
			TABLET_DISABLE_PRESSANDHOLD |
			TABLET_DISABLE_PENTAPFEEDBACK |
			TABLET_DISABLE_PENBARRELFEEDBACK |
			TABLET_DISABLE_FLICKS |
			TABLET_DISABLE_TOUCHUIFORCEON |
			TABLET_DISABLE_TOUCHUIFORCEOFF |
			TABLET_DISABLE_TOUCHSWITCH |
			TABLET_DISABLE_SMOOTHSCROLLING |
			TABLET_DISABLE_FLICKFALLBACKKEYS;

	case WT_PACKET:
		if (t->ctx && t->event) {
			// this implies proximity; sometimes proximity events
			// are missed when the window does not have focus.
			t->proximity = true;
			// process as many packets as we can.
			tablet_read_packets(t);
		}
		return 0;

	case WT_PROXIMITY:
		if (LOWORD(lParam)) {
			// pen is entering the context.
			t->proximity = true;
			trace("in proximity %d", (int)lParam);
		} else {
			// pen is leaving the context.
			t->proximity = false;
			trace("out of proximity %d", (int)lParam);
		}
		return 0;
	}
	return hook->next->proc(hook->next, message, wParam, lParam);
}

TabletInput* tablet_input_create(
	UI_Window* targetWindow,
	Tablet_EventFunc events, void* context)
{
	TabletInput* t = cpart_new(TabletInput);
	t->hook.proc = TabletHookProc;
	t->hook.next = 0;
	t->window = targetWindow;
	t->event = events; t->context = context;
	t->press_scale = 1.0;
	t->proximity = false;
	t->ctx = 0;
	if (init_tablet(t)) {
		ui_hook_window(targetWindow, &t->hook);
		trace("installed tablet hook");
	}
	return t;
}

void tablet_input_destroy(TabletInput* t)
{
	ui_unhook_window(t->window, &t->hook);
	if (t->ctx) gpWTClose(t->ctx);
	cpart_free(t);
	UnloadWintab();
}

void tablet_activate(TabletInput* t)
{
	if (t->ctx)
		gpWTOverlap(t->ctx, TRUE);
}

void tablet_input_process(TabletInput* t, UI_PointerEvent* e)
{
	if (t->event && !t->proximity)
	{
		// notify listener that a batch of events is pending.
		{Tablet_Event e; e.event = tablet_event_begin;
		t->event(t->context, &e);}

		trace("simulated tablet event");

		// translate pointer event into pen event.
		{Tablet_InputEvent p;
		p.event = tablet_event_input;
		p.x = e->pos.x; p.y = e->pos.y;
		p.pressure = 0.5; //0.999999999999;
		p.tilt = 0.0f; p.rotation = 0.0f;
		p.buttons = e->buttons; // TODO: remap buttons?
		t->event(t->context, (Tablet_Event*)&p);}

		// notify listener that no more events are pending.
		{Tablet_Event e; e.event = tablet_event_end;
		t->event(t->context, &e);}
	}
}
