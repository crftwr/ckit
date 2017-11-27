#ifndef __CKITCORE_H__
#define __CKITCORE_H__

#include <windows.h>
#include <wchar.h>
#include <vector>
#include <list>
#include <string>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

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
    	
    		fg_color = RGB(255,255,255);

    		bg_color[0] = RGB(0,0,0);
    		bg_color[1] = RGB(0,0,0);
    		bg_color[2] = RGB(0,0,0);
    		bg_color[3] = RGB(0,0,0);

    		line_color[0] = RGB(255,255,255);
    		line_color[1] = RGB(255,255,255);
    	}

		unsigned char bg;
		unsigned char line[2];
        COLORREF fg_color;
        COLORREF bg_color[4];
        COLORREF line_color[2];

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

    struct Image
    {
    	Image( int width, int height, const char * pixels, const COLORREF * transparent_color=0, bool halftone=false );
    	~Image();

    	void AddRef() { ref_count++; /* printf("Image::AddRef : %d\n", ref_count ); */ }
    	void Release() { ref_count--; /* printf("Image::Release : %d\n", ref_count ); */ if(ref_count==0) delete this; }

    	HBITMAP handle;
    	int width, height;
    	bool transparent;
    	bool halftone;
    	COLORREF transparent_color;

		int ref_count;
    };

    struct Font
    {
    	Font( const wchar_t * name, int height );
    	~Font();

    	void AddRef() { ref_count++; /* printf("Font::AddRef : %d\n", ref_count ); */ }
    	void Release() { ref_count--; /* printf("Font::Release : %d\n", ref_count ); */ if(ref_count==0) delete this; }

        LOGFONT logfont;
        HFONT handle;
        int char_width;
        int char_height;
	    std::vector<bool> zenkaku_table;

    	int ref_count;
    };

    struct Plane
    {
    	Plane( struct Window * window, int x, int y, int width, int height, float priority );
    	virtual ~Plane();

		void Show( bool show );
    	void SetPosition( int x, int y );
    	void SetSize( int width, int height );
    	void SetPriority( float priority );

		virtual void Draw( const RECT & paint_rect ) = 0;

		struct Window * window;
		bool show;
    	int x, y, width, height;
    	float priority;
    };

    struct ImagePlane : public Plane
    {
    	ImagePlane( struct Window * window, int x, int y, int width, int height, float priority );
    	virtual ~ImagePlane();

        void SetPyObject( PyObject * pyobj );

    	void SetImage( Image * image );

		virtual void Draw( const RECT & paint_rect );

		PyObject * pyobj;
    	Image * image;
    };

    struct TextPlane : public Plane
    {
    	TextPlane( struct Window * window, int x, int y, int width, int height, float priority );
    	virtual ~TextPlane();

        void SetPyObject( PyObject * pyobj );

    	void SetFont( Font * font );

		void PutString( int x, int y, int width, int height, const Attribute & attr, const wchar_t * str, int offset );
        int GetStringWidth( const wchar_t * str, int tab_width=4, int offset=0, int columns[]=NULL );
		void Scroll( int x, int y, int width, int height, int delta_x, int delta_y );

		void DrawOffscreen();
		void DrawHorizontalLine( int x1, int y1, int x2, COLORREF color, bool dotted );
		void DrawVerticalLine( int x1, int y1, int y2, COLORREF color, bool dotted );
        virtual void Draw( const RECT & paint_rect );
        void SetCaretPosition( int x, int y );

		PyObject * pyobj;
    	Font * font;
        std::vector<Line*> char_buffer;
		HDC	offscreen_dc;
		HBITMAP	offscreen_bmp;
		unsigned char * offscreen_buf;
		SIZE offscreen_size;
        bool dirty;
	};

    struct TimerInfo
    {
    	TimerInfo( PyObject * _pyobj ) : pyobj(_pyobj), calling(false) {}

    	PyObject * pyobj;
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
    	~MenuNode();

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

    struct Window
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
	        struct Window * parent_window;
	        HWND parent_window_hwnd;
	        COLORREF bg_color;
	        COLORREF frame_color;
	        COLORREF caret0_color;
	        COLORREF caret1_color;
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
	        COLORREF transparent_color;
	        bool is_top_most;
	        bool sysmenu;
	        bool tool;
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
	    };

        Window( Param & param );
        ~Window();

        void SetPyObject( PyObject * pyobj );

        HWND getHWND() const { return hwnd; }
		void show( bool show, bool activate );
        void enable( bool enable );
        void destroy();
        void activate();
        void inactivate();
        void foreground();
        void restore();
        void maximize();
        void minimize();
        void topmost( bool topmost );
        bool isEnabled();
        bool isVisible();
        bool isMaximized();
        bool isMinimized();
        bool isActive();
        bool isForeground();
        void getWindowRect( RECT * rect );
        void getNormalWindowRect( RECT * rect );
        void getNormalClientSize( SIZE * size );
        void clear();
        void setCaretRect( const RECT & rect );
        void setImeRect( const RECT & rect );
        void enableIme( bool enable );
		void setImeFont( const LOGFONT & logfont );
        void setPositionAndSize( int x, int y, int width, int height, int origin );
  		void appendDirtyRect( const RECT & rect );
		void clearDirtyRect();
  		void enumFonts( std::vector<std::wstring> * font_list );
  		void setBGColor( COLORREF color );
  		void setFrameColor( COLORREF color );
  		void setCaretColor( COLORREF color0, COLORREF color1 );
  		void setMenu( PyObject * menu );

		void _drawBackground( const RECT & paint_rect );
		void _drawPlanes( const RECT & paint_rect );
		void _drawCaret( const RECT & paint_rect );
        void _onNcPaint( HDC hDC );
        void _onSizing(DWORD side, LPRECT rc);
        void _onWindowPositionChange( WINDOWPOS * wndpos, bool send_event );
        void _updateWindowFrameRect();
        void _setImePosition();
        void flushPaint( HDC hDC=0, bool bitblt=true );
        void _onTimerCaretBlink();
        static LRESULT CALLBACK _wndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
        static int _getModKey();
	    static bool _registerWindowClass();
        bool _createWindow( Param & param );
        void setCapture();
        void releaseCapture();
		void setMouseCursor( int id );
        void drag( int x, int y );
        void _refreshMenu();
		void _buildMenu( HMENU menu_handle, PyObject * pymenu_node, int depth, bool parent_enabled );
	    void _clearCallables();
	    void _clearMenuCommands();
	    void _clearPopupMenuCommands();

		HWND hwnd;
		PyObject * pyobj;

	    bool quit_requested;
	    bool active;

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
        bool caret;
        int caret_blink;
        bool ime_on;
        std::list<Plane*> plane_list;
        RECT caret_rect;
        RECT ime_rect;
        bool dirty;
        RECT dirty_rect;
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
        ~TaskTrayIcon();

        void SetPyObject( PyObject * pyobj );
        void destroy();
		
        static LRESULT CALLBACK _wndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
	    static bool _registerWindowClass();
		void _addIconWithRetry();
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
	};
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
    ckit::Image * p;
};


extern PyTypeObject Font_Type;
#define Font_Check(op) PyObject_TypeCheck(op, &Font_Type)

struct Font_Object
{
    PyObject_HEAD
    ckit::Font * p;
};


extern PyTypeObject ImagePlane_Type;
#define ImagePlane_Check(op) PyObject_TypeCheck(op, &ImagePlane_Type)

struct ImagePlane_Object
{
    PyObject_HEAD
    ckit::ImagePlane * p;
};


extern PyTypeObject TextPlane_Type;
#define TextPlane_Check(op) PyObject_TypeCheck(op, &TextPlane_Type)

struct TextPlane_Object
{
    PyObject_HEAD
    ckit::TextPlane * p;
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
    ckit::Window * p;
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

    PyObject * reload_handler;
    uint64_t reload_pos;
    uint64_t reload_len;

    int flags;
};


#endif //__CKITCORE_H__
