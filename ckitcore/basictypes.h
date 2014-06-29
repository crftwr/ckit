#ifndef _BASICTYPES_H_
#define _BASICTYPES_H_

#if defined(_WIN32)
# define PLATFORM_WIN32
#elif defined(TARGET_OS_MAC)
# define PLATFORM_MAC
#else
# error unsupported platform.
#endif


#if defined(PLATFORM_WIN32)

#include <windows.h>

namespace ckit
{
	typedef POINT Point;
	typedef SIZE Size;
	typedef RECT Rect;
	typedef COLORREF Color;
	typedef HWND WindowHandle;
};
#endif

#if defined(PLATFORM_MAC)
namespace ckit
{
	typedef Point Point;
	typedef SIZE Size;
	typedef RECT Rect;
	typedef COLORREF Color;
	typedef HWND WindowHandle;
};
#endif

const int ORIGIN_X_LEFT     = 0;
const int ORIGIN_X_CENTER   = 1<<0;
const int ORIGIN_X_RIGHT    = 1<<1;
const int ORIGIN_Y_TOP      = 0;
const int ORIGIN_Y_CENTER   = 1<<2;
const int ORIGIN_Y_BOTTOM   = 1<<3;

const int MOUSE_CURSOR_APPSTARTING	= 1;
const int MOUSE_CURSOR_ARROW	    = 2;
const int MOUSE_CURSOR_CROSS	    = 3;
const int MOUSE_CURSOR_HAND	        = 4;
const int MOUSE_CURSOR_HELP	        = 5;
const int MOUSE_CURSOR_IBEAM	    = 6;
const int MOUSE_CURSOR_NO	        = 7;
const int MOUSE_CURSOR_SIZEALL	    = 8;
const int MOUSE_CURSOR_SIZENESW	    = 9;
const int MOUSE_CURSOR_SIZENS	    = 10;
const int MOUSE_CURSOR_SIZENWSE	    = 11;
const int MOUSE_CURSOR_SIZEWE	    = 12;
const int MOUSE_CURSOR_UPARROW	    = 13;
const int MOUSE_CURSOR_WAIT         = 14;

#endif // _BASICTYPES_H_
