#ifndef __CKITCORE_H__
#define __CKITCORE_H__

#include <wchar.h>
#include <vector>
#include <list>
#include <string>

#if defined(_DEBUG)
#undef _DEBUG
#include "python.h"
#define _DEBUG
#else
#include "python.h"
#endif

#include "basictypes.h"

namespace ckit
{
    struct Attribute
    {
    	enum
    	{
    		BG_Flat      = 1<<0,
    		BG_Gradation = 1<<1,
    	};
    	
    	enum
    	{
    		Line_Left   = 1<<0,
    		Line_Right  = 1<<1,
    		Line_Top    = 1<<2,
    		Line_Bottom = 1<<3,
    		Line_Dot    = 1<<4,
    	};

    	Attribute()
    		:
    		bg(0)
    	{
    		line[0] = 0;
    		line[1] = 0;
    	
    		fg_color = Color::FromRgb(255,255,255);

    		bg_color[0] = Color::FromRgb(0,0,0);
    		bg_color[1] = Color::FromRgb(0,0,0);
    		bg_color[2] = Color::FromRgb(0,0,0);
    		bg_color[3] = Color::FromRgb(0,0,0);

    		line_color[0] = Color::FromRgb(255,255,255);
    		line_color[1] = Color::FromRgb(255,255,255);
    	}

		unsigned char bg;
		unsigned char line[2];
        Color fg_color;
        Color bg_color[4];
        Color line_color[2];

		// 全ての要素を比較
        bool Equal( const Attribute & rhs ) const
        {
        	return (
        		bg==rhs.bg &&
        		line[0]==rhs.line[0] &&
        		line[1]==rhs.line[1] &&
        		fg_color==rhs.fg_color &&
        		bg_color[0]==rhs.bg_color[0] &&
        		bg_color[1]==rhs.bg_color[1] &&
        		bg_color[2]==rhs.bg_color[2] &&
        		bg_color[3]==rhs.bg_color[3] &&
        		line_color[0]==rhs.line_color[0] &&
        		line_color[1]==rhs.line_color[1] );
        }

		// fg_color を除く比較
        bool EqualWithoutFgColor( const Attribute & rhs ) const
        {
        	return (
        		bg==rhs.bg &&
        		line[0]==rhs.line[0] &&
        		line[1]==rhs.line[1] &&
        		bg_color[0]==rhs.bg_color[0] &&
        		bg_color[1]==rhs.bg_color[1] &&
        		bg_color[2]==rhs.bg_color[2] &&
        		bg_color[3]==rhs.bg_color[3] &&
        		line_color[0]==rhs.line_color[0] &&
        		line_color[1]==rhs.line_color[1] );
        }
	};

    struct Char
    {
    	Char( wchar_t _c )
    		:
    		c(_c),
			dirty(true)
    	{
    	}

    	Char( wchar_t _c, Attribute _attr )
    		:
    		c(_c),
    		attr(_attr),
			dirty(true)
    	{
    	}

        wchar_t c;
    	Attribute attr;
		bool dirty;

        bool operator==( const Char & rhs ) const
        {
			return ( c==rhs.c && attr.Equal(rhs.attr) );
        }

        bool operator!=( const Char & rhs ) const
        {
			return ( c!=rhs.c || !attr.Equal(rhs.attr) );
        }
    };

    struct Line : public std::vector<Char>
    {
    };

    struct ImageBase
    {
    	ImageBase( int width, int height, const char * pixels, const Color * transparent_color=0, bool halftone=false );
    	virtual ~ImageBase();

    	void AddRef() { ref_count++; /* printf("Image::AddRef : %d\n", ref_count ); */ }
    	void Release() { ref_count--; /* printf("Image::Release : %d\n", ref_count ); */ if(ref_count==0) delete this; }

    	int width, height;
    	bool transparent;
    	bool halftone;
    	Color transparent_color;

		int ref_count;
    };

    struct FontBase
    {
    	FontBase();
    	virtual ~FontBase();

    	void AddRef() { ref_count++; /* printf("Font::AddRef : %d\n", ref_count ); */ }
    	void Release() { ref_count--; /* printf("Font::Release : %d\n", ref_count ); */ if(ref_count==0) delete this; }

        int char_width;
        int char_height;
	    std::vector<bool> zenkaku_table;

    	int ref_count;
    };

    struct PlaneBase
    {
    	PlaneBase( struct WindowBase * window, int x, int y, int width, int height, float priority );
    	virtual ~PlaneBase();

