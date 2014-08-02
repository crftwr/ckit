#ifndef __CKITCORE_MAC_H__
#define __CKITCORE_MAC_H__

#include <wchar.h>
#include <vector>
#include <list>
#include <string>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CGImage.h>
#include <CoreGraphics/CGLayer.h>
#include <CoreText/CTFont.h>
#include <CoreText/CTFontCollection.h>
#include <CoreText/CTFontDescriptor.h>
#include <CoreText/CTStringAttributes.h>
#include <CoreText/CTLine.h>

#include "ckitcore.h"
#include "ckitcore_cocoa_export.h"

namespace ckit
{
    struct ImageMac : public ImageBase
    {
    	ImageMac( int width, int height, const char * pixels, const Color * transparent_color=0, bool halftone=false );
    	virtual ~ImageMac();

    	CGImageRef handle;
        unsigned char * buffer;
    };

    struct FontMac : public FontBase
    {
    	FontMac( const wchar_t * name, int height );
    	virtual ~FontMac();

        CTFontRef handle;
        CGFloat ascent;
        CGFloat descent;
    };

    struct ImagePlaneMac : public ImagePlaneBase
    {
    	ImagePlaneMac( struct WindowBase * window, int x, int y, int width, int height, float priority );
    	virtual ~ImagePlaneMac();

		virtual void Draw( const Rect & paint_rect );
    };

    struct TextPlaneMac : public TextPlaneBase
    {
    	TextPlaneMac( struct WindowBase * window, int x, int y, int width, int height, float priority );
    	virtual ~TextPlaneMac();

		virtual void Scroll( int x, int y, int width, int height, int delta_x, int delta_y );
		virtual void DrawOffscreen();
		virtual void Draw( const Rect & paint_rect );

        CGLayerRef offscreen_handle;
        CGContextRef offscreen_context;
		Size offscreen_size;
	};

    struct WindowMac : public WindowBase
    {
        static void initializeSystem( const wchar_t * prefix );
        static void terminateSystem();
        
        WindowMac( Param & param );
        virtual ~WindowMac();

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
		virtual void setTimer( TimerInfo * timer_info );
		virtual void killTimer( TimerInfo * timer_info );
		virtual void setHotKey( int vk, int mod, PyObject * func );
		virtual void killHotKey( PyObject * func );
		virtual void setText( const wchar_t * text );
		virtual bool popupMenu( int x, int y, PyObject * items );
        virtual void enableIme( bool enable );
		virtual void setImeFont( FontBase * font );
		virtual void messageLoop( PyObject * continue_cond_func );
        
        int drawRect( CGRect rect, CGContextRef gctx );
        void beginPaint( CGContextRef gtxt );
        void endPaint();
        void paintBackground();
        void paintPlanes();
        void paintCaret();
        
        int windowShouldClose();
        int windowDidResize( CGSize size );
        int windowWillResize( CGSize * size );
        int windowDidBecomeKey();
        int windowDidResignKey();
        void callSizeHandler( Size size );
        void calculateFrameSize();
        CGRect calculateWindowRectFromPositionSizeOrigin( int x, int y, int width, int height, int origin );

        int timerHandler( CocoaObject * timer );
        
        int keyDown( int vk, int mod );
        int keyUp( int vk, int mod );
        int insertText( const wchar_t * text, int mod );
        int mouse( const ckit_MouseEvent * event );
        
        bool initialized;

        CocoaObject * handle;
        Size window_frame_size;
        
        bool initial_rect_set;
        CGRect initial_rect;
        
        CocoaObject * timer_paint;
        CocoaObject * timer_check_quit;
        
        std::vector<PyObject*> messageloop_continue_cond_func_stack;
        
        Color bg_color;
        Color caret_color0;
        Color caret_color1;
        
        CGContextRef paint_gctx;
        Size paint_client_size;
	};

	typedef ImageMac Image;
	typedef FontMac Font;
	typedef ImagePlaneMac ImagePlane;
	typedef TextPlaneMac TextPlane;
	typedef WindowMac Window;
};

#endif //__CKITCORE_MAC_H__
