#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <TlHelp32.h>
#include <tpcshrd.h>
#include <CommCtrl.h>

#include "mhook-lib/mhook.h"
#include "guicon.h"

#include <stdlib.h>
#include <stdio.h>

#ifdef _DEBUG
#define dprintf printf
#else
#define dprintf(...)
#endif

char *GetErrorMessage()
{
	static char msgbuf[1024];
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msgbuf, sizeof(msgbuf), NULL);
	return msgbuf;
}

bool isTouchMouseEvent()
{
	const LONG_PTR cSignatureMask = 0xFFFFFF00;
	const LONG_PTR cFromTouch = 0xFF515700;
	return (GetMessageExtraInfo() & cSignatureMask) == cFromTouch;
}

POINTS touchCorrection;
bool touching = false;

typedef BOOL(WINAPI *SetCursorPos_t)(_In_ int X, _In_ int Y);
SetCursorPos_t OrigSetCursorPos = (SetCursorPos_t)GetProcAddress(GetModuleHandle(L"user32"), "SetCursorPos");
BOOL WINAPI MySetCursorPos(_In_ int x, _In_ int y)
{
	dprintf("scp %i %i\n", x, y);
	if (touching) {
		POINT p;
		GetCursorPos(&p);
		touchCorrection.x = (SHORT)(p.x - x);
		touchCorrection.y = (SHORT)(p.y - y);
		return true;
	}
	else {
		return OrigSetCursorPos(x, y);
	}
}

LRESULT CALLBACK HookedWindowProc(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	DWORD_PTR dwRefData
)
{
#ifdef _DEBUG
	if (uMsg == WM_APP && lParam == 0xDEADBEEF && wParam == 0xCAFE) {
		dprintf("self-remove\n");
		RemoveWindowSubclass(hWnd, HookedWindowProc, 1);
		Mhook_Unhook((PVOID *)&OrigSetCursorPos);
		return 0xBABE;
	}
#endif

	switch (uMsg) {
	case WM_TABLET_QUERYSYSTEMGESTURESTATUS:
		dprintf("question\n");
		return TABLET_DISABLE_PRESSANDHOLD;
	case WM_NCDESTROY:
		RemoveWindowSubclass(hWnd, HookedWindowProc, uIdSubclass);
		break;
	}
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

bool firstTime = true;
wchar_t winTitle[20];

extern "C" __declspec(dllexport) LRESULT CALLBACK EntryHook(int nCode, WPARAM wParam, LPARAM lParam)
{
	MSG &msg = *(MSG *)lParam;
	if (firstTime) {
		GetWindowText(msg.hwnd, winTitle, _countof(winTitle));
		if (!lstrcmp(winTitle, L"Traktor")) {
			dprintf("First time\n");
			firstTime = false;
			SetWindowSubclass(msg.hwnd, HookedWindowProc, 1, 0);
			Mhook_SetHook((PVOID *)&OrigSetCursorPos, MySetCursorPos);
			dprintf("hook done\n");
		}
	}

	if (msg.message == WM_LBUTTONDOWN && isTouchMouseEvent()) {
		touching = true;
		touchCorrection.x = touchCorrection.y = 0;
		dprintf("touch down\n");
	}
	if (msg.message == WM_LBUTTONUP && isTouchMouseEvent()) {
		touching = false;
		dprintf("touch up\n");
	}
	if (touching && (msg.message >= WM_MOUSEFIRST) && (msg.message <= WM_MOUSELAST) && isTouchMouseEvent()) {
		POINTS &lPoint = MAKEPOINTS(msg.lParam);
		dprintf("touchy mouse %i %i -> %i %i\n", lPoint.x, lPoint.y, lPoint.x - touchCorrection.x, lPoint.y - touchCorrection.y);
		lPoint.x -= touchCorrection.x;
		lPoint.y -= touchCorrection.y;
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
#ifdef _DEBUG
		RedirectIOToConsole();
#endif
		break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
		break;
    case DLL_PROCESS_DETACH:
#ifdef _DEBUG
		CloseConsole();
#endif
        break;
    }
    return TRUE;
}