		void Show( bool show );
    	void SetPosition( int x, int y );
    	void SetSize( int width, int height );
    	void SetPriority( float priority );

		virtual void Draw( const Rect & paint_rect ) = 0;

		struct WindowBase * window;
		bool show;
    	int x, y, width, height;
    	float priority;
    };

    struct ImagePlaneBase : public PlaneBase
    {
    	ImagePlaneBase( struct WindowBase * window, int x, int y, int width, int height, float priority );
    	virtual ~ImagePlaneBase();

        void SetPyObject( PyObject * pyobj );

    	void SetImage( ImageBase * image );

		PyObject * pyobj;
    	ImageBase * image;
    };

    struct TextPlaneBase : public PlaneBase
    {
    	TextPlaneBase( struct WindowBase * window, int x, int y, int width, int height, float priority );
    	virtual ~TextPlaneBase();

        void SetPyObject( PyObject * pyobj );

    	void SetFont( FontBase * font );

		void PutString( int x, int y, int width, int height, const Attribute & attr, const wchar_t * str, int offset );
        int GetStringWidth( const wchar_t * str, int tab_width=4, int offset=0, int columns[]=NULL );
		
		virtual void Scroll( int x, int y, int width, int height, int delta_x, int delta_y ) = 0;
		virtual void DrawOffscreen() = 0;
		void SetCaretPosition( int x, int y );

		PyObject * pyobj;
    	FontBase * font;
        std::vector<Line*> char_buffer;
        bool dirty;
	};

    struct TimerInfo
    {
    	TimerInfo() : pyobj(NULL), interval(0), handle(NULL), calling(false) {}

    	PyObject * pyobj;
        int interval;
        void * handle;
    	bool calling;
    };

    struct DelayedCallInfo
    {
    	DelayedCallInfo( PyObject * _pyobj, int _timeout ) : pyobj(_pyobj), timeout(_timeout), calling(false) {}

    	PyObject * pyobj;
    	int timeout;
    	bool calling;
    };

    struct HotKeyInfo
    {
    	HotKeyInfo( PyObject * _pyobj, int _id ) : pyobj(_pyobj), id(_id), calling(false) {}

    	PyObject * pyobj;
    	int id;
    	bool calling;
    };

    struct MenuNode
    {
	    struct Param
	    {
	        Param();
	        
	        std::wstring name;
	        std::wstring text;
		    PyObject * command;
		    PyObject * visible;
		    PyObject * enabled;
		    PyObject * checked;
		    PyObject * items;
        	bool separator;
	    };

    	MenuNode( Param & param );
    	virtual ~MenuNode();

    	void AddRef() { ref_count++; /* printf("MenuNode::AddRef : %d\n", ref_count ); */ }
    	void Release() { ref_count--; /* printf("MenuNode::Release : %d\n", ref_count ); */ if(ref_count==0) delete this; }

    	int ref_count;

        std::wstring name;
        std::wstring text;
	    PyObject * command;
	    PyObject * visible;
	    PyObject * enabled;
	    PyObject * checked;
	    PyObject * items;
        bool separator;
    };

    struct WindowBase
    {
	    struct Param
	    {
	        Param();

	        bool is_winpos_given;
	        int winpos_x;
	        int winpos_y;
	        int winsize_w;
	        int winsize_h;
	        int origin;
	        struct WindowBase * parent_window;
	        WindowHandle parent_window_hwnd;
	        Color bg_color;
	        Color frame_color;
	        Color caret0_color;
	        Color caret1_color;
	        bool title_bar;
	        std::wstring title;
	        bool show;
	        bool resizable;
	        bool noframe;
	        bool minimizebox;
	        bool maximizebox;
	        bool caret;
	        int border_size;
	        bool is_transparency_given;
	        int transparency;
	        bool is_transparent_color_given;
	        Color transparent_color;
	        bool is_top_most;
	        bool sysmenu;
	        bool tool;
	        bool ncpaint;

