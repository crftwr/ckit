#ifndef _BASICTYPES_H_
#define _BASICTYPES_H_

#if defined(_WIN32)
# define PLATFORM_WIN32
#elif defined(__APPLE__)
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

#include <CoreGraphics/CGGeometry.h>

namespace ckit
{
    struct Point
    {
        int x;
        int y;
    };
    
    struct Size
    {
        int cx;
        int cy;
    };
    
    struct Rect
    {
        int left;
        int top;
        int right;
        int bottom;
        
        Rect()
        {
        }
        
        Rect( int _left, int _top, int _right, int _bottom )
            :
            left(_left),
            top(_top),
            right(_right),
            bottom(_bottom)
        {
        }
        
        // FIXME : 座標系を合わせる
        Rect( const CGRect & rect )
            :
            left(rect.origin.x),
            top(rect.origin.y),
            right(rect.origin.x+rect.size.width),
            bottom(rect.origin.y+rect.size.height)
        {
        }
        
        // FIXME : 座標系を合わせる
        operator CGRect ()
        {
            return CGRectMake(left,top,right-left,bottom-top);
        }
    };
    
    struct Color
    {
        unsigned char r;
        unsigned char g;
        unsigned char b;
        unsigned char a;

        static Color FromRgb( unsigned char r, unsigned char g, unsigned char b )
        {
            Color color;
            color.r = r;
            color.g = g;
            color.b = b;
            color.a = 255;
            return color;
        }
        
        static Color FromRgba( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
        {
            Color color;
            color.r = r;
            color.g = g;
            color.b = b;
            color.a = a;
            return color;
        }
        
        static Color FromInt( unsigned int src )
        {
            Color color;
            color.r = ((unsigned char*)&src)[0];
            color.g = ((unsigned char*)&src)[1];
            color.b = ((unsigned char*)&src)[2];
            color.a = ((unsigned char*)&src)[3];
            return color;
        }
        
        bool operator==( const Color & right ) const
        {
            return (r==right.r and g==right.g and b==right.b and a==right.a);
        }
        
        static Color Zero()
        {
            return FromRgba(0,0,0,0);
        }
    };
    
	typedef void* WindowHandle;
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
