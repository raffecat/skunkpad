import * from defs
import * from ui_win_impl

typedef unsigned long (*app_run_func)(void* data);

void init(void);
void final(void);

// cleanly exit the app - begin shutdown.
void app_exit(void);

// register a scheduler callback.
void app_set_scheduler(app_run_func func, void* data);

// enable exhaustive heap checking in debug builds.
void app_heap_check();

#include <Commctrl.h>
#pragma comment(lib, "comctl32.lib")

HINSTANCE g_hInstance = 0;

static app_run_func g_app_func = 0;
static void* g_app_data = 0;
static bool g_app_running = true;

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hInstPrev, LPSTR lpCmdLine, int nCmdShow)
{
    MSG msg;

    g_hInstance = hInstance;
    InitCommonControls();

    init();

	while (g_app_running)
	{
		// process any pending windows messages.
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
				g_app_running = false;
		}

		if (g_app_running)
		{
			unsigned long wait;

			if (g_app_func)
				wait = g_app_func(g_app_data);
			else
				wait = INFINITE;

			// wait for the idle time to elapse.
			if (wait > 0)
				MsgWaitForMultipleObjectsEx(0, NULL, wait, QS_ALLINPUT, 0);
		}
	}

    final();

    return msg.wParam;
}

#ifdef _DEBUG
#include <stdio.h>
#include <stdarg.h>
void CDECL trace(const char* fmt, ...)
{
	va_list args; int i;
	char buf[1026];
	va_start(args, fmt);
	i = _vsnprintf(buf, 1023, fmt, args);
	if (i<0) i = 1023;
	buf[i] = '\r'; buf[i+1] = '\n'; buf[i+2] = '\0';
	OutputDebugString(buf);
}
#else
void CDECL trace(const char* fmt, ...) {}
#endif

void app_set_scheduler(app_run_func func, void* data)
{
	g_app_func = func;
	g_app_data = data;
}

void app_exit()
{
	PostQuitMessage(0);
}

#include <crtdbg.h>

void app_heap_check()
{
	_CrtSetDbgFlag(
		_CRTDBG_ALLOC_MEM_DF|
		_CRTDBG_CHECK_ALWAYS_DF|
		0);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_WNDW);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_WNDW);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_WNDW);
}

void memtest()
{
	BOOL memValid = _CrtCheckMemory();
	_ASSERTE(memValid);
	assert(memValid);
}


/*

// HACK when avoiding the C library.
int _fltused = 0;


* Must supply the following:

_WinMainCRTStartup
malloc, free, calloc, realloc
read, close, lseek, open (res_load.c)


* JPEG Library:

fclose, fopen


* PNG Library:

longjmp, _setjmp3
_ftol
_iob
fread
fprintf
strtod
_CIpow
_snprintf
abort


* Lua library:

fclose, fopen
longjmp, _setjmp3
_ftol
strstr
strchr
ungetc
freopen
_filbuf
_iob
fread
strerror
errno
fprintf
exit
_pctype
_isctype
__mb_cur_max
strtoul
strtod
sprintf
strncat
strcspn
strncpy
strcoll
floor
_CIpow
fgets
fputs
_CIsinh
_CIcosh
_CItanh
_CIasin
_CIacos
ceil
_CIfmod
modf
frexp
ldexp
rand
srand
_HUGE
tolower
toupper
strpbrk
memchr
_popen
tmpfile
clearerr
fscanf
fwrite
ftell
fseek
setvbuf
fflush
_pclose
strrchr
localeconv

*/
