#ifndef __CKITCORE_WIN_H__
#define __CKITCORE_WIN_H__

#include <wchar.h>
#include <vector>
#include <list>
#include <string>

#include <windows.h>

#include "ckitcore.h"

namespace ckit
{
    struct ImageWin : public ImageBase
    {
    	ImageWin( int width, int height, const char * pixels, const COLORREF * transparent_color=0, bool halftone=false );
    	virtual ~ImageWin();

    	HBITMAP handle;
    };

    struct FontWin : public FontBase
    {
    	FontWin( const wchar_t * name, int height );
    	virtual ~FontWin();

        LOGFONT logfont;
        HFONT handle;
    };

    struct ImagePlaneWin : public ImagePlaneBase
    {
    	ImagePlaneWin( struct WindowBase * window, int x, int y, int width, int height, float priority );
    	virtual ~ImagePlaneWin();

		virtual void Draw( const Rect & paint_rect );
    };

    struct TextPlaneWin : public TextPlaneBase
    {
    	TextPlaneWin( struct WindowBase * window, int x, int y, int width, int height, float priority );
    	virtual ~TextPlaneWin();

		virtual void Scroll( int x, int y, int width, int height, int delta_x, int delta_y );
		virtual void DrawOffscreen();
		void DrawHorizontalLine( int x1, int y1, int x2, COLORREF color, bool dotted );
		void DrawVerticalLine( int x1, int y1, int y2, COLORREF color, bool dotted );
        virtual void Draw( const RECT & paint_rect );

		HDC	offscreen_dc;
		HBITMAP	offscreen_bmp;
		unsigned char * offscreen_buf;
		SIZE offscreen_size;
	};

    struct WindowWin : public WindowBase
    {
        WindowWin( Param & param );
        virtual ~WindowWin();

		virtual WindowHandle getHandle() const;
        virtual void flushPaint();
  		virtual void enumFonts( std::vector<std::wstring> * font_list );
  		virtual void setBGColor( Color color );
  		virtual void setFrameColor( Color color );
  		virtual void setCaretColor( Color color0, Color color1 );
  		virtual void setMenu( PyObject * menu );
        virtual void setPositionAndSize( int x, int y, int width, int height, int origin );
        virtual void setCapture();
        virtual void releaseCapture();
		virtual void setMouseCursor( int id );
		virtual void drag( int x, int y );
		virtual void show( bool show, bool activate );
        virtual void enable( bool enable );
        virtual void activate();
        virtual void inactivate();
        virtual void foreground();
        virtual void restore();
        virtual void maximize();
        virtual void minimize();
        virtual void topmost( bool topmost );
        virtual bool isEnabled();
        virtual bool isVisible();
        virtual bool isMaximized();
        virtual bool isMinimized();
        virtual bool isActive();
        virtual bool isForeground();
        virtual void getWindowRect( Rect * rect );
        virtual void getClientSize( Size * rect );
        virtual void getNormalWindowRect( Rect * rect );
        virtual void getNormalClientSize( Size * size );
		virtual void clientToScreen(Point * point);
		virtual void screenToClient(Point * point);
		virtual void setTimer( PyObject * func, int interval );
		virtual void killTimer( PyObject * func );
		virtual void setHotKey( int vk, int mod, PyObject * func );
		virtual void killHotKey( PyObject * func );
		virtual void setText( const wchar_t * text );
		virtual bool popupMenu( int x, int y, PyObject * items );
        virtual void enableIme( bool enable );
		virtual void setImeFont( FontBase * font );

		bool _createWindow( Param & param );
		void _drawBackground( const Rect & paint_rect );
		void _drawPlanes( const Rect & paint_rect );
		void _drawCaret( const Rect & paint_rect );
		void _setImePosition();
		void _onNcPaint( HDC hDC );
		void _onSizing( DWORD edge, LPRECT rect );
		void _onWindowPositionChange( WINDOWPOS * wndpos, bool send_event );
		void _updateWindowFrameRect();
		void _onTimerCaretBlink();
		static LRESULT CALLBACK _wndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
		void _refreshMenu();
		void _buildMenu( HMENU menu_handle, PyObject * pysequence, int depth, bool parent_enabled );
		static bool _registerWindowClass();

		HWND hwnd;
		HIMC ime_context;
		LOGFONT ime_logfont;
	    DWORD style;
	    DWORD exstyle;
        SIZE window_frame_size;
        RECT last_valid_window_rect; // 最小化されていない状態のウインドウ矩形
		HDC	offscreen_dc;
		HBITMAP	offscreen_bmp;
		SIZE offscreen_size;
		HBRUSH bg_brush;
        HPEN frame_pen;
        HBRUSH caret0_brush;
        HBRUSH caret1_brush;
	};

	typedef ImageWin Image;
	typedef FontWin Font;
	typedef ImagePlaneWin ImagePlane;
	typedef TextPlaneWin TextPlane;
	typedef WindowWin Window;
};

#endif //__CKITCORE_WIN_H__