		    PyObject * activate_handler;
		    PyObject * close_handler;
		    PyObject * endsession_handler;          // FIXME : Mac対応
		    PyObject * move_handler;                // FIXME : Mac対応
		    PyObject * sizing_handler;
		    PyObject * size_handler;
		    PyObject * dropfiles_handler;           // FIXME : Mac対応
		    PyObject * ipc_handler;                 // FIXME : Mac対応
		    PyObject * keydown_handler;
		    PyObject * keyup_handler;
		    PyObject * char_handler;
		    PyObject * lbuttondown_handler;
		    PyObject * lbuttonup_handler;
		    PyObject * mbuttondown_handler;
		    PyObject * mbuttonup_handler;
		    PyObject * rbuttondown_handler;
		    PyObject * rbuttonup_handler;
		    PyObject * lbuttondoubleclick_handler;
		    PyObject * mbuttondoubleclick_handler;
		    PyObject * rbuttondoubleclick_handler;
		    PyObject * mousemove_handler;
		    PyObject * mousewheel_handler;          // FIXME : Mac対応
		    PyObject * nchittest_handler;           // FIXME : Mac対応
	    };

        WindowBase( Param & param );
        virtual ~WindowBase();

        void SetPyObject( PyObject * pyobj );

  		void appendDirtyRect( const Rect & rect );
		void clearDirtyRect();

        virtual WindowHandle getHandle() const = 0;
        virtual void flushPaint() = 0;
  		virtual void enumFonts( std::vector<std::wstring> * font_list ) = 0;
  		virtual void setBGColor( Color color ) = 0;
  		virtual void setFrameColor( Color color ) = 0;
  		virtual void setCaretColor( Color color0, Color color1 ) = 0;
  		virtual void setMenu( PyObject * menu ) = 0;
        virtual void setPositionAndSize( int x, int y, int width, int height, int origin ) = 0;
        virtual void setCapture() = 0;
        virtual void releaseCapture() = 0;
		virtual void setMouseCursor( int id ) = 0;
		virtual void drag( int x, int y ) = 0;
		virtual void show( bool show, bool activate ) = 0;
        virtual void enable( bool enable ) = 0;
        virtual void activate() = 0;
        virtual void inactivate() = 0;
        virtual void foreground() = 0;
        virtual void restore() = 0;
        virtual void maximize() = 0;
        virtual void minimize() = 0;
        virtual void topmost( bool topmost ) = 0;
        virtual bool isEnabled() = 0;
        virtual bool isVisible() = 0;
        virtual bool isMaximized() = 0;
        virtual bool isMinimized() = 0;
        virtual bool isActive() = 0;
        virtual bool isForeground() = 0;
        virtual void getWindowRect( Rect * rect ) = 0;
        virtual void getClientSize( Size * rect ) = 0;
        virtual void getNormalWindowRect( Rect * rect ) = 0;
        virtual void getNormalClientSize( Size * size ) = 0;
		virtual void clientToScreen(Point * point) = 0;
		virtual void screenToClient(Point * point) = 0;
		virtual void setTimer( TimerInfo * timer_info ) = 0;
		virtual void killTimer( TimerInfo * timer_info ) = 0;
		virtual void setHotKey( int vk, int mod, PyObject * func ) = 0;
		virtual void killHotKey( PyObject * func ) = 0;
		virtual void setText( const wchar_t * text ) = 0;
		virtual bool popupMenu( int x, int y, PyObject * items ) = 0;
		virtual void enableIme( bool enable ) = 0;
		virtual void setImeFont( FontBase * font ) = 0;
		virtual void messageLoop( PyObject * continue_cond_func ) = 0;

        void clear();
        void clearPlanes();

		void setCaretRect( const Rect & rect );
        void setImeRect( const Rect & rect );

	    void clearCallables();
	    void clearMenuCommands();
	    void clearPopupMenuCommands();

	public:
		PyObject * pyobj;

	    bool quit_requested;
	    bool active;

        bool caret;
        int caret_blink;
        bool ime_on;
        std::list<PlaneBase*> plane_list;
        Rect caret_rect;
        Rect ime_rect;
        bool dirty;
        Rect dirty_rect;
		int perf_fillrect_count;
		int perf_drawtext_count;
		int perf_drawplane_count;
        bool ncpaint;

	    PyObject * activate_handler;
	    PyObject * close_handler;
	    PyObject * endsession_handler;
	    PyObject * move_handler;
	    PyObject * sizing_handler;
	    PyObject * size_handler;
	    PyObject * dropfiles_handler;
	    PyObject * ipc_handler;
	    PyObject * keydown_handler;
	    PyObject * keyup_handler;
	    PyObject * char_handler;
	    PyObject * lbuttondown_handler;
	    PyObject * lbuttonup_handler;
	    PyObject * mbuttondown_handler;
	    PyObject * mbuttonup_handler;
	    PyObject * rbuttondown_handler;
	    PyObject * rbuttonup_handler;
	    PyObject * lbuttondoubleclick_handler;
	    PyObject * mbuttondoubleclick_handler;
	    PyObject * rbuttondoubleclick_handler;
	    PyObject * mousemove_handler;
	    PyObject * mousewheel_handler;
	    PyObject * nchittest_handler;

	    std::list<TimerInfo> timer_list;
	    int timer_list_ref_count;

	    std::list<DelayedCallInfo> delayed_call_list;
	    int delayed_call_list_ref_count;

	    std::list<HotKeyInfo> hotkey_list;
	    int hotkey_list_ref_count;

        PyObject * menu; 							// メニューバー
	    std::vector<PyObject*> menu_commands;		// メニューバー用のコマンド
	    std::vector<PyObject*> popup_menu_commands; // popupメニュー用のコマンド
	};

    struct TaskTrayIcon
    {
        #if defined(PLATFORM_WIN32)
	    struct Param
	    {
	        Param();

	        std::wstring title;
		    PyObject * lbuttondown_handler;
		    PyObject * lbuttonup_handler;
		    PyObject * rbuttondown_handler;
		    PyObject * rbuttonup_handler;
		    PyObject * lbuttondoubleclick_handler;
	    };

        TaskTrayIcon( Param & param );
        virtual ~TaskTrayIcon();

        void SetPyObject( PyObject * pyobj );
        void destroy();
        
        static LRESULT CALLBACK _wndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
	    static bool _registerWindowClass();
	    void _clearPopupMenuCommands();
	
		HWND hwnd;
		PyObject * pyobj;

		NOTIFYICONDATA icon_data;

	    PyObject * lbuttondown_handler;
	    PyObject * lbuttonup_handler;
	    PyObject * rbuttondown_handler;
	    PyObject * rbuttonup_handler;
	    PyObject * lbuttondoubleclick_handler;
	    
	    std::vector<PyObject*> popup_menu_commands;	// popupメニュー用のコマンド
        #endif // PLATFORM
	};

	struct Globals
	{
		PyObject * Error;
		PyObject * command_info_constructor;
	};

	extern Globals g;
};

//-------------------------------------------------------------------

extern PyTypeObject Attribute_Type;
#define Attribute_Check(op) PyObject_TypeCheck(op, &Attribute_Type)

struct Attribute_Object
{
    PyObject_HEAD
    ckit::Attribute attr;
};


extern PyTypeObject Image_Type;
#define Image_Check(op) PyObject_TypeCheck(op, &Image_Type)

struct Image_Object
{
    PyObject_HEAD
    ckit::ImageBase * p;
};


extern PyTypeObject Font_Type;
#define Font_Check(op) PyObject_TypeCheck(op, &Font_Type)

struct Font_Object
{
    PyObject_HEAD
    ckit::FontBase * p;
};


extern PyTypeObject ImagePlane_Type;
#define ImagePlane_Check(op) PyObject_TypeCheck(op, &ImagePlane_Type)

struct ImagePlane_Object
{
    PyObject_HEAD
    ckit::ImagePlaneBase * p;
};


extern PyTypeObject TextPlane_Type;
#define TextPlane_Check(op) PyObject_TypeCheck(op, &TextPlane_Type)

struct TextPlane_Object
{
    PyObject_HEAD
    ckit::TextPlaneBase * p;
};


extern PyTypeObject MenuNode_Type;
#define MenuNode_Check(op) PyObject_TypeCheck(op, &MenuNode_Type)

struct MenuNode_Object
{
    PyObject_HEAD
    ckit::MenuNode * p;
};


extern PyTypeObject Window_Type;
#define Window_Check(op) PyObject_TypeCheck(op, &Window_Type)

struct Window_Object
{
    PyObject_HEAD
    ckit::WindowBase * p;
};


extern PyTypeObject TaskTrayIcon_Type;
#define TaskTrayIcon_Check(op) PyObject_TypeCheck(op, &TaskTrayIcon_Type)

struct TaskTrayIcon_Object
{
    PyObject_HEAD
    ckit::TaskTrayIcon * p;
};


extern PyTypeObject Line_Type;
#define Line_Check(op) PyObject_TypeCheck(op, &Line_Type)

struct Line_Object
{
    PyObject_HEAD
    
    PyObject * s;
    PyObject * ctx;
    PyObject * tokens;
    int flags;
};


#endif //__CKITCORE_H__
