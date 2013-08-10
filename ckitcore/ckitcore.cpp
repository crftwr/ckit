#include <vector>
#include <algorithm>

#include <windows.h>

#if defined(_DEBUG)
#undef _DEBUG
#include "python.h"
#define _DEBUG
#else
#include "python.h"
#endif

#include "structmember.h"
#include "frameobject.h"

#include "pythonutil.h"
#include "ckitcore.h"

using namespace ckit;

//-----------------------------------------------------------------------------

#define MODULE_NAME "ckitcore"

static std::wstring WINDOW_CLASS_NAME 			= L"CkitWindowClass";
static std::wstring TASKTRAY_WINDOW_CLASS_NAME  = L"CkitTaskTrayWindowClass";

static PyObject * Error;
static PyObject * command_info_constructor;

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

const int WM_USER_NTFYICON  = WM_USER + 100;
const int ID_MENUITEM       = 256;
const int ID_MENUITEM_MAX   = 256+1024-1;
const int ID_POPUP_MENUITEM       = ID_MENUITEM_MAX+1;
const int ID_POPUP_MENUITEM_MAX   = ID_POPUP_MENUITEM+1024;

const int TIMER_PAINT		   			= 0x101;
const int TIMER_PAINT_INTERVAL 			= 10;
const int TIMER_CURSOR_BLINK   			= 0x102;

const int GLOBAL_OPTION_WALKAROUND_KB436093 = 0x101; // http://support.microsoft.com/kb/436093/ja

static bool walkaround_kb436093 = false;

//-----------------------------------------------------------------------------

//#define PRINTF printf
#define PRINTF(...)

//#define TRACE printf("%s(%d) : %s\n",__FILE__,__LINE__,__FUNCTION__)
#define TRACE

#if 0
	struct FuncTrace
	{
		FuncTrace( const char * _funcname, unsigned int _lineno )
		{
			funcname = _funcname;
			lineno   = _lineno;

			printf( "FuncTrace : Enter : %s(%)\n", funcname, lineno );
		}

		~FuncTrace()
		{
			printf( "FuncTrace : Leave : %s(%)\n", funcname, lineno );
		}

		const char * funcname;
		unsigned int lineno;
	};
	#define FUNC_TRACE FuncTrace functrace(__FUNCTION__,__LINE__)
#else
	#define FUNC_TRACE
#endif

#if 0
	#define PERF_BEGIN(tag,line) \
		__int64 tick_prev, tick; \
		QueryPerformanceCounter( (LARGE_INTEGER*)&tick_prev ); \
		printf( "%s:%d: -------\n", tag, line );
	
	#define PERF_PRINT(tag,line) \
		QueryPerformanceCounter( (LARGE_INTEGER*)&tick ); \
		printf( "%s:%d: %I64d\n", tag, line, tick-tick_prev ); \
		tick_prev = tick;
#else
	#define PERF_BEGIN(tag,line)
	#define PERF_PRINT(tag,line)
#endif

//-----------------------------------------------------------------------------

Image::Image( int _width, int _height, const char * pixels, const COLORREF * _transparent_color, bool _halftone )
	:
	width(_width),
	height(_height),
	transparent(_transparent_color!=0),
	halftone(_halftone),
	transparent_color( _transparent_color ? *_transparent_color : 0 ),
	ref_count(0)
{
	FUNC_TRACE;

	HDC hDC = GetDC(NULL);

	handle = CreateCompatibleBitmap( hDC, width, height );

	BITMAPINFO info = {0};
	info.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth    = width;
	info.bmiHeader.biHeight   = height;
	info.bmiHeader.biPlanes   = 1;
	info.bmiHeader.biBitCount = 32;

	if(pixels)
	{
		SetDIBits( NULL, handle, 0, height, pixels, &info, DIB_RGB_COLORS );
	}

    ReleaseDC( NULL, hDC );
}

Image::~Image()
{
	FUNC_TRACE;

	DeleteObject(handle);
}

//-----------------------------------------------------------------------------

Icon::Icon( const wchar_t * filename )
	:
	ref_count(0)
{
	FUNC_TRACE;

	SHFILEINFO shfi;

	::SHGetFileInfo( filename, 0, &shfi, sizeof(SHFILEINFO), SHGFI_ICON|SHGFI_SMALLICON );

	handle = shfi.hIcon;
}

Icon::~Icon()
{
	FUNC_TRACE;

	DestroyIcon(handle);
}

//-----------------------------------------------------------------------------

Plane::Plane( Window * _window, int _x, int _y, int _width, int _height, float _priority )
	:
	window(_window),
	pyobj(NULL),
	show(true),
	x(_x),
	y(_y),
	width(_width),
	height(_height),
	priority(_priority),
	image(NULL),
	icon(NULL)
{
	FUNC_TRACE;
}

Plane::~Plane()
{
	FUNC_TRACE;

	if(image) image->Release();
	if(icon) icon->Release();

	((Plane_Object*)pyobj)->p = NULL;
	Py_XDECREF(pyobj); pyobj=NULL;

	RECT dirty_rect = { x, y, x+width, y+height };
	window->appendDirtyRect( dirty_rect );
}

void Plane::SetPyObject( PyObject * _pyobj )
{
	FUNC_TRACE;

	if( pyobj != _pyobj )
	{
		Py_XDECREF(pyobj);
		pyobj = _pyobj;
		Py_XINCREF(pyobj);
	}
}

void Plane::Show( bool _show )
{
	FUNC_TRACE;

	if( show == _show ) return;

	show = _show;

	RECT dirty_rect = { x, y, x+width, y+height };
	window->appendDirtyRect( dirty_rect );
}

void Plane::SetPos( int _x, int _y )
{
	FUNC_TRACE;

	if( x==_x && y==_y ) return;

	RECT dirty_rect = { x, y, x+width, y+height };
	window->appendDirtyRect( dirty_rect );

	x = _x;
	y = _y;

	RECT dirty_rect2 = { x, y, x+width, y+height };
	window->appendDirtyRect( dirty_rect2 );
}

void Plane::SetSize( int _width, int _height )
{
	FUNC_TRACE;

	if( width==_width && height==_height ) return;

	RECT dirty_rect = { x, y, x+width, y+height };
	window->appendDirtyRect( dirty_rect );

	width = _width;
	height = _height;

	RECT dirty_rect2 = { x, y, x+width, y+height };
	window->appendDirtyRect( dirty_rect2 );
}

void Plane::SetPriority( float _priority )
{
	FUNC_TRACE;

	if( priority==_priority ) return;

	priority = _priority;

	RECT dirty_rect = { x, y, x+width, y+height };
	window->appendDirtyRect( dirty_rect );
}

void Plane::SetImage( Image * _image )
{
	FUNC_TRACE;

	if( image == _image ) return;

	if(image) image->Release();
	if(icon) icon->Release();

	image = _image;
	icon = NULL;

	if(image) image->AddRef();

	RECT dirty_rect = { x, y, x+width, y+height };
	window->appendDirtyRect( dirty_rect );
}

void Plane::SetIcon( Icon * _icon )
{
	FUNC_TRACE;

	if( icon == _icon ) return;

	if(image) image->Release();
	if(icon) icon->Release();

	image = NULL;
	icon = _icon;

	if(icon) icon->AddRef();

	RECT dirty_rect = { x, y, x+width, y+height };
	window->appendDirtyRect( dirty_rect );
}

//-----------------------------------------------------------------------------

MenuNode::Param::Param()
	:
    command(NULL),
    visible(NULL),
    enabled(NULL),
    checked(NULL),
    items(NULL),
    separator(false)
{
	FUNC_TRACE;
}

MenuNode::MenuNode( Param & param )
	:
	ref_count(0),
	separator(false)
{
	FUNC_TRACE;
	
	name = param.name;
	text = param.text;
	
	command = param.command;
	Py_XINCREF(command);

	visible = param.visible;
	Py_XINCREF(visible);

	enabled = param.enabled;
	Py_XINCREF(enabled);

	checked = param.checked;
	Py_XINCREF(checked);

	items = param.items;
	Py_XINCREF(items);

	separator = param.separator;
}

MenuNode::~MenuNode()
{
	FUNC_TRACE;

    Py_XDECREF(command); command=NULL;
    Py_XDECREF(visible); visible=NULL;
    Py_XDECREF(enabled); enabled=NULL;
    Py_XDECREF(checked); checked=NULL;
    Py_XDECREF(items); items=NULL;
}

//-----------------------------------------------------------------------------

Window::Param::Param()
{
	FUNC_TRACE;

    is_winpos_given = false;
    winpos_x = 0;
    winpos_y = 0;
    winsize_char_w = 80;
    winsize_char_h = 24;
    origin = 0;
    parent_window = NULL;
    parent_window_hwnd = NULL;
    font_size = 12;
    bg_color = RGB(0x01, 0x01, 0x01);
    frame_color = RGB(0xff, 0xff, 0xff);
    cursor0_color = RGB(0xff, 0xff, 0xff);
    cursor1_color = RGB(0xff, 0x00, 0x00);
    title_bar = true;
    show = true;
    resizable = true;
    noframe = false;
    minimizebox = true;
    maximizebox = true;
    cursor = false;
    border_size = 1;
    is_transparency_given = false;
    transparency = 255;
    is_transparent_color_given = false;
    transparent_color = 0;
    is_top_most = false;
    sysmenu = false;
    tool = false;
    ncpaint = false;

    activate_handler = NULL;
    close_handler = NULL;
    endsession_handler = NULL;
    move_handler = NULL;
    size_handler = NULL;
    dropfiles_handler = NULL;
    ipc_handler = NULL;
    keydown_handler = NULL;
    keyup_handler = NULL;
    char_handler = NULL;
    lbuttondown_handler = NULL;
    lbuttonup_handler = NULL;
    mbuttondown_handler = NULL;
    mbuttonup_handler = NULL;
    rbuttondown_handler = NULL;
    rbuttonup_handler = NULL;
	lbuttondoubleclick_handler = NULL;
	mbuttondoubleclick_handler = NULL;
	rbuttondoubleclick_handler = NULL;
    mousemove_handler = NULL;
    mousewheel_handler = NULL;
    nchittest_handler = NULL;
}

Window::Window( Param & param )
{
	FUNC_TRACE;

    hwnd = NULL;
    pyobj = NULL;
    quit_requested = false;
    active = false;
    memset( &logfont, 0, sizeof(logfont) );
    font_handle = NULL;
    font_handle_created = false;
    font_w = 0;
    font_h = 0;
    window_column = 0;
    window_row = 0;
    memset( &window_frame_rect, 0, sizeof(window_frame_rect) );
    memset( &last_valid_window_rect, 0, sizeof(last_valid_window_rect) );
	offscreen_dc = NULL;
	offscreen_bmp = NULL;
	offscreen_size.cx = 0;
	offscreen_size.cy = 0;
	bg_brush = NULL;
    frame_pen = NULL;
    cursor0_brush = NULL;
    cursor1_brush = NULL;
    border_size = param.border_size;
    border_left = border_size;
    border_top = border_size;
    cursor = param.cursor;
    cursor_blink = 1;
    ime_on = false;
    ime_context = (HIMC)NULL;
    menu = NULL;
	perf_drawtext_count = 0;
	perf_drawplane_count = 0;

    visible_rect.left = 0;
    visible_rect.right = param.winsize_char_w;
    visible_rect.top = 0;
    visible_rect.bottom = param.winsize_char_h;

    memset( &cursor_position, 0, sizeof(cursor_position) );
    memset( &ime_rect, 0, sizeof(ime_rect) );
    
    ncpaint = param.ncpaint;

    activate_handler = param.activate_handler; Py_XINCREF(activate_handler);
    close_handler = param.close_handler; Py_XINCREF(close_handler);
    endsession_handler = param.endsession_handler; Py_XINCREF(endsession_handler);
    move_handler = param.move_handler; Py_XINCREF(move_handler);
    size_handler = param.size_handler; Py_XINCREF(size_handler);
    dropfiles_handler = param.dropfiles_handler; Py_XINCREF(dropfiles_handler);
    ipc_handler = param.ipc_handler; Py_XINCREF(ipc_handler);
    keydown_handler = param.keydown_handler; Py_XINCREF(keydown_handler);
    keyup_handler = param.keyup_handler; Py_XINCREF(keyup_handler);
    char_handler = param.char_handler; Py_XINCREF(char_handler);
    lbuttondown_handler = param.lbuttondown_handler; Py_XINCREF(lbuttondown_handler);
    lbuttonup_handler = param.lbuttonup_handler; Py_XINCREF(lbuttonup_handler);
    mbuttondown_handler = param.mbuttondown_handler; Py_XINCREF(mbuttondown_handler);
    mbuttonup_handler = param.mbuttonup_handler; Py_XINCREF(mbuttonup_handler);
    rbuttondown_handler = param.rbuttondown_handler; Py_XINCREF(rbuttondown_handler);
    rbuttonup_handler = param.rbuttonup_handler; Py_XINCREF(rbuttonup_handler);
    lbuttondoubleclick_handler = param.lbuttondoubleclick_handler; Py_XINCREF(lbuttondoubleclick_handler);
    mbuttondoubleclick_handler = param.mbuttondoubleclick_handler; Py_XINCREF(mbuttondoubleclick_handler);
    rbuttondoubleclick_handler = param.rbuttondoubleclick_handler; Py_XINCREF(rbuttondoubleclick_handler);
    mousemove_handler = param.mousemove_handler; Py_XINCREF(mousemove_handler);
    mousewheel_handler = param.mousewheel_handler; Py_XINCREF(mousewheel_handler);
    nchittest_handler = param.nchittest_handler; Py_XINCREF(nchittest_handler);

	timer_list_ref_count = 0;
	delayed_call_list_ref_count = 0;
	hotkey_list_ref_count = 0;

    if(!bg_brush) bg_brush = CreateSolidBrush(param.bg_color);
    if(!frame_pen) frame_pen = CreatePen( PS_SOLID, 0, param.frame_color );
    if(!cursor0_brush) cursor0_brush = CreateSolidBrush(param.cursor0_color);
    if(!cursor1_brush) cursor1_brush = CreateSolidBrush(param.cursor1_color);

	if( param.parent_window && param.font_name.empty() )
	{
		if(!setFont(param.parent_window))
		{
	        printf("_createFont failed\n");
	        return;
		}
	}
    else if(!setFont(param.font_name.c_str(), param.font_size))
    {
        printf("_createFont failed\n");
        return;
    }

    if(! _createWindow(param))
    {
        printf("_createWindow failed\n");
        return;
    }

    _dirty = false;
}

Window::~Window()
{
	FUNC_TRACE;

	clear();

	((Window_Object*)pyobj)->p = NULL;
    Py_XDECREF(pyobj); pyobj=NULL;
    Py_XDECREF(activate_handler); activate_handler=NULL;
    Py_XDECREF(close_handler); close_handler=NULL;
    Py_XDECREF(endsession_handler); endsession_handler=NULL;
    Py_XDECREF(move_handler); move_handler=NULL;
    Py_XDECREF(size_handler); size_handler=NULL;
    Py_XDECREF(dropfiles_handler); dropfiles_handler=NULL;
    Py_XDECREF(ipc_handler); ipc_handler=NULL;
    Py_XDECREF(keydown_handler); keydown_handler=NULL;
    Py_XDECREF(keyup_handler); keyup_handler=NULL;
    Py_XDECREF(char_handler); char_handler=NULL;
    Py_XDECREF(lbuttondown_handler); lbuttondown_handler=NULL;
    Py_XDECREF(lbuttonup_handler); lbuttonup_handler=NULL;
    Py_XDECREF(mbuttondown_handler); mbuttondown_handler=NULL;
    Py_XDECREF(mbuttonup_handler); mbuttonup_handler=NULL;
    Py_XDECREF(rbuttondown_handler); rbuttondown_handler=NULL;
    Py_XDECREF(rbuttonup_handler); rbuttonup_handler=NULL;
    Py_XDECREF(lbuttondoubleclick_handler); lbuttondoubleclick_handler=NULL;
    Py_XDECREF(mbuttondoubleclick_handler); mbuttondoubleclick_handler=NULL;
    Py_XDECREF(rbuttondoubleclick_handler); rbuttondoubleclick_handler=NULL;
    Py_XDECREF(mousemove_handler); mousemove_handler=NULL;
    Py_XDECREF(mousewheel_handler); mousewheel_handler=NULL;

	for( std::list<TimerInfo>::const_iterator i=timer_list.begin(); i!=timer_list.end() ; i++ )
	{
		Py_XDECREF( i->pyobj );
	}
	timer_list.clear();

	for( std::list<DelayedCallInfo>::const_iterator i=delayed_call_list.begin(); i!=delayed_call_list.end() ; i++ )
	{
		Py_XDECREF( i->pyobj );
	}
	delayed_call_list.clear();

	for( std::list<HotKeyInfo>::const_iterator i=hotkey_list.begin(); i!=hotkey_list.end() ; i++ )
	{
		Py_XDECREF( i->pyobj );
	}
	hotkey_list.clear();

    if(font_handle && font_handle_created) { DeleteObject(font_handle); font_handle=NULL; font_handle_created=false; }
    if(bg_brush) { DeleteObject(bg_brush); bg_brush = NULL; }
    if(frame_pen) { DeleteObject(frame_pen); frame_pen = NULL; }
    if(cursor0_brush) { DeleteObject(cursor0_brush); cursor0_brush = NULL; }
    if(cursor1_brush) { DeleteObject(cursor1_brush); cursor1_brush = NULL; }
	if(offscreen_bmp) { DeleteObject(offscreen_bmp); offscreen_bmp = NULL; };
	if(offscreen_dc) { DeleteObject(offscreen_dc); offscreen_dc = NULL; };
}

void Window::SetPyObject( PyObject * _pyobj )
{
	FUNC_TRACE;

	if( pyobj != _pyobj )
	{
		Py_XDECREF(pyobj);
		pyobj = _pyobj;
		Py_XINCREF(pyobj);
	}
}

void Window::appendDirtyRect( const RECT & rect, bool dirty_text )
{
	FUNC_TRACE;

	// テキストレイヤの再描画マーク
	if(dirty_text)
	{
		setDirtyTextLayerRect(rect);
	}

	if(_dirty)
	{
		if( dirty_rect.left > rect.left ) dirty_rect.left = rect.left;
		if( dirty_rect.top > rect.top ) dirty_rect.top = rect.top;
		if( dirty_rect.right < rect.right ) dirty_rect.right = rect.right;
		if( dirty_rect.bottom < rect.bottom ) dirty_rect.bottom = rect.bottom;
	}
	else
	{
		dirty_rect = rect;
		_dirty = true;
	}
}

void Window::clearDirtyRect()
{
	if(_dirty)
	{
		/*
		printf( "clearDirtyRect\n" );
		printf( "  dirty_rect : %d, %d, %d, %d\n", dirty_rect.left, dirty_rect.top, dirty_rect.right, dirty_rect.bottom );
		printf( "  perf_drawtext_count : %d\n", perf_drawtext_count );
		printf( "  perf_drawplane_count : %d\n", perf_drawplane_count );
		*/

		perf_drawtext_count = 0;
		perf_drawplane_count = 0;

		_dirty = false;
	}
}

void Window::setDirtyTextLayerRect( const RECT & rect )
{
	RECT text_rect = {
		(rect.left-border_left) / font_w,
		(rect.top-border_top) / font_h,
		(rect.right-border_left) / font_w + 1,
		(rect.bottom-border_top) / font_h + 1,
	};
	
	if(text_rect.left<0){ text_rect.left=0; }
	if(text_rect.top<0){ text_rect.top=0; }

	for( int i=text_rect.top ; i<(int)char_buffer.size() && i<text_rect.bottom ; ++i )
	{
		for( int j=text_rect.left ; j<(int)(*char_buffer[i]).size() && j<text_rect.right ; ++j )
		{
			(*char_buffer[i])[j].dirty = true;
		}
	}
}

void Window::scroll( int x, int y, int width, int height, int delta_x, int delta_y )
{
	FUNC_TRACE;

	// まだ描いてないものがあれば描く
    flushPaint( NULL, false );

	// 埋まっていなかったら空文字で進める
	while( (int)char_buffer.size() <= y+height+delta_y )
	{
        Line * line = new Line();
        char_buffer.push_back(line);
	}

	// キャラクタバッファをコピー
	if(delta_y<0)
	{
		for( int i=0 ; i<height ; ++i )
		{
			*char_buffer[y+i+delta_y] = *char_buffer[y+i];
		}
	}
	else if(delta_y>0)
	{
		for( int i=height-1 ; i>=0 ; --i )
		{
			*char_buffer[y+i+delta_y] = *char_buffer[y+i];
		}
	}
	else
	{
		// 埋まっていなかったら空文字で進める
		for( int i=0 ; i<height ; ++i )
		{
		    Line * line = char_buffer[y+i];
			while( (int)line->size() < x+width+delta_x )
			{
		        line->push_back( Char( L' ' ) );
			}
		}

		if(delta_x<0)
		{
			for( int i=0 ; i<height ; ++i )
			{
				for( int j=0 ; j<width ; ++j )
				{
					(*char_buffer[y+i+delta_y])[x+j+delta_x] = (*char_buffer[y+i])[x+j];
				}
			}
		}
		else
		{
			for( int i=0 ; i<height ; ++i )
			{
				for( int j=width-1 ; j>=0 ; --j )
				{
					(*char_buffer[y+i+delta_y])[x+j+delta_x] = (*char_buffer[y+i])[x+j];
				}
			}
		}
	}

	RECT src_rect = { 
		x * font_w + border_left, 
		y * font_h + border_top, 
		(x+width) * font_w + border_left, 
		(y+height) * font_h + border_top };

	RECT dst_rect = { 
		(x+delta_x) * font_w + border_left, 
		(y+delta_y) * font_h + border_top, 
		(x+width+delta_x) * font_w + border_left, 
		(y+height+delta_y) * font_h + border_top };

	BOOL ret = ScrollDC( 
		offscreen_dc, 
		delta_x * font_w,
		delta_y * font_h,
		&src_rect,
		NULL, NULL, NULL );
	assert(ret);

	appendDirtyRect( dst_rect, false );
}

void Window::_drawBorder( HDC hDC, const RECT & paint_rect )
{
    RECT client_rect;
    GetClientRect(hwnd, &client_rect);

	RECT rect;
	
	// 上
	rect.left = 0;
	rect.top = 0;
	rect.right = client_rect.right;
	rect.bottom = border_top;

	if( RectVisible( hDC, &rect ) )
	{
		FillRect( hDC, &rect, bg_brush );
	}

	// 下
	rect.left = 0;
	rect.top = height() * font_h + border_top;
	rect.right = client_rect.right;
	rect.bottom = client_rect.bottom;

	if( RectVisible( hDC, &rect ) )
	{
		FillRect( hDC, &rect, bg_brush );
	}

	// 左
	rect.left = 0;
	rect.top = border_top;
	rect.right = border_left;
	rect.bottom = height() * font_h + border_top;

	if( RectVisible( hDC, &rect ) )
	{
		FillRect( hDC, &rect, bg_brush );
	}

	// 右
	rect.left = width() * font_w + border_left;
	rect.top = border_top;
	rect.right = client_rect.right;
	rect.bottom = height() * font_h + border_top;

	if( RectVisible( hDC, &rect ) )
	{
		FillRect( hDC, &rect, bg_brush );
	}
}

// テキストレイヤーの描画
void Window::_drawTextLayer( HDC hDC, const RECT & paint_rect, bool clear_bg, bool draw_text )
{
	FUNC_TRACE;

    wchar_t * work_text = new wchar_t[ width() ];
    int * work_width = new int [ width() ];
    unsigned int work_len;
	bool work_dirty;

    for( int y=visible_rect.top ; y<(int)char_buffer.size() && y<visible_rect.bottom ; ++y )
    {
    	if( (y+1)*font_h+border_top <= paint_rect.top ) continue;
    	if( y*font_h+border_top >= paint_rect.bottom ) break;

        Line & line = *(char_buffer[y]);

        for( int x=visible_rect.left ; x<(int)line.size() && x<visible_rect.right ; ++x )
        {
	    	if( x*font_w+border_left >= paint_rect.right ) break;

            Char & chr = line[x];

            work_len = 0;
			work_dirty = false;

            int x2;
            for( x2=x ; x2<(int)line.size() && x2<visible_rect.right ; ++x2 )
            {
                Char & chr2 = line[x2];

                if( chr2.c==0 )
                {
                    continue;
                }

				if( chr2.c!=' ' )
				{
					if( ! chr.attr.Equal(chr2.attr) )
					{
						break;
					}
				}
				else
				{
					if( ! chr.attr.EqualWithoutFgColor(chr2.attr) )
					{
						break;
					}
				}

                work_text[work_len] = chr2.c;
                work_width[work_len] = (!zenkaku_table[chr2.c]) ? font_w : font_w*2;
                work_len ++;

				if(chr2.dirty)
				{
					work_dirty = true;
					
					if(draw_text)
					{
						chr2.dirty = false;
					}
				}
            }

    		if( x2*font_w+border_left <= paint_rect.left ) continue;

			if(clear_bg && work_dirty)
			{
				if( chr.attr.bg==0 )
				{
					RECT rect = {
						(x-visible_rect.left) * font_w + border_left,
						(y-visible_rect.top) * font_h + border_top,
						(x2-visible_rect.left) * font_w + border_left,
                		(y-visible_rect.top) * font_h + font_h + border_top
					};

					FillRect( hDC, &rect, bg_brush );
				}
			}

			if(draw_text && work_dirty)
			{
				if( chr.attr.bg & Attribute::BG_Flat )
				{
					HBRUSH hBrush = CreateSolidBrush( chr.attr.bg_color[0] );
					HPEN hPen = CreatePen( PS_SOLID, 0, chr.attr.bg_color[0] );

            		SelectObject( hDC, hBrush );
            		SelectObject( hDC, hPen );

					Rectangle(
						hDC,
						(x-visible_rect.left) * font_w + border_left,
						(y-visible_rect.top) * font_h + border_top,
						(x2-visible_rect.left) * font_w + border_left,
                		(y-visible_rect.top) * font_h + font_h + border_top
						);

					DeleteObject(hBrush);
					DeleteObject(hPen);
				}
				else if( chr.attr.bg & Attribute::BG_Gradation )
				{
					TRIVERTEX v[4];

					v[0].x = (x-visible_rect.left) * font_w + border_left;
					v[0].y = (y-visible_rect.top) * font_h + border_top;
					v[0].Red = GetRValue(chr.attr.bg_color[0])<<8;
					v[0].Green = GetGValue(chr.attr.bg_color[0])<<8;
					v[0].Blue = GetBValue(chr.attr.bg_color[0])<<8;
					v[0].Alpha = 255<<8;

					v[1].x = (x2-visible_rect.left) * font_w + border_left;
					v[1].y = (y-visible_rect.top) * font_h + border_top;
					v[1].Red = GetRValue(chr.attr.bg_color[1])<<8;
					v[1].Green = GetGValue(chr.attr.bg_color[1])<<8;
					v[1].Blue = GetBValue(chr.attr.bg_color[1])<<8;
					v[1].Alpha = 255<<8;

					v[2].x = (x-visible_rect.left) * font_w + border_left;
					v[2].y = (y-visible_rect.top) * font_h + font_h + border_top;
					v[2].Red = GetRValue(chr.attr.bg_color[2])<<8;
					v[2].Green = GetGValue(chr.attr.bg_color[2])<<8;
					v[2].Blue = GetBValue(chr.attr.bg_color[2])<<8;
					v[2].Alpha = 255<<8;

					v[3].x = (x2-visible_rect.left) * font_w + border_left;
					v[3].y = (y-visible_rect.top) * font_h + font_h + border_top;
					v[3].Red = GetRValue(chr.attr.bg_color[3])<<8;
					v[3].Green = GetGValue(chr.attr.bg_color[3])<<8;
					v[3].Blue = GetBValue(chr.attr.bg_color[3])<<8;
					v[3].Alpha = 255<<8;

					GRADIENT_TRIANGLE triangle[2];
					triangle[0].Vertex1 = 0;
					triangle[0].Vertex2 = 1;
					triangle[0].Vertex3 = 2;
					triangle[1].Vertex1 = 1;
					triangle[1].Vertex2 = 2;
					triangle[1].Vertex3 = 3;

					GradientFill(
						hDC,
						v,
						4,
						triangle,
						2,
						GRADIENT_FILL_TRIANGLE
					);
				}

	            SetTextColor( hDC, chr.attr.fg_color );
	            SetBkMode( hDC, TRANSPARENT );

	            ExtTextOut(
	                hDC,
	                (x-visible_rect.left) * font_w + border_left,
	                (y-visible_rect.top) * font_h + border_top,
	                0, NULL,
	                (LPCWSTR)work_text,
	                (UINT)(work_len),
	                work_width );

				perf_drawtext_count ++;

				for( int line=0 ; line<2 ; ++line )
				{
		            if( chr.attr.line[line] & (Attribute::Line_Left|Attribute::Line_Right|Attribute::Line_Top|Attribute::Line_Bottom) )
		            {
		            	HPEN hPen;
		            	if(chr.attr.line[line] & Attribute::Line_Dot)
		            	{
							LOGBRUSH log_brush;
							log_brush.lbColor = chr.attr.line_color[line];
							log_brush.lbHatch = 0;
							log_brush.lbStyle = BS_SOLID;
							DWORD pattern[] = { 1, 1 };
					    
						    hPen = ExtCreatePen( PS_GEOMETRIC | PS_ENDCAP_FLAT | PS_USERSTYLE, 1, &log_brush, 2, pattern );
		            	}
		            	else
		            	{
						    hPen = CreatePen( PS_SOLID, 1, chr.attr.line_color[line] );
		            	}

		            	SelectObject( hDC, hPen );

						if(chr.attr.line[line] & Attribute::Line_Left)
						{
				            MoveToEx(
				            	hDC,
				            	(x-visible_rect.left) * font_w + border_left,
			                	(y-visible_rect.top) * font_h + border_top,
			                	NULL
				            	);

				            LineTo(
				            	hDC,
				            	(x-visible_rect.left) * font_w + border_left,
			                	(y-visible_rect.top) * font_h + font_h + border_top
				            	);
						}

						if(chr.attr.line[line] & Attribute::Line_Bottom)
						{
				            MoveToEx(
				            	hDC,
				            	(x-visible_rect.left) * font_w + border_left,
			                	(y-visible_rect.top) * font_h + font_h-1 + border_top,
			                	NULL
				            	);

				            LineTo(
				            	hDC,
				            	(x2-visible_rect.left) * font_w + border_left,
			                	(y-visible_rect.top) * font_h + font_h-1 + border_top
				            	);
						}

						if(chr.attr.line[line] & Attribute::Line_Right)
						{
				            MoveToEx(
				            	hDC,
				            	(x2-visible_rect.left) * font_w-1 + border_left,
			                	(y-visible_rect.top) * font_h + border_top,
			                	NULL
				            	);

				            LineTo(
				            	hDC,
				            	(x2-visible_rect.left) * font_w-1 + border_left,
			                	(y-visible_rect.top) * font_h + font_h + border_top
				            	);
						}

						if(chr.attr.line[line] & Attribute::Line_Top)
						{
				            MoveToEx(
				            	hDC,
				            	(x-visible_rect.left) * font_w + border_left,
			                	(y-visible_rect.top) * font_h + border_top,
			                	NULL
				            	);

				            LineTo(
				            	hDC,
				            	(x2-visible_rect.left) * font_w + border_left,
			                	(y-visible_rect.top) * font_h + border_top
				            	);
						}

			            DeleteObject(hPen);
		            }
				}
			}
			
            x = x2 - 1;
        }
    }

    // カーソルの描画
    if( draw_text && cursor && cursor_blink )
    {
	    if(visible_rect.top      <= cursor_position.Y &&
	       visible_rect.bottom-1 >= cursor_position.Y &&
	       visible_rect.left     <= cursor_position.X &&
	       visible_rect.right-1  >= cursor_position.X)
	    {
            int pntX = (cursor_position.X - visible_rect.left) * font_w + border_left;
            int pntY = (cursor_position.Y - visible_rect.top) * font_h + border_top;
			
			if(ime_on)
			{
	            SelectObject( hDC, cursor1_brush ) ;
			}
			else
			{
	            SelectObject( hDC, cursor0_brush ) ;
			}

            PatBlt( hDC, pntX, pntY, 2, font_h, PATINVERT );
	    }
    }

    delete [] work_width;
    delete [] work_text;
}

// ビットマップレイヤーの描画
void Window::_drawBitmapLayer( HDC hDC, const RECT & paint_rect, bool draw_low_priority, bool draw_high_priority )
{
	FUNC_TRACE;

	std::list<Plane*>::const_iterator i;
    for( i=plane_list.begin() ; i!=plane_list.end() ; i++ )
    {
    	Plane * plane = *i;

    	if( draw_high_priority && plane->priority>=0 ) continue;
    	if( draw_low_priority && plane->priority<0 ) break;
    	if( !plane->show ) continue;

    	RECT plane_rect = { plane->x, plane->y, plane->x+plane->width, plane->y+plane->height };
   		if( plane_rect.left>=paint_rect.right || plane_rect.right<=paint_rect.left
   		 || plane_rect.top>=paint_rect.bottom || plane_rect.bottom<=paint_rect.top )
   		{
   			continue;
   		}

		// テキストより奥の Plane は上に乗っているテキストの再描画を促す
		if(draw_low_priority)
		{
			setDirtyTextLayerRect(plane_rect);
		}

   		if( plane->image )
   		{
			HDC compatibleDC = CreateCompatibleDC(hDC);

		    SelectObject( compatibleDC, plane->image->handle );

			/*
			printf( "StretchBlt : %d %d %d %d  %d %d %d %d\n",
				plane->x, plane->y, plane->width, plane->height,
				0, 0, plane->image->width, plane->image->height );
			*/

			if( plane->image->halftone )	
			{
				SetStretchBltMode( hDC, STRETCH_HALFTONE );
			}
			else
			{
				SetStretchBltMode( hDC, STRETCH_DELETESCANS );
			}

			if( plane->image->transparent )
			{
				/*
				BLENDFUNCTION bf;
				bf.BlendOp = AC_SRC_OVER;
				bf.BlendFlags = 0;
				bf.AlphaFormat = AC_SRC_ALPHA;
				bf.SourceConstantAlpha = 255;
				
				AlphaBlend(
					hDC, plane->x, plane->y, plane->width, plane->height,
					compatibleDC, 0, 0, plane->image->width, plane->image->height,
					bf
					);
				*/

				TransparentBlt(
					hDC, plane->x, plane->y, plane->width, plane->height,
					compatibleDC, 0, 0, plane->image->width, plane->image->height,
					plane->image->transparent_color
					);
				
				perf_drawplane_count ++;
			}
			else
			{
				StretchBlt(
					hDC, plane->x, plane->y, plane->width, plane->height,
					compatibleDC, 0, 0, plane->image->width, plane->image->height,
					SRCCOPY
					);

				perf_drawplane_count ++;
			}

	    	DeleteDC(compatibleDC);
   		}
   		else if( plane->icon )
   		{
   			DrawIconEx( hDC, plane->x, plane->y, plane->icon->handle, plane->width, plane->height, 0, NULL, DI_NORMAL );
   		}
    }
}

void Window::flushPaint( HDC hDC, bool bitblt )
{
    if(!_dirty){ return; }

	FUNC_TRACE;

	bool dc_need_release = false;
	if(hDC==NULL)
	{
		hDC = GetDC(hwnd);
		dc_need_release = true;
	}

    RECT client_rect;
    GetClientRect(hwnd, &client_rect);

	if( dirty_rect.left < client_rect.left ) dirty_rect.left = client_rect.left;
	if( dirty_rect.top < client_rect.top ) dirty_rect.top = client_rect.top;
	if( dirty_rect.right > client_rect.right ) dirty_rect.right = client_rect.right;
	if( dirty_rect.bottom > client_rect.bottom ) dirty_rect.bottom = client_rect.bottom;

	if( offscreen_dc==NULL || offscreen_bmp==NULL || offscreen_size.cx!=client_rect.right-client_rect.left || offscreen_size.cy!=client_rect.bottom-client_rect.top )
	{
		if(offscreen_dc){ DeleteObject(offscreen_dc); }
		if(offscreen_bmp){ DeleteObject(offscreen_bmp); }
		offscreen_dc = CreateCompatibleDC(hDC);
		offscreen_bmp = CreateCompatibleBitmap(hDC, client_rect.right-client_rect.left, client_rect.bottom-client_rect.top);
		offscreen_size.cx = client_rect.right-client_rect.left;
		offscreen_size.cy = client_rect.bottom-client_rect.top;
		SelectObject(offscreen_dc, offscreen_bmp);
		
		// オフスクリーンを作った直後は全て描く
		dirty_rect = client_rect;
		setDirtyTextLayerRect(dirty_rect);
		FillRect( offscreen_dc, &dirty_rect, bg_brush );
	}

	{
	    IntersectClipRect( offscreen_dc, dirty_rect.left, dirty_rect.top, dirty_rect.right, dirty_rect.bottom );

		SelectObject(offscreen_dc, font_handle);

		_drawBorder( offscreen_dc, dirty_rect );
		_drawTextLayer( offscreen_dc, dirty_rect, true, false );
		_drawBitmapLayer( offscreen_dc, dirty_rect, true, false );
		_drawTextLayer( offscreen_dc, dirty_rect, false, true );
		_drawBitmapLayer( offscreen_dc, dirty_rect, false, true );

		SelectClipRgn( offscreen_dc, NULL );
	}

	if(bitblt)
	{
	    BOOL ret = BitBlt( hDC, dirty_rect.left, dirty_rect.top, dirty_rect.right-dirty_rect.left, dirty_rect.bottom-dirty_rect.top, offscreen_dc, dirty_rect.left, dirty_rect.top, SRCCOPY );
		assert(ret);
	}

	if(dc_need_release)
	{
		ReleaseDC( hwnd, hDC );
	}

	clearDirtyRect();

	if(ime_on)
    {
        _setImePosition();
    }

	int w = width();
    int h = height();

    if(window_column != w || window_row != h)
    {
        w = (w * font_w) + (border_size * 2) + (window_frame_rect.right - window_frame_rect.left);
        h = (h * font_h) + (border_size * 2) + (window_frame_rect.bottom - window_frame_rect.top);
        SetWindowPos( hwnd, NULL, 0,0,w,h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE );
    }
}

void Window::_onNcPaint( HDC hDC )
{
	RECT window_rect;
	GetWindowRect(hwnd, &window_rect);

	RECT client_rect_local;
	GetClientRect(hwnd, &client_rect_local);
	
	POINT client_topleft = { 0, 0 };
	POINT client_bottomright = { client_rect_local.right, client_rect_local.bottom };
	ClientToScreen( hwnd, &client_topleft );
	ClientToScreen( hwnd, &client_bottomright );
	
	RECT client_rect = { client_topleft.x, client_topleft.y, client_bottomright.x, client_bottomright.y };
	
	SelectObject( hDC, bg_brush );
	SelectObject( hDC, frame_pen );

	// 上
	RECT rect1 = { 
		0, 
		0,
		window_rect.right-window_rect.left,
		client_rect.top-window_rect.top
		};
	FillRect( hDC, &rect1, bg_brush );

	// 左
	RECT rect2 = { 
		0, 
		client_rect.top-window_rect.top, 
		client_rect.left-window_rect.left, 
		client_rect.bottom-window_rect.top
		};
	FillRect( hDC, &rect2, bg_brush );

	// 右
	RECT rect3 = { 
		client_rect.right-window_rect.left,
		client_rect.top-window_rect.top, 
		window_rect.right-window_rect.left, 
		client_rect.bottom-window_rect.top 
		};
	FillRect( hDC, &rect3, bg_brush );

	// 下
	RECT rect4 = { 
		0,
		client_rect.bottom-window_rect.top, 
		window_rect.right-window_rect.left, 
		window_rect.bottom-window_rect.top 
		};
	FillRect( hDC, &rect4, bg_brush );
	
	// 枠線
	MoveToEx( hDC, 0, 0, NULL );
	LineTo( hDC, window_rect.right-window_rect.left-1, 0 );
	LineTo( hDC, window_rect.right-window_rect.left-1, window_rect.bottom-window_rect.top-1 );
	LineTo( hDC, 0, window_rect.bottom-window_rect.top-1 );
	LineTo( hDC, 0, 0 );
}

void Window::_setConsoleWindowSize( LONG cols, LONG rows )
{
	FUNC_TRACE;

    window_column = cols;
    window_row = rows;

    if( cols==width()
     && rows==height() )
        return;

    visible_rect.right = cols;
    visible_rect.bottom += ( rows - height() );
}

void Window::_onSizing( DWORD side, LPRECT rc)
{
	FUNC_TRACE;
	
    LONG fw = (window_frame_rect.right - window_frame_rect.left) + (border_size * 2);
    LONG fh = (window_frame_rect.bottom - window_frame_rect.top) + (border_size * 2);
    LONG width  = rc->right - rc->left;
    LONG height = rc->bottom - rc->top;

    width  -= fw;
    width  -= width % font_w;
    width  += fw;

    height -= fh;
    height -= height % font_h;
    height += fh;

    if(side==WMSZ_LEFT || side==WMSZ_TOPLEFT || side==WMSZ_BOTTOMLEFT)
    {
        rc->left = rc->right - width;
    }
    else if(side==WMSZ_RIGHT || side==WMSZ_TOPRIGHT || side==WMSZ_BOTTOMRIGHT)
    {
        rc->right = rc->left + width;
    }

    if(side==WMSZ_TOP || side==WMSZ_TOPLEFT || side==WMSZ_TOPRIGHT)
    {
        rc->top = rc->bottom - height;
    }
    else if(side==WMSZ_BOTTOM || side==WMSZ_BOTTOMLEFT || side==WMSZ_BOTTOMRIGHT)
    {
        rc->bottom = rc->top + height;
    }
}

void Window::_onWindowPosChange( WINDOWPOS * wndpos, bool send_event )
{
	FUNC_TRACE;
	
	if(!IsIconic(hwnd))
	{
		::GetWindowRect(hwnd,&last_valid_window_rect);
	
	    if(!(wndpos->flags & SWP_NOMOVE))
	    {
	        if( send_event && move_handler )
			{
				PyObject * pyarglist = Py_BuildValue("(ii)", wndpos->x, wndpos->y );
				PyObject * pyresult = PyEval_CallObject( move_handler, pyarglist );
				Py_DECREF(pyarglist);
				if(pyresult)
				{
					Py_DECREF(pyresult);
				}
				else
				{
					PyErr_Print();
				}
			}
	    }

	    if(!(wndpos->flags & SWP_NOSIZE))
	    {
	    	_updateWindowFrameRect();

	        LONG fw = (window_frame_rect.right - window_frame_rect.left) + (border_size * 2);
	        LONG fh = (window_frame_rect.bottom - window_frame_rect.top) + (border_size * 2);
	        LONG width  = wndpos->cx;
	        LONG height = wndpos->cy;

	        width  = width - fw;
	        height = height - fh;

	        if(width<0) width=0;
	        if(height<0) height=0;

	        width  = width / font_w;
	        height = height / font_h;

	        _setConsoleWindowSize(width, height);

	    	// ウインドウを最大化したときのボーダーを上下と左右で均等に調整
		    RECT client_rect;
		    GetClientRect( hwnd, &client_rect );
		    border_left = ( ( client_rect.right  - client_rect.left - border_size * 2 ) % font_w + border_size * 2 ) / 2;
		    border_top =  ( ( client_rect.bottom - client_rect.top  - border_size * 2 ) % font_h + border_size * 2 ) / 2;
		    
	        if( send_event && size_handler )
			{
				PyObject * pyarglist = Py_BuildValue("(ii)", window_column, window_row );
				PyObject * pyresult = PyEval_CallObject( size_handler, pyarglist );
				Py_DECREF(pyarglist);
				if(pyresult)
				{
					Py_DECREF(pyresult);
				}
				else
				{
					PyErr_Print();
				}
			}
		
			// ウインドウのサイズが変わったので再描画する
	        InvalidateRect( hwnd, NULL, TRUE );
	    }
	}
}

void Window::_updateWindowFrameRect()
{
	FUNC_TRACE;
	
	// クライアント領域とウインドウ矩形の差分を計算する
	// AdjustWindowRectEx を使う方法だと、メニューバーが複数行に折り返したことに対応できない。
	
	RECT window_rect;
	if( ! GetWindowRect( hwnd, &window_rect ) )
	{
		return;
	}

	RECT client_rect;
	if( ! GetClientRect( hwnd, &client_rect ) )
	{
		return;
	}

	memset( &window_frame_rect, 0, sizeof(window_frame_rect) );
	window_frame_rect.right = (window_rect.right-window_rect.left) - (client_rect.right-client_rect.left);
	window_frame_rect.bottom = (window_rect.bottom-window_rect.top) - (client_rect.bottom-client_rect.top);
}

void Window::_setImePosition()
{
	FUNC_TRACE;

    HIMC imc = ImmGetContext(hwnd);

    LONG px = cursor_position.X - visible_rect.left;
    LONG py = cursor_position.Y - visible_rect.top;

    COMPOSITIONFORM cf;
    
    if( ime_rect.left==0 && ime_rect.right==0 )
    {
    	cf.dwStyle = CFS_POINT;
	    cf.ptCurrentPos.x = px * font_w + border_left;
	    cf.ptCurrentPos.y = py * font_h + border_top;
    }
    else
    {
	    cf.dwStyle = CFS_RECT;
	    cf.ptCurrentPos.x = px * font_w + border_left;
	    cf.ptCurrentPos.y = py * font_h + border_top;
	    cf.rcArea = ime_rect;
    }
    
    ImmSetCompositionWindow(imc, &cf);

    ImmReleaseContext(hwnd, imc);
}

void Window::_onTimerCursorBlink()
{
	FUNC_TRACE;

	if(cursor_blink>0)
	{
		--cursor_blink;
	}
	else
	{
		cursor_blink = 1;
	}

	if( cursor_position.X>=0 && cursor_position.Y>=0 )
	{
		RECT dirty_rect = { (cursor_position.X-1)*font_w+border_left, cursor_position.Y*font_h+border_top, (cursor_position.X+1)*font_w+border_left, (cursor_position.Y+1)*font_h+border_top };
		appendDirtyRect( dirty_rect );
	}
}

int Window::_getModKey()
{
	int mod = 0;
	if(GetKeyState(VK_MENU)&0xf0)       mod |= 1;
	if(GetKeyState(VK_CONTROL)&0xf0)    mod |= 2;
	if(GetKeyState(VK_SHIFT)&0xf0)      mod |= 4;
	if(GetKeyState(VK_LWIN)&0xf0)       mod |= 8;
	if(GetKeyState(VK_RWIN)&0xf0)       mod |= 8;
	return mod;
}

void Window::_clearMenuCommands()
{
	for( unsigned int i=0 ; i<menu_commands.size() ; ++i )
	{
		Py_XDECREF(menu_commands[i]);
	}
	menu_commands.clear();
}

void Window::_clearPopupMenuCommands()
{
	for( unsigned int i=0 ; i<popup_menu_commands.size() ; ++i )
	{
		Py_XDECREF(popup_menu_commands[i]);
	}
	popup_menu_commands.clear();
}

LRESULT CALLBACK Window::_wndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	//FUNC_TRACE;

    Window * window = (Window*)GetProp( hwnd, L"ckit_userdata" );

	PythonUtil::GIL_Ensure gil_ensure;

    switch(msg)
    {
    case WM_CREATE:
        {
            CREATESTRUCT * create_data = (CREATESTRUCT*)lp;
            Window * window = (Window*)create_data->lpCreateParams;
            window->hwnd = hwnd;
            SetProp( hwnd, L"ckit_userdata", window );

	        window->enableIme(false);

			GetCaretBlinkTime();

            SetTimer(hwnd, TIMER_PAINT, TIMER_PAINT_INTERVAL, NULL);
            SetTimer(hwnd, TIMER_CURSOR_BLINK, GetCaretBlinkTime(), NULL);
        }
        break;

    case WM_DESTROY:
        {
            KillTimer( hwnd, TIMER_PAINT );
            KillTimer( hwnd, TIMER_CURSOR_BLINK );

            RemoveProp( hwnd, L"ckit_userdata" );

            delete window;
        }
        break;

    case WM_CLOSE:
		if(window->close_handler)
		{
			PyObject * pyarglist = Py_BuildValue("()");
			PyObject * pyresult = PyEval_CallObject( window->close_handler, pyarglist );
			Py_DECREF(pyarglist);
			if(pyresult)
			{
				Py_DECREF(pyresult);
			}
			else
			{
				PyErr_Print();
			}
		}
		else
		{
	        return( DefWindowProc( hwnd, msg, wp, lp) );
		}
    	break;

	case WM_ACTIVATE:
		{
			switch(LOWORD(wp))
			{
			case WA_ACTIVE:
			case WA_CLICKACTIVE:
				window->active = true;
				break;
			case WA_INACTIVE:
			default:
				window->active = false;
				break;
			}

			if(window->activate_handler)
			{
				PyObject * pyarglist = Py_BuildValue("(i)", window->active );
				PyObject * pyresult = PyEval_CallObject( window->activate_handler, pyarglist );
				Py_DECREF(pyarglist);
				if(pyresult)
				{
					Py_DECREF(pyresult);
				}
				else
				{
					PyErr_Print();
				}
			}
		}
		break;

	case WM_INITMENU:
		{
	        window->_refreshMenu();
		}
		break;

	case WM_COMMAND:
		{
			int wmId    = LOWORD(wp);
			int wmEvent = HIWORD(wp);
			
			if( ID_MENUITEM<=wmId && wmId<ID_MENUITEM+(int)window->menu_commands.size() )
			{
				PyObject * func = window->menu_commands[wmId-ID_MENUITEM];
				if(func)
				{
					PyObject * command_info;
					if(command_info_constructor)
					{
						PyObject * pyarglist2 = Py_BuildValue( "()" );
						command_info = PyEval_CallObject( command_info_constructor, pyarglist2 );
						Py_DECREF(pyarglist2);
						if(!command_info)
						{
							PyErr_Print();
							break;
						}
					}
					else
					{
						command_info = Py_None;
						Py_INCREF(command_info);
					}
				
					PyObject * pyarglist = Py_BuildValue( "(O)", command_info );
					Py_DECREF(command_info);
					PyObject * pyresult = PyEval_CallObject( func, pyarglist );
					Py_DECREF(pyarglist);
					if(pyresult)
					{
						Py_DECREF(pyresult);
					}
					else
					{
						PyErr_Print();
					}
				}
			}
			else if( ID_POPUP_MENUITEM<=wmId && wmId<ID_POPUP_MENUITEM+(int)window->popup_menu_commands.size() )
			{
				PyObject * func = window->popup_menu_commands[wmId-ID_POPUP_MENUITEM];
				if(func)
				{
					PyObject * command_info;
					if(command_info_constructor)
					{
						PyObject * pyarglist2 = Py_BuildValue( "()" );
						command_info = PyEval_CallObject( command_info_constructor, pyarglist2 );
						Py_DECREF(pyarglist2);
						if(!command_info)
						{
							PyErr_Print();
							break;
						}
					}
					else
					{
						command_info = Py_None;
						Py_INCREF(command_info);
					}
				
					PyObject * pyarglist = Py_BuildValue( "(O)", command_info );
					Py_DECREF(command_info);
					PyObject * pyresult = PyEval_CallObject( func, pyarglist );
					Py_DECREF(pyarglist);
					if(pyresult)
					{
						Py_DECREF(pyresult);
					}
					else
					{
						PyErr_Print();
					}
				}
			}
			else
			{			
				return DefWindowProc( hwnd, msg, wp, lp);
			}
		}
		break;

    case WM_TIMER:
    	{
	    	if(wp==TIMER_PAINT)
	    	{
		        window->flushPaint();
		        
		        if( window->delayed_call_list.size() )
		        {
		    		window->delayed_call_list_ref_count ++;

					for( std::list<DelayedCallInfo>::iterator i=window->delayed_call_list.begin(); i!=window->delayed_call_list.end() ; i++ )
					{
						if(i->pyobj)
						{
							i->timeout -= TIMER_PAINT_INTERVAL;
							if(i->timeout<=0)
							{
								// ネストした呼び出しが起きないように、呼び出し前にNULLにする。
								PyObject * pyobj = i->pyobj;
								i->pyobj = NULL;

								PyObject * pyarglist = Py_BuildValue("()" );
								PyObject * pyresult = PyEval_CallObject( pyobj, pyarglist );
								Py_DECREF(pyarglist);
								if(pyresult)
								{
									Py_DECREF(pyresult);
								}
								else
								{
									PyErr_Print();
								}
						
								Py_XDECREF(pyobj);
							}
						}
					}

					window->delayed_call_list_ref_count --;

					// 無効化されたノードを削除
		    		if(window->delayed_call_list_ref_count==0)
		    		{
						for( std::list<DelayedCallInfo>::iterator i=window->delayed_call_list.begin(); i!=window->delayed_call_list.end() ; )
						{
							if( (i->pyobj)==0 )
							{
								i = window->delayed_call_list.erase(i);
							}
							else
							{
								i++;
							}
						}
		    		}
				}
	    	}
	    	else if(wp==TIMER_CURSOR_BLINK)
			{
		        window->_onTimerCursorBlink();
		        window->flushPaint();
			}
	    	else
	    	{
	    		if( ! window->quit_requested )
	    		{
		    		window->timer_list_ref_count ++;

					for( std::list<TimerInfo>::iterator i=window->timer_list.begin(); i!=window->timer_list.end() ; i++ )
					{
						if( wp == (WPARAM)i->pyobj )
						{
							if(i->calling) continue;
					
							i->calling = true;

							PyObject * pyarglist = Py_BuildValue("()" );
							PyObject * pyresult = PyEval_CallObject( (i->pyobj), pyarglist );
							Py_DECREF(pyarglist);
							if(pyresult)
							{
								Py_DECREF(pyresult);
							}
							else
							{
								if( PyErr_Occurred()==PyExc_KeyboardInterrupt )
								{
									printf("KeyboardInterrupt\n");
									exit(0);
								}

								PyErr_Print();
							}

							i->calling = false;

							break;
						}
					}

		    		window->timer_list_ref_count --;

					// 無効化されたノードを削除
		    		if(window->timer_list_ref_count==0)
		    		{
						for( std::list<TimerInfo>::iterator i=window->timer_list.begin(); i!=window->timer_list.end() ; )
						{
							if( (i->pyobj)==0 )
							{
								i = window->timer_list.erase(i);
							}
							else
							{
								i++;
							}
						}
		    		}
	    		}
	    	}
    	}
        break;

    case WM_HOTKEY:
		if( ! window->quit_requested )
		{
    		window->hotkey_list_ref_count ++;

			for( std::list<HotKeyInfo>::iterator i=window->hotkey_list.begin(); i!=window->hotkey_list.end() ; i++ )
			{
				if( wp == i->id )
				{
					if(i->calling) continue;
					
					i->calling = true;

					PyObject * pyarglist = Py_BuildValue("()" );
					PyObject * pyresult = PyEval_CallObject( i->pyobj, pyarglist );
					Py_DECREF(pyarglist);
					if(pyresult)
					{
						Py_DECREF(pyresult);
					}
					else
					{
						PyErr_Print();
					}

					i->calling = false;

					break;
				}
			}

    		window->hotkey_list_ref_count --;

			// 無効化されたノードを削除
			if(window->hotkey_list_ref_count==0)
			{
				for( std::list<HotKeyInfo>::iterator i=window->hotkey_list.begin(); i!=window->hotkey_list.end() ; )
				{
					if( i->pyobj==0 )
					{
						i = window->hotkey_list.erase(i);
					}
					else
					{
						i++;
					}
				}
			}
		}
		break;

    case WM_ERASEBKGND:
        break;

    case WM_PAINT:
    	{
			if(walkaround_kb436093)
    		{
			    // WindowsXP のバグの対処
			    // http://support.microsoft.com/kb/436093/ja

			    RECT client_rect;
			    GetClientRect(hwnd, &client_rect);

				HDC hDC = GetDC(hwnd);
				
				BitBlt( 
    				hDC, 
    				client_rect.left, 
    				client_rect.top, 
    				client_rect.right - client_rect.left, 
    				client_rect.bottom - client_rect.top, 
    				window->offscreen_dc, 
    				client_rect.left, 
    				client_rect.top, 
    				SRCCOPY );

				ReleaseDC( hwnd, hDC );

			    PAINTSTRUCT ps;
			    hDC = BeginPaint(hwnd, &ps);
		        EndPaint(hwnd, &ps);
			}
    		else
    		{
			    PAINTSTRUCT ps;
			    HDC	hDC = BeginPaint(hwnd, &ps);

				BitBlt( 
    				hDC, 
    				ps.rcPaint.left, 
    				ps.rcPaint.top, 
    				ps.rcPaint.right - ps.rcPaint.left, 
    				ps.rcPaint.bottom - ps.rcPaint.top, 
    				window->offscreen_dc, 
    				ps.rcPaint.left, 
    				ps.rcPaint.top, 
    				SRCCOPY );

		        EndPaint(hwnd, &ps);
    		}
    	}
        break;

    case WM_SIZING:
        window->_onSizing((DWORD)wp, (LPRECT)lp);
        break;

    case WM_WINDOWPOSCHANGING:
        window->_onWindowPosChange( (WINDOWPOS*)lp, false );
        break;

    case WM_WINDOWPOSCHANGED:
        window->_onWindowPosChange( (WINDOWPOS*)lp, true );
        break;

    case WM_LBUTTONDOWN:
		if(window->lbuttondown_handler)
		{
			int mod = _getModKey();
			
			PyObject * pyarglist = Py_BuildValue("(iii)", (short)LOWORD(lp), (short)HIWORD(lp), mod );
			PyObject * pyresult = PyEval_CallObject( window->lbuttondown_handler, pyarglist );
			Py_DECREF(pyarglist);
			if(pyresult)
			{
				Py_DECREF(pyresult);
			}
			else
			{
				PyErr_Print();
			}
		}
        break;

    case WM_LBUTTONUP:
		if(window->lbuttonup_handler)
		{
			int mod = _getModKey();

			PyObject * pyarglist = Py_BuildValue("(iii)", (short)LOWORD(lp), (short)HIWORD(lp), mod );
			PyObject * pyresult = PyEval_CallObject( window->lbuttonup_handler, pyarglist );
			Py_DECREF(pyarglist);
			if(pyresult)
			{
				Py_DECREF(pyresult);
			}
			else
			{
				PyErr_Print();
			}
		}
        break;

    case WM_MBUTTONDOWN:
		if(window->mbuttondown_handler)
		{
			int mod = _getModKey();

			PyObject * pyarglist = Py_BuildValue("(iii)", (short)LOWORD(lp), (short)HIWORD(lp), mod );
			PyObject * pyresult = PyEval_CallObject( window->mbuttondown_handler, pyarglist );
			Py_DECREF(pyarglist);
			if(pyresult)
			{
				Py_DECREF(pyresult);
			}
			else
			{
				PyErr_Print();
			}
		}
        break;

    case WM_MBUTTONUP:
		if(window->mbuttonup_handler)
		{
			int mod = _getModKey();

			PyObject * pyarglist = Py_BuildValue("(iii)", (short)LOWORD(lp), (short)HIWORD(lp), mod );
			PyObject * pyresult = PyEval_CallObject( window->mbuttonup_handler, pyarglist );
			Py_DECREF(pyarglist);
			if(pyresult)
			{
				Py_DECREF(pyresult);
			}
			else
			{
				PyErr_Print();
			}
		}
        break;

    case WM_RBUTTONDOWN:
		if(window->rbuttondown_handler)
		{
			int mod = _getModKey();

			PyObject * pyarglist = Py_BuildValue("(iii)", (short)LOWORD(lp), (short)HIWORD(lp), mod );
			PyObject * pyresult = PyEval_CallObject( window->rbuttondown_handler, pyarglist );
			Py_DECREF(pyarglist);
			if(pyresult)
			{
				Py_DECREF(pyresult);
			}
			else
			{
				PyErr_Print();
			}
		}
        break;

    case WM_RBUTTONUP:
		if(window->rbuttonup_handler)
		{
			int mod = _getModKey();

			PyObject * pyarglist = Py_BuildValue("(iii)", (short)LOWORD(lp), (short)HIWORD(lp), mod );
			PyObject * pyresult = PyEval_CallObject( window->rbuttonup_handler, pyarglist );
			Py_DECREF(pyarglist);
			if(pyresult)
			{
				Py_DECREF(pyresult);
			}
			else
			{
				PyErr_Print();
			}
		}
        break;

    case WM_LBUTTONDBLCLK:
		if(window->lbuttondoubleclick_handler)
		{
			int mod = _getModKey();

			PyObject * pyarglist = Py_BuildValue("(iii)", (short)LOWORD(lp), (short)HIWORD(lp), mod );
			PyObject * pyresult = PyEval_CallObject( window->lbuttondoubleclick_handler, pyarglist );
			Py_DECREF(pyarglist);
			if(pyresult)
			{
				Py_DECREF(pyresult);
			}
			else
			{
				PyErr_Print();
			}
		}
        break;

    case WM_MBUTTONDBLCLK:
		if(window->mbuttondoubleclick_handler)
		{
			int mod = _getModKey();

			PyObject * pyarglist = Py_BuildValue("(iii)", (short)LOWORD(lp), (short)HIWORD(lp), mod );
			PyObject * pyresult = PyEval_CallObject( window->mbuttondoubleclick_handler, pyarglist );
			Py_DECREF(pyarglist);
			if(pyresult)
			{
				Py_DECREF(pyresult);
			}
			else
			{
				PyErr_Print();
			}
		}
        break;

    case WM_RBUTTONDBLCLK:
		if(window->rbuttondoubleclick_handler)
		{
			int mod = _getModKey();

			PyObject * pyarglist = Py_BuildValue("(iii)", (short)LOWORD(lp), (short)HIWORD(lp), mod );
			PyObject * pyresult = PyEval_CallObject( window->rbuttondoubleclick_handler, pyarglist );
			Py_DECREF(pyarglist);
			if(pyresult)
			{
				Py_DECREF(pyresult);
			}
			else
			{
				PyErr_Print();
			}
		}
        break;

    case WM_MOUSEMOVE:
		if(window->mousemove_handler)
		{
			int mod = _getModKey();

			PyObject * pyarglist = Py_BuildValue("(iii)", (short)LOWORD(lp), (short)HIWORD(lp), mod );
			PyObject * pyresult = PyEval_CallObject( window->mousemove_handler, pyarglist );
			Py_DECREF(pyarglist);
			if(pyresult)
			{
				Py_DECREF(pyresult);
			}
			else
			{
				PyErr_Print();
			}
		}
        break;

	case WM_MOUSEWHEEL:
		if(window->mousewheel_handler)
		{
			int mod = _getModKey();

			PyObject * pyarglist = Py_BuildValue("(iiii)", (short)LOWORD(lp), (short)HIWORD(lp), ((short)HIWORD(wp))/WHEEL_DELTA, mod );
			PyObject * pyresult = PyEval_CallObject( window->mousewheel_handler, pyarglist );
			Py_DECREF(pyarglist);
			if(pyresult)
			{
				Py_DECREF(pyresult);
			}
			else
			{
				PyErr_Print();
			}
		}
		break;

    case WM_DROPFILES:
    	{
			PyObject * pyfiles = PyList_New(0);

	 		HDROP drop = (HDROP)wp;
	 		POINT point;
 			UINT num_items = DragQueryFile( drop, 0xFFFFFFFF, NULL, 0 );
	 		for( unsigned int i=0 ; i<num_items ; ++i )
	 		{
 				wchar_t path[MAX_PATH+2];
 	 			DragQueryFile( drop, i, path, sizeof(path) );

				PyObject * pyitem = Py_BuildValue( "u", path );
				PyList_Append( pyfiles, pyitem );
				Py_XDECREF(pyitem);
	 		}
			DragQueryPoint( drop, &point );
	 		DragFinish(drop);

	        if( window->dropfiles_handler )
			{
				PyObject * pyarglist = Py_BuildValue("(iiO)", point.x, point.y, pyfiles );
				Py_XDECREF(pyfiles);
				PyObject * pyresult = PyEval_CallObject( window->dropfiles_handler, pyarglist );
				Py_DECREF(pyarglist);
				if(pyresult)
				{
					Py_DECREF(pyresult);
				}
				else
				{
					PyErr_Print();
				}
			}

    	}
        break;

    case WM_COPYDATA:
    	{
			COPYDATASTRUCT * data = (COPYDATASTRUCT*)lp;

	        if( data->dwData==0x101 && window->ipc_handler )
			{
		
				PyObject * pyarglist = Py_BuildValue("(s#)", data->lpData, data->cbData );
				PyObject * pyresult = PyEval_CallObject( window->ipc_handler, pyarglist );
				Py_DECREF(pyarglist);
				if(pyresult)
				{
					Py_DECREF(pyresult);
				}
				else
				{
					PyErr_Print();
				}
			}
    	}
        break;

    case WM_IME_STARTCOMPOSITION:
        window->_setImePosition();
        return( DefWindowProc( hwnd, msg, wp, lp) );

    case WM_IME_NOTIFY:
        if(wp == IMN_SETOPENSTATUS) {
            HIMC imc = ImmGetContext(hwnd);
            window->ime_on = ImmGetOpenStatus(imc)!=FALSE;
            ImmReleaseContext(hwnd, imc);
            InvalidateRect( hwnd, NULL, TRUE);
        }
        return( DefWindowProc(hwnd, msg, wp, lp) );

    case WM_VSCROLL:
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
		if(window->keydown_handler)
		{
			INT_PTR vk = wp;
			int mod = _getModKey();

			PyObject * pyarglist = Py_BuildValue("(ii)", vk, mod );
			PyObject * pyresult = PyEval_CallObject( window->keydown_handler, pyarglist );
			Py_DECREF(pyarglist);
			if(pyresult)
			{
				Py_DECREF(pyresult);
			}
			else
			{
				PyErr_Print();
			}
		}
    	break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
		if(window->keyup_handler)
		{
			INT_PTR vk = wp;
			int mod = _getModKey();

			PyObject * pyarglist = Py_BuildValue("(ii)", vk, mod );
			PyObject * pyresult = PyEval_CallObject( window->keyup_handler, pyarglist );
			Py_DECREF(pyarglist);
			if(pyresult)
			{
				Py_DECREF(pyresult);
			}
			else
			{
				PyErr_Print();
			}
		}
        break;

    case WM_CHAR:
    case WM_IME_CHAR:
		if(window->char_handler)
		{
			INT_PTR ch = wp;
			int mod = _getModKey();

			PyObject * pyarglist = Py_BuildValue("(ii)", ch, mod );
			PyObject * pyresult = PyEval_CallObject( window->char_handler, pyarglist );
			Py_DECREF(pyarglist);
			if(pyresult)
			{
				Py_DECREF(pyresult);
			}
			else
			{
				PyErr_Print();
			}
		}
        break;

	case WM_ENDSESSION:
		if(wp)
		{
			if(window->endsession_handler)
			{
				PyObject * pyarglist = Py_BuildValue("()");
				PyObject * pyresult = PyEval_CallObject( window->endsession_handler, pyarglist );
				Py_DECREF(pyarglist);
				if(pyresult)
				{
					Py_DECREF(pyresult);
				}
				else
				{
					PyErr_Print();
				}
			}
		}
		return 0;

	case WM_NCPAINT:
		if( window->ncpaint )
		{
			HDC hdc = GetWindowDC(hwnd);
			window->_onNcPaint(hdc);
			ReleaseDC(hwnd, hdc);
			return 0;
		}
		else
		{
	        return( DefWindowProc( hwnd, msg, wp, lp) );
		}
		
	case WM_NCHITTEST:
		if( window->nchittest_handler )
		{
			PyObject * pyarglist = Py_BuildValue("(ii)", (short)LOWORD(lp), (short)HIWORD(lp) );
			PyObject * pyresult = PyEval_CallObject( window->nchittest_handler, pyarglist );
			Py_DECREF(pyarglist);
			if(pyresult)
			{
				int result;
				PyArg_Parse(pyresult,"i", &result );
				Py_DECREF(pyresult);
				return result;
			}
			else
			{
				PyErr_Print();
			}
		}

    default:
        return( DefWindowProc( hwnd, msg, wp, lp) );
    }

    return(1);
}

bool Window::_registerWindowClass()
{
	FUNC_TRACE;

    HINSTANCE hInstance = GetModuleHandle(NULL);

    WNDCLASSEX wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = _wndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon( hInstance, (const wchar_t*)1 );
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush( RGB(0x00, 0x00, 0x00) );
    wc.lpszMenuName = NULL;
    wc.lpszClassName = WINDOW_CLASS_NAME.c_str();
	wc.hIconSm = (HICON)LoadImage( hInstance, (const wchar_t*)1, IMAGE_ICON, 16, 16, 0 );
    if(!wc.hIconSm) wc.hIconSm = wc.hIcon;

    if(! RegisterClassEx(&wc))
    {
        printf("RegisterClassEx failed\n");
        return(FALSE);
    }

    return(TRUE);
}

bool Window::_createWindow( Param & param )
{
	FUNC_TRACE;

    HINSTANCE hInstance = GetModuleHandle(NULL);
    style = WS_OVERLAPPED | WS_CLIPCHILDREN | WS_POPUP;
    exstyle = WS_EX_ACCEPTFILES;
    LONG	w, h;
    LONG	posx, posy;

    if( param.is_transparent_color_given || (param.is_transparency_given) )
    {
        exstyle |= WS_EX_LAYERED;
    }

    if(param.is_top_most)
    {
        exstyle |= WS_EX_TOPMOST;
    }

    if(param.title_bar)
    {
        style |= WS_CAPTION;
    }

    if(param.resizable)
    {
        style |= WS_THICKFRAME;
    }
    else if(!param.noframe)
    {
    	exstyle |= WS_EX_DLGMODALFRAME;
    }

    if(param.minimizebox)
    {
        style |= WS_MINIMIZEBOX;
    }

    if(param.maximizebox && param.resizable)
    {
        style |= WS_MAXIMIZEBOX;
    }

    if( param.sysmenu )
    {
    	style |= WS_SYSMENU;
    }

    if( param.tool )
    {
        exstyle |= WS_EX_TOOLWINDOW;
    }
	
    AdjustWindowRectEx( &window_frame_rect, style, (menu!=NULL), exstyle );

    window_column = w = width();
    window_row = h = height();
    w *= font_w;
    h *= font_h;
    w += border_size * 2;
    h += border_size * 2;
    w += window_frame_rect.right  - window_frame_rect.left;
    h += window_frame_rect.bottom - window_frame_rect.top;

    if(param.is_winpos_given)
    {
        posx = param.winpos_x;
        posy = param.winpos_y;

        if( param.origin & ORIGIN_X_CENTER )
        {
        	posx -= w / 2;
        }
        else if( param.origin & ORIGIN_X_RIGHT )
        {
        	posx -= w;
        }

        if( param.origin & ORIGIN_Y_CENTER )
        {
        	posy -= h / 2;
        }
        else if( param.origin & ORIGIN_Y_BOTTOM )
        {
        	posy -= h;
        }
    }
    else
    {
        posx = CW_USEDEFAULT;
        posy = CW_USEDEFAULT;
    }

    CreateWindowEx( exstyle, WINDOW_CLASS_NAME.c_str(), param.title.c_str(), style,
                    posx, posy, w, h,
                    param.parent_window_hwnd, NULL, hInstance, this );

    if(!hwnd)
    {
        printf("CreateWindowEx failed\n");
        return(FALSE);
    }

    if(param.is_transparency_given)
    {
        SetLayeredWindowAttributes(hwnd, 0, param.transparency, LWA_ALPHA);
    }
    else if(param.is_transparent_color_given)
    {
        SetLayeredWindowAttributes(hwnd, param.transparent_color, 255, LWA_COLORKEY);
    }

    if( param.show )
    {
	    ShowWindow(hwnd, SW_SHOW);
    }

    return(TRUE);
}

bool Window::setFont( Window * src )
{
	FUNC_TRACE;
	
	logfont = src->logfont;

    if(font_handle && font_handle_created) { DeleteObject(font_handle); font_handle=NULL; font_handle_created=false; }
    font_handle = src->font_handle;

    font_w = src->font_w;
    font_h = src->font_h;
	zenkaku_table = src->zenkaku_table;
	
	updateImeFont();

    return(TRUE);
}

bool Window::setFont( const wchar_t * name, int height )
{
	FUNC_TRACE;
	
    memset(&logfont, 0, sizeof(logfont));
    logfont.lfHeight = -height;
    logfont.lfWidth = 0;
    logfont.lfEscapement = 0;
    logfont.lfOrientation = 0;
    logfont.lfWeight = FW_NORMAL;
    logfont.lfItalic = 0;
    logfont.lfUnderline = 0;
    logfont.lfStrikeOut = 0;
    logfont.lfCharSet = DEFAULT_CHARSET;
    logfont.lfOutPrecision = OUT_DEFAULT_PRECIS;
    logfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;

    logfont.lfQuality = DEFAULT_QUALITY; // OS の設定によっては CLEARTYPE_QUALITY と同じ動きになる
	//logfont.lfQuality = CLEARTYPE_QUALITY; // ANTIALIASED_QUALITYよりなぜか速い
	//logfont.lfQuality = ANTIALIASED_QUALITY; // なぜか ClearType 有効時に低速になってしまう

    logfont.lfPitchAndFamily = FIXED_PITCH | FF_DONTCARE;
    lstrcpy( logfont.lfFaceName, name );

    if(font_handle && font_handle_created) { DeleteObject(font_handle); font_handle=NULL; font_handle_created=false; }
    font_handle = CreateFontIndirect(&logfont);
    font_handle_created = true;

    HDC	hDC = GetDC(NULL);
    HGDIOBJ	oldfont = SelectObject(hDC, font_handle);

	{
	    TEXTMETRIC met;
	    GetTextMetrics(hDC, &met);

		int * char_width_table = (int*)malloc(0x10000*sizeof(int));
	    GetCharWidth32( hDC, 0, 0xffff, char_width_table );

	    INT	width = 0;
	    for(int i=0 ; i<26 ; i++)
	    {
	        width += char_width_table['A'+i];
	        width += char_width_table['a'+i];
	    }
	    width /= 26 * 2;
	    font_w = width;
	    font_h = met.tmHeight;

		zenkaku_table.clear();
	    for( int i=0 ; i<=0xffff ; i++ )
	    {
		    zenkaku_table.push_back( char_width_table[i] > font_w );
	    }

	    free(char_width_table);
	}

    SelectObject(hDC, oldfont);
    ReleaseDC(NULL, hDC);

	updateImeFont();

    return(TRUE);
}

static int CALLBACK cbEnumFont(
  ENUMLOGFONTEX *lpelfe,    // 論理的なフォントデータ
  NEWTEXTMETRICEX *lpntme,  // 物理的なフォントデータ
  DWORD FontType,           // フォントの種類
  LPARAM lParam             // アプリケーション定義のデータ
)
{
	if( ( lpelfe->elfLogFont.lfPitchAndFamily & FIXED_PITCH )==0 ) return 1;
	if( lpelfe->elfLogFont.lfFaceName[0]=='@' ) return 1;

	std::vector<std::wstring> & font_list = *((std::vector<std::wstring>*)lParam);

	for( std::vector<std::wstring>::const_iterator i=font_list.begin() ; i!=font_list.end() ; i++ )
	{
		if( *i == lpelfe->elfLogFont.lfFaceName ) return 1;
	}

	font_list.push_back( lpelfe->elfLogFont.lfFaceName );

    return 1;
}

void Window::enumFonts( std::vector<std::wstring> * font_list )
{
	HDC hdc = GetDC(NULL);

	EnumFontFamiliesEx(
		hdc,
		NULL,
		(FONTENUMPROC)cbEnumFont,
		(LPARAM)font_list,
		0
	);

	ReleaseDC( NULL, hdc );
}

void Window::setBGColor( COLORREF color )
{
    if(bg_brush){ DeleteObject(bg_brush); }
    bg_brush = CreateSolidBrush(color);

	RECT dirty_rect;
    GetClientRect( hwnd, &dirty_rect );
	appendDirtyRect( dirty_rect );

	RedrawWindow( hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE );
}

void Window::setFrameColor( COLORREF color )
{
    if(frame_pen){ DeleteObject(frame_pen); }
    frame_pen = CreatePen( PS_SOLID, 0, color );

	RedrawWindow( hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE );
}

void Window::setCursorColor( COLORREF color0, COLORREF color1 )
{
	FUNC_TRACE;

    if(cursor0_brush){ DeleteObject(cursor0_brush); }
    if(cursor1_brush){ DeleteObject(cursor1_brush); }

    cursor0_brush = CreateSolidBrush(color0);
    cursor1_brush = CreateSolidBrush(color1);

	RECT dirty_rect = { (cursor_position.X-1)*font_w+border_left, cursor_position.Y*font_h+border_top, (cursor_position.X+1)*font_w+border_left, (cursor_position.Y+1)*font_h+border_top };
	appendDirtyRect( dirty_rect );
}

void Window::_buildMenu( HMENU menu_handle, PyObject * pysequence, int depth, bool parent_enabled )
{
	if( PySequence_Check(pysequence) )
	{
		int num_items = (int)PySequence_Length(pysequence);
		for( int i=0 ; i<num_items ; ++i )
		{
			PyObject * pychild_node = PySequence_GetItem( pysequence, i );
			
			bool enabled = parent_enabled;
			
			if( MenuNode_Check(pychild_node) )
			{
				MenuNode * child_node = ((MenuNode_Object*)pychild_node)->p;
			
				UINT flags = 0;
			
		        if( depth && child_node->visible )
				{
					PyObject * pyarglist = Py_BuildValue("()");
					PyObject * pyresult = PyEval_CallObject( child_node->visible, pyarglist );
					Py_DECREF(pyarglist);
					if(pyresult)
					{
						int result;
						PyArg_Parse(pyresult,"i", &result );
						Py_DECREF(pyresult);
					
						if(!result)
						{
							goto cont;
						}
					}
					else
					{
						PyErr_Print();
					}
				}
			
				if(enabled)
				{
			        if( child_node->enabled )
					{
						PyObject * pyarglist = Py_BuildValue("()");
						PyObject * pyresult = PyEval_CallObject( child_node->enabled, pyarglist );
						Py_DECREF(pyarglist);
						if(pyresult)
						{
							int result;
							PyArg_Parse(pyresult,"i", &result );
							Py_DECREF(pyresult);
					
							if(!result)
							{
								enabled = false;
							
								if(depth)
								{
									flags |= MF_DISABLED | MF_GRAYED;
								}
							}
						}
						else
						{
							PyErr_Print();
						}
					}
				}
				else
				{
					if(depth)
					{
						flags |= MF_DISABLED | MF_GRAYED;
					}
				}
			
		        if( depth && child_node->checked )
				{
					PyObject * pyarglist = Py_BuildValue("()");
					PyObject * pyresult = PyEval_CallObject( child_node->checked, pyarglist );
					Py_DECREF(pyarglist);
					if(pyresult)
					{
						int result;
						PyArg_Parse(pyresult,"i", &result );
						Py_DECREF(pyresult);
					
						if(result)
						{
							flags |= MF_CHECKED;
						}
						else
						{
							flags |= MF_UNCHECKED;
						}
					}
					else
					{
						PyErr_Print();
					}
				}
			
				if( depth && child_node->separator )
				{
					AppendMenu( menu_handle, MF_SEPARATOR, 0, L"" );
				}
		        else if( child_node->items )
				{
					HMENU child_menu_handle = CreateMenu();
				
					_buildMenu( child_menu_handle, child_node->items, depth+1, enabled );

					AppendMenu( menu_handle, flags | MF_POPUP, (UINT_PTR)child_menu_handle, child_node->text.c_str() );
				}
				else if( depth && child_node->command )
				{
				    if( ID_MENUITEM + menu_commands.size() > ID_MENUITEM_MAX )
				    {
						goto cont;
				    }
			    
					AppendMenu( menu_handle, flags, ID_MENUITEM + menu_commands.size(), child_node->text.c_str() );

					menu_commands.push_back(child_node->command);
					Py_INCREF(child_node->command);
				}
			}
			else if( depth && PyCallable_Check(pychild_node) )
			{
				PyObject * pyarglist = Py_BuildValue("()");
				PyObject * pyresult = PyEval_CallObject( pychild_node, pyarglist );
				Py_DECREF(pyarglist);
				if(pyresult)
				{
					_buildMenu( menu_handle, pyresult, depth, enabled );
				
					Py_DECREF(pyresult);
				}
				else
				{
					PyErr_Print();
				}
			}
			
			cont:

			Py_XDECREF(pychild_node);
		}
	}
}

void Window::setMenu( PyObject * _menu )
{
	if( menu == _menu ) return;
	
	Py_XDECREF(menu);
	menu = _menu;
	Py_XINCREF(menu);
	
	HMENU old_menu_handle = GetMenu(hwnd);

	_clearMenuCommands();

	// MenuNode にしたがってメニューを再構築する
	if(menu)
	{
		HMENU menu_handle = CreateMenu();
	
		if( MenuNode_Check(menu) )
		{
			_buildMenu( menu_handle, ((MenuNode_Object*)menu)->p->items, 0, true );
		}

		SetMenu( hwnd, menu_handle );
	}
	else
	{
		SetMenu( hwnd, NULL );
	}

	// 古いメニューを削除する
	if(old_menu_handle)
	{
		DestroyMenu(old_menu_handle);
	}

	_updateWindowFrameRect();
}

void Window::_refreshMenu()
{
	_clearMenuCommands();

	// MenuNode にしたがってメニューを再構築する
	if(menu)
	{
		HMENU menu_handle = GetMenu(hwnd);
		
		// メニューをクリアする
		while(true)
		{
			if( ! DeleteMenu( menu_handle, 0, MF_BYPOSITION ) )
			{
				break;
			}
		}

		if( MenuNode_Check(menu) )
		{
			bool menu_enabled = true;
	        if( ((MenuNode_Object*)menu)->p->enabled )
			{
				PyObject * pyarglist = Py_BuildValue("()");
				PyObject * pyresult = PyEval_CallObject( ((MenuNode_Object*)menu)->p->enabled, pyarglist );
				Py_DECREF(pyarglist);
				if(pyresult)
				{
					int result;
					PyArg_Parse(pyresult,"i", &result );
					Py_DECREF(pyresult);
				
					if(!result)
					{
						menu_enabled = false;
					}
				}
				else
				{
					PyErr_Print();
				}
			}
			
			_buildMenu( menu_handle, ((MenuNode_Object*)menu)->p->items, 0, menu_enabled );
		}
		
		DrawMenuBar(hwnd);
	}
}

void Window::setPosSize( int x, int y, int width, int height, int origin )
{
	FUNC_TRACE;
	
    int client_w = width * font_w + border_size * 2;
    int client_h = height * font_h + border_size * 2;
    int window_w = client_w + (window_frame_rect.right  - window_frame_rect.left);
    int window_h = client_h + (window_frame_rect.bottom - window_frame_rect.top);

    if( origin & ORIGIN_X_CENTER )
    {
    	x -= window_w / 2;
    }
    else if( origin & ORIGIN_X_RIGHT )
    {
    	x -= window_w;
    }

    if( origin & ORIGIN_Y_CENTER )
    {
    	y -= window_h / 2;
    }
    else if( origin & ORIGIN_Y_BOTTOM )
    {
    	y -= window_h;
    }
    
	::SetWindowPos( hwnd, NULL, x, y, window_w, window_h, SWP_NOZORDER | SWP_NOACTIVATE );

	RECT dirty_rect = { 0, 0, client_w, client_h };
	appendDirtyRect( dirty_rect );
}

void Window::setCapture()
{
	SetCapture(hwnd);
}

void Window::releaseCapture()
{
    if(hwnd!=GetCapture())
    {
        return;
    }
    ReleaseCapture();
}

void Window::setMouseCursor( int id )
{
	LPCTSTR idc;
	
	switch(id)
	{
	case MOUSE_CURSOR_APPSTARTING:
		idc = IDC_APPSTARTING;
		break;
	case MOUSE_CURSOR_ARROW:
		idc = IDC_ARROW;
		break;
	case MOUSE_CURSOR_CROSS:
		idc = IDC_CROSS;
		break;
	case MOUSE_CURSOR_HAND:
		idc = IDC_HAND;
		break;
	case MOUSE_CURSOR_HELP:
		idc = IDC_HELP;
		break;
	case MOUSE_CURSOR_IBEAM:
		idc = IDC_IBEAM;
		break;
	case MOUSE_CURSOR_NO:
		idc = IDC_NO;
		break;
	case MOUSE_CURSOR_SIZEALL:
		idc = IDC_SIZEALL;
		break;
	case MOUSE_CURSOR_SIZENESW:
		idc = IDC_SIZENESW;
		break;
	case MOUSE_CURSOR_SIZENS:
		idc = IDC_SIZENS;
		break;
	case MOUSE_CURSOR_SIZENWSE:
		idc = IDC_SIZENWSE;
		break;
	case MOUSE_CURSOR_SIZEWE:
		idc = IDC_SIZEWE;
		break;
	case MOUSE_CURSOR_UPARROW:
		idc = IDC_UPARROW;
		break;
	case MOUSE_CURSOR_WAIT:
		idc = IDC_WAIT;
		break;
	default:
		return;	
	}

	HCURSOR hcursor = LoadCursor(NULL,idc);
	//printf("hcursor = %d\n",hcursor);
	SetCursor(hcursor);
}

void Window::drag( int x, int y )
{
	PostMessage( hwnd, WM_NCLBUTTONDOWN, (WPARAM)HTCAPTION, ((y<<16)|x) );
}

void Window::getNormalSize( int * _width, int * _height ) const
{
	FUNC_TRACE;

	WINDOWPLACEMENT window_place;
	memset( &window_place, 0, sizeof(window_place) );
	window_place.length = sizeof(window_place);
	::GetWindowPlacement( hwnd, &window_place );

    int fw = (window_frame_rect.right - window_frame_rect.left) + (border_size * 2);
    int fh = (window_frame_rect.bottom - window_frame_rect.top) + (border_size * 2);

	int width = window_place.rcNormalPosition.right - window_place.rcNormalPosition.left;
	int height = window_place.rcNormalPosition.bottom - window_place.rcNormalPosition.top;

    width = width - fw;
    height = height - fh;

    if(width<0) width=0;
    if(height<0) height=0;

    width = width / font_w;
    height = height / font_h;

	*_width = width;
	*_height = height;
}

void Window::show( bool show, bool activate )
{
	FUNC_TRACE;

    ShowWindow( hwnd, show ? (activate?SW_SHOW:SW_SHOWNOACTIVATE) : SW_HIDE );
}

void Window::enable( bool enable )
{
	FUNC_TRACE;

    EnableWindow( hwnd, enable );
}

void Window::destroy()
{
	FUNC_TRACE;

	_clearMenuCommands();
	_clearPopupMenuCommands();

    DestroyWindow(hwnd);
}

void Window::activate()
{
	FUNC_TRACE;

    SetActiveWindow(hwnd);
}

void Window::inactivate()
{
	FUNC_TRACE;

	HWND _hwnd = GetForegroundWindow();

	while(_hwnd)
	{
		LONG ex_style = GetWindowLong( _hwnd, GWL_EXSTYLE );

		TCHAR class_name[256];
		GetClassName( _hwnd, class_name, sizeof(class_name) );

		if( wcscmp( class_name, WINDOW_CLASS_NAME.c_str() )!=0
		 &&	!(ex_style & WS_EX_TOPMOST)
		 && IsWindowVisible(_hwnd)
		 &&	IsWindowEnabled(_hwnd) )
		{
			break;
		}

		_hwnd = GetWindow(_hwnd,GW_HWNDNEXT);
	}

	if( _hwnd )
	{
	    HWND hwnd_last_active = GetLastActivePopup( _hwnd );
	    SetForegroundWindow( hwnd_last_active );
	}
}

void Window::foreground()
{
	FUNC_TRACE;

    HWND hwnd_last_active = GetLastActivePopup( hwnd );
    SetForegroundWindow( hwnd_last_active );
}

void Window::restore()
{
	FUNC_TRACE;

    ShowWindow( hwnd, SW_RESTORE );
}

void Window::maximize()
{
	FUNC_TRACE;

    ShowWindow( hwnd, SW_MAXIMIZE );
}

void Window::minimize()
{
	FUNC_TRACE;

    ShowWindow( hwnd, SW_MINIMIZE );
}

void Window::topmost( bool topmost )
{
	FUNC_TRACE;

	if(topmost)
	{
	    SetWindowPos( hwnd, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	}
	else
	{
	    SetWindowPos( hwnd, HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	}
}

bool Window::isEnabled()
{
	FUNC_TRACE;

	return ::IsWindowEnabled( hwnd )!=FALSE;
}

bool Window::isVisible()
{
	FUNC_TRACE;

	return ::IsWindowVisible( hwnd )!=FALSE;
}

bool Window::isMaximized()
{
	FUNC_TRACE;
	
	return ::IsZoomed( hwnd )!=FALSE;
}

bool Window::isMinimized()
{
	FUNC_TRACE;

	return ::IsIconic( hwnd )!=FALSE;
}

bool Window::isActive()
{
	FUNC_TRACE;
	
	return active;
}

bool Window::isForeground()
{
	FUNC_TRACE;
	
	return hwnd==GetForegroundWindow();
}

void Window::getWindowRect( RECT * rect )
{
	FUNC_TRACE;
	
	if(::IsIconic(hwnd))
	{
		*rect = last_valid_window_rect;
	}
	else
	{
		::GetWindowRect(hwnd,rect);
	}
}

void Window::getNormalWindowRect( RECT * rect )
{
	FUNC_TRACE;

	WINDOWPLACEMENT window_place;
	memset( &window_place, 0, sizeof(window_place) );
	window_place.length = sizeof(window_place);
	::GetWindowPlacement( hwnd, &window_place );

	*rect = window_place.rcNormalPosition;
}

void Window::clear()
{
	FUNC_TRACE;

	{
		std::vector<Line*>::iterator i;
		for( i=char_buffer.begin() ; i!=char_buffer.end() ; i++ )
		{
			delete (*i);
		}
		char_buffer.clear();
	}

	{
		std::list<Plane*>::iterator i;
		for( i=plane_list.begin() ; i!=plane_list.end() ; i++ )
		{
			delete (*i);
		}
		plane_list.clear();
	}

    visible_rect.left = 0;
    visible_rect.right = width();
    visible_rect.top = 0;
    visible_rect.bottom = height();

    cursor_position.X = 0;
    cursor_position.Y = 0;

	RECT dirty_rect;
    GetClientRect( hwnd, &dirty_rect );
	appendDirtyRect( dirty_rect );
}

static inline void _putChar( Line * line, int pos, const Char & chr, bool * dirty )
{
	if( (int)line->size() <= pos )
	{
        line->push_back(chr);
        *dirty = true;
	}
	else
	{
		if( (*line)[pos] != chr )
		{
			(*line)[pos] = chr;
	        *dirty = true;
		}
	}
}

void Window::putString( int x, int y, int width, int height, const Attribute & attr, const wchar_t * str, int offset )
{
	FUNC_TRACE;
	
	if( x<0 || y<0 )
	{
		return;
	}

	bool dirty = false;

	while( (int)char_buffer.size() <= y )
	{
        Line * line = new Line();
        char_buffer.push_back(line);

        dirty = true;
	}

    Line * line = char_buffer[y];

	// 埋まっていなかったら空文字で進める
	while( (int)line->size() < x )
	{
        line->push_back( Char( L' ' ) );

        dirty = true;
	}

	int pos = x + offset;

    for( int i=0 ; str[i] ; i++ )
    {
		// いっぱいまで文字が埋まったら抜ける
    	if( pos + 1 > x + width )
    	{
    		break;
    	}

		// 全角文字を入れる隙間がなかったら、スペースを埋めて抜ける
    	if( zenkaku_table[str[i]] && pos + 2 > x + width )
    	{
	        _putChar( line, pos, Char( L' ', attr ), &dirty );
    		break;
    	}

		if( pos>=x )
		{
	        _putChar( line, pos, Char( str[i], attr ), &dirty );
			pos++;

	        // 全角文字は、幅を合わせるために、後ろに無駄な文字を入れる
	        if(zenkaku_table[str[i]])
	        {
		        _putChar( line, pos, Char(0), &dirty );
				pos++;
	        }
		}
		else
		{
			pos++;
	        if(zenkaku_table[str[i]])
	        {
	        	// 左端で全角文字の半分の位置から描画範囲に入る場合はスペースで埋める
	        	if( pos>=x )
	        	{
			        _putChar( line, pos, Char( L' ', attr ), &dirty );
	        	}
				pos++;
	        }
		}
    }

    if( dirty )
    {
    	//printf( "putString : dirty : %d, %d\n", x, y );

		RECT dirty_rect = { x*font_w+border_left, y*font_h+border_top, pos*font_w+border_left, (y+height)*font_h+border_top };
		appendDirtyRect( dirty_rect, false );
    }
}

void Window::setCursorPos( int x, int y )
{
	FUNC_TRACE;

	cursor_blink = 2;

	RECT dirty_rect = { (cursor_position.X-1)*font_w+border_left, cursor_position.Y*font_h+border_top, (cursor_position.X+1)*font_w+border_left, (cursor_position.Y+1)*font_h+border_top };
	appendDirtyRect( dirty_rect );

	cursor_position.X = x;
	cursor_position.Y = y;
	
	RECT dirty_rect2 = { (cursor_position.X-1)*font_w+border_left, cursor_position.Y*font_h+border_top, (cursor_position.X+1)*font_w+border_left, (cursor_position.Y+1)*font_h+border_top };
	appendDirtyRect( dirty_rect2 );
}

void Window::getCursorPos( int * x, int * y )
{
	FUNC_TRACE;

	*x = cursor_position.X;
	*y = cursor_position.Y;
}

void Window::setImeRect( const RECT & rect )
{
	FUNC_TRACE;

	ime_rect = rect;
}

void Window::enableIme( bool enable )
{
	FUNC_TRACE;

	if(enable)
	{
		if(ime_context)
		{
			ImmSetOpenStatus( ime_context, FALSE );
            ImmSetCompositionFontW( ime_context, &logfont );
			ImmAssociateContext( hwnd, ime_context );
			ime_context = (HIMC)NULL;
		}
	}
	else
	{
		if(!ime_context)
		{
			ime_context = ImmAssociateContext( hwnd, (HIMC)NULL );
		}
	}
}

void Window::updateImeFont()
{
	FUNC_TRACE;

	if(ime_context)
	{
        ImmSetCompositionFontW( ime_context, &logfont );
	}
	else
	{
		ime_context = ImmAssociateContext( hwnd, (HIMC)NULL );
        ImmSetCompositionFontW( ime_context, &logfont );
		ImmAssociateContext( hwnd, ime_context );
		ime_context = (HIMC)NULL;
	}
}

int Window::getStringWidth( const wchar_t * str, int tab_width, int offset, int columns[] )
{
	FUNC_TRACE;

	int width = 0;

	int i;
    for( i=0 ; str[i] ; i++ )
    {
    	if(columns)
    	{
    		columns[i] = width;
    	}

		if(str[i]=='\t')
		{
			width = width + (tab_width - width % tab_width);
			continue;
		}
    
		width++;

        if(zenkaku_table[str[i]])
        {
			width++;
        }
    }

	if(columns)
	{
		columns[i] = width;
	}

	return width;
}

// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------

TaskTrayIcon::Param::Param()
{
	FUNC_TRACE;

    lbuttondown_handler = NULL;
    lbuttonup_handler = NULL;
    rbuttondown_handler = NULL;
    rbuttonup_handler = NULL;
    lbuttondoubleclick_handler = NULL;
}

TaskTrayIcon::TaskTrayIcon( Param & param )
	:
	pyobj(NULL)
{
    lbuttondown_handler = param.lbuttondown_handler; Py_XINCREF(lbuttondown_handler);
    lbuttonup_handler = param.lbuttonup_handler; Py_XINCREF(lbuttonup_handler);
    rbuttondown_handler = param.rbuttondown_handler; Py_XINCREF(rbuttondown_handler);
    rbuttonup_handler = param.rbuttonup_handler; Py_XINCREF(rbuttonup_handler);
    lbuttondoubleclick_handler = param.lbuttondoubleclick_handler; Py_XINCREF(lbuttondoubleclick_handler);

    HINSTANCE hInstance = GetModuleHandle(NULL);

	hwnd = CreateWindow( TASKTRAY_WINDOW_CLASS_NAME.c_str(), param.title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, this );

	memset( &icon_data, 0, sizeof(icon_data) );
	icon_data.cbSize = sizeof( NOTIFYICONDATA );
	icon_data.uID = 0;
	icon_data.hWnd = hwnd;
	icon_data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	icon_data.hIcon = (HICON)LoadImage( hInstance, (const wchar_t*)1, IMAGE_ICON, 16, 16, 0 );
	icon_data.uCallbackMessage = WM_USER_NTFYICON;
	lstrcpy( icon_data.szTip, param.title.c_str() );
	
	// Shell_NotifyIcon にはリトライが必要
	// http://support.microsoft.com/kb/418138/JA/
	while(true)
	{
		if( ! Shell_NotifyIcon( NIM_ADD, &icon_data ) )
		{
			int err = GetLastError();
			if( err==ERROR_TIMEOUT || err==0 )
			{
		        printf("TaskTrayIcon : Shell_NotifyIcon(NIM_ADD) failed : %d\n", err );
		        printf("retry\n");
		        Sleep(1000);
		        
		        // タイムアウト後に実は成功していたかもしれないので、念のため確認する
		        if( Shell_NotifyIcon( NIM_MODIFY , &icon_data ) )
		        {
			        printf("TaskTrayIcon : Shell_NotifyIcon(NIM_MODIFY) succeeded\n" );
					break;
		        }
		        
		        continue;
			}
			else
			{
		        printf("TaskTrayIcon : Shell_NotifyIcon failed : %d\n", err );
			}
		}
		break;
	}

	ShowWindow(hwnd, SW_HIDE);
	UpdateWindow(hwnd);
}

TaskTrayIcon::~TaskTrayIcon()
{
	Shell_NotifyIcon( NIM_DELETE, &icon_data );

    Py_XDECREF(lbuttondown_handler); lbuttondown_handler=NULL;
    Py_XDECREF(lbuttonup_handler); lbuttonup_handler=NULL;
    Py_XDECREF(rbuttondown_handler); rbuttondown_handler=NULL;
    Py_XDECREF(rbuttonup_handler); rbuttonup_handler=NULL;
    Py_XDECREF(lbuttondoubleclick_handler); lbuttondoubleclick_handler=NULL;
}

void TaskTrayIcon::SetPyObject( PyObject * _pyobj )
{
	FUNC_TRACE;

	if( pyobj != _pyobj )
	{
		Py_XDECREF(pyobj);
		pyobj = _pyobj;
		Py_XINCREF(pyobj);
	}
}

void TaskTrayIcon::destroy()
{
	FUNC_TRACE;
	
	_clearPopupMenuCommands();

    DestroyWindow(hwnd);
}

bool TaskTrayIcon::_registerWindowClass()
{
	FUNC_TRACE;

    HINSTANCE hInstance = GetModuleHandle(NULL);

    WNDCLASSEX wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = _wndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = 0;
    wc.hCursor = 0;
    wc.hbrBackground = CreateSolidBrush( RGB(0x00, 0x00, 0x00) );
    wc.lpszMenuName = NULL;
    wc.lpszClassName = TASKTRAY_WINDOW_CLASS_NAME.c_str();
    wc.hIconSm = 0;

    if(! RegisterClassEx(&wc))
    {
        printf("RegisterClassEx failed\n");
        return(FALSE);
    }

    return(TRUE);
}

void TaskTrayIcon::_clearPopupMenuCommands()
{
	for( int i=0 ; i<(int)popup_menu_commands.size() ; ++i )
	{
		Py_XDECREF(popup_menu_commands[i]);
	}
	popup_menu_commands.clear();
}

LRESULT CALLBACK TaskTrayIcon::_wndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    TaskTrayIcon * task_tray_icon = (TaskTrayIcon*)GetProp( hwnd, L"ckit_userdata" );

	PythonUtil::GIL_Ensure gil_ensure;

	switch(msg)
	{
    case WM_CREATE:
        {
            CREATESTRUCT * create_data = (CREATESTRUCT*)lp;
            TaskTrayIcon * task_tray_icon = (TaskTrayIcon*)create_data->lpCreateParams;
            task_tray_icon->hwnd = hwnd;
            SetProp( hwnd, L"ckit_userdata", task_tray_icon );
        }
        break;

    case WM_DESTROY:
        {
            RemoveProp( hwnd, L"ckit_userdata" );
            delete task_tray_icon;
        }
        break;

	case WM_COMMAND:
		{
			int wmId    = LOWORD(wp);
			int wmEvent = HIWORD(wp);
			
			if( ID_POPUP_MENUITEM<=wmId && wmId<ID_POPUP_MENUITEM+(int)task_tray_icon->popup_menu_commands.size() )
			{
				PyObject * func = task_tray_icon->popup_menu_commands[wmId-ID_POPUP_MENUITEM];
				if(func)
				{
					PyObject * command_info;
					if(command_info_constructor)
					{
						PyObject * pyarglist2 = Py_BuildValue( "()" );
						command_info = PyEval_CallObject( command_info_constructor, pyarglist2 );
						Py_DECREF(pyarglist2);
						if(!command_info)
						{
							PyErr_Print();
							break;
						}
					}
					else
					{
						command_info = Py_None;
						Py_INCREF(command_info);
					}
				
					PyObject * pyarglist = Py_BuildValue( "(O)", command_info );
					Py_DECREF(command_info);
					PyObject * pyresult = PyEval_CallObject( func, pyarglist );
					Py_DECREF(pyarglist);
					if(pyresult)
					{
						Py_DECREF(pyresult);
					}
					else
					{
						PyErr_Print();
					}
				}
			}
			else
			{			
				return DefWindowProc( hwnd, msg, wp, lp);
			}
		}
		break;

	case WM_USER_NTFYICON:
		{
			switch(lp)
			{
			case WM_LBUTTONDOWN:
				if(task_tray_icon->lbuttondown_handler)
				{
					int mod = Window::_getModKey();

					PyObject * pyarglist = Py_BuildValue("(iii)", (short)LOWORD(lp), (short)HIWORD(lp), mod );
					PyObject * pyresult = PyEval_CallObject( task_tray_icon->lbuttondown_handler, pyarglist );
					Py_DECREF(pyarglist);
					if(pyresult)
					{
						Py_DECREF(pyresult);
					}
					else
					{
						PyErr_Print();
					}
				}
				break;

			case WM_LBUTTONUP:
				if(task_tray_icon->lbuttonup_handler)
				{
					int mod = Window::_getModKey();

					PyObject * pyarglist = Py_BuildValue("(iii)", (short)LOWORD(lp), (short)HIWORD(lp), mod );
					PyObject * pyresult = PyEval_CallObject( task_tray_icon->lbuttonup_handler, pyarglist );
					Py_DECREF(pyarglist);
					if(pyresult)
					{
						Py_DECREF(pyresult);
					}
					else
					{
						PyErr_Print();
					}
				}
				break;

			case WM_RBUTTONDOWN:
				if(task_tray_icon->rbuttondown_handler)
				{
					int mod = Window::_getModKey();

					PyObject * pyarglist = Py_BuildValue("(iii)", (short)LOWORD(lp), (short)HIWORD(lp), mod );
					PyObject * pyresult = PyEval_CallObject( task_tray_icon->rbuttondown_handler, pyarglist );
					Py_DECREF(pyarglist);
					if(pyresult)
					{
						Py_DECREF(pyresult);
					}
					else
					{
						PyErr_Print();
					}
				}
				break;

			case WM_RBUTTONUP:
				if(task_tray_icon->rbuttonup_handler)
				{
					int mod = Window::_getModKey();

					PyObject * pyarglist = Py_BuildValue("(iii)", (short)LOWORD(lp), (short)HIWORD(lp), mod );
					PyObject * pyresult = PyEval_CallObject( task_tray_icon->rbuttonup_handler, pyarglist );
					Py_DECREF(pyarglist);
					if(pyresult)
					{
						Py_DECREF(pyresult);
					}
					else
					{
						PyErr_Print();
					}
				}
				break;

			case WM_LBUTTONDBLCLK:
				if(task_tray_icon->lbuttondoubleclick_handler)
				{
					int mod = Window::_getModKey();

					PyObject * pyarglist = Py_BuildValue("(iii)", (short)LOWORD(lp), (short)HIWORD(lp), mod );
					PyObject * pyresult = PyEval_CallObject( task_tray_icon->lbuttondoubleclick_handler, pyarglist );
					Py_DECREF(pyarglist);
					if(pyresult)
					{
						Py_DECREF(pyresult);
					}
					else
					{
						PyErr_Print();
					}
				}
				break;
			}
		}
		break;

	case WM_TIMER:
		return 0;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			EndPaint(hwnd, &ps);
		}
		break;

	default:
		return DefWindowProc(hwnd, msg, wp, lp);
	}
	return 0;
}

// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------

static int Attribute_init( PyObject * self, PyObject * args, PyObject * kwds)
{
	FUNC_TRACE;

	PyObject * fg = NULL;
	PyObject * bg = NULL;
	PyObject * bg_gradation = NULL;
	PyObject * line0 = NULL;
	PyObject * line1 = NULL;

    static char * kwlist[] = {
        "fg",
        "bg",
        "bg_gradation",
        "line0",
        "line1",
        NULL
    };

    if(!PyArg_ParseTupleAndKeywords( args, kwds, "|OOOOO", kwlist,
        &fg,
        &bg,
        &bg_gradation,
        &line0,
        &line1
    ))
    {
        return -1;
    }

	if( fg && PySequence_Check(fg) )
	{
		int r, g, b;
	    if( ! PyArg_ParseTuple( fg, "iii", &r, &g, &b ) )
	        return -1;

	    ((Attribute_Object*)self)->attr.fg_color = RGB(r,g,b);
	}

	if( bg_gradation && PySequence_Check(bg_gradation) )
	{
		int r[4],g[4],b[4];

	    if( ! PyArg_ParseTuple(
	    	bg_gradation,
	    	"(iii)(iii)(iii)(iii)",
	    	&r[0], &g[0], &b[0],
	    	&r[1], &g[1], &b[1],
	    	&r[2], &g[2], &b[2],
	    	&r[3], &g[3], &b[3] ) )
	   	{
	        return -1;
	    }

	    ((Attribute_Object*)self)->attr.bg = Attribute::BG_Gradation;
	    ((Attribute_Object*)self)->attr.bg_color[0] = RGB(r[0],g[0],b[0]);
	    ((Attribute_Object*)self)->attr.bg_color[1] = RGB(r[1],g[1],b[1]);
	    ((Attribute_Object*)self)->attr.bg_color[2] = RGB(r[2],g[2],b[2]);
	    ((Attribute_Object*)self)->attr.bg_color[3] = RGB(r[3],g[3],b[3]);
	}
	else if( bg && PySequence_Check(bg) )
	{
		int r, g, b;
	    if( ! PyArg_ParseTuple( bg, "iii", &r, &g, &b ) )
	        return -1;

	    ((Attribute_Object*)self)->attr.bg = Attribute::BG_Flat;
	    ((Attribute_Object*)self)->attr.bg_color[0] = RGB(r,g,b);
	}

	if( line0 && PySequence_Check(line0) )
	{
		int line;
		int r, g, b;
	    if( ! PyArg_ParseTuple( line0, "i(iii)", &line, &r, &g, &b ) )
	        return -1;

	    ((Attribute_Object*)self)->attr.line[0] = line;
	    ((Attribute_Object*)self)->attr.line_color[0] = RGB(r,g,b);
	}

	if( line1 && PySequence_Check(line1) )
	{
		int line;
		int r, g, b;
	    if( ! PyArg_ParseTuple( line1, "i(iii)", &line, &r, &g, &b ) )
	        return -1;

	    ((Attribute_Object*)self)->attr.line[1] = line;
	    ((Attribute_Object*)self)->attr.line_color[1] = RGB(r,g,b);
	}

    return 0;
}

static void Attribute_dealloc(PyObject* self)
{
	FUNC_TRACE;

    self->ob_type->tp_free(self);
}

static PyMethodDef Attribute_methods[] = {
    {NULL,NULL}
};

PyTypeObject Attribute_Type = {
    PyVarObject_HEAD_INIT(NULL,0)
    "Attribute",		/* tp_name */
    sizeof(Attribute_Object), /* tp_basicsize */
    0,					/* tp_itemsize */
    (destructor)Attribute_dealloc,/* tp_dealloc */
    0,					/* tp_print */
    0,					/* tp_getattr */
    0,					/* tp_setattr */
    0,					/* tp_reserved */
    0, 					/* tp_repr */
    0,					/* tp_as_number */
    0,					/* tp_as_sequence */
    0,					/* tp_as_mapping */
    0,					/* tp_hash */
    0,					/* tp_call */
    0,					/* tp_str */
    PyObject_GenericGetAttr,/* tp_getattro */
    PyObject_GenericSetAttr,/* tp_setattro */
    0,					/* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,/* tp_flags */
    "",					/* tp_doc */
    0,					/* tp_traverse */
    0,					/* tp_clear */
    0,					/* tp_richcompare */
    0,					/* tp_weaklistoffset */
    0,					/* tp_iter */
    0,					/* tp_iternext */
    Attribute_methods,	/* tp_methods */
    0,					/* tp_members */
    0,					/* tp_getset */
    0, 					/* tp_base */
    0,					/* tp_dict */
    0,					/* tp_descr_get */
    0,					/* tp_descr_set */
    0,					/* tp_dictoffset */
    Attribute_init,		/* tp_init */
    0,					/* tp_alloc */
    PyType_GenericNew,	/* tp_new */
    0,					/* tp_free */
};

// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------

static void Image_dealloc(PyObject* self)
{
	FUNC_TRACE;

    Image * image = ((Image_Object*)self)->p;
    image->Release();
	self->ob_type->tp_free(self);
}

static PyObject * Image_getSize(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;
	return Py_BuildValue( "(ii)", ((Image_Object*)self)->p->width, ((Image_Object*)self)->p->height );
}

static void _Image_ConvertRGBAtoDIB( char * dst, const char * src, int w, int h, int wb )
{
	FUNC_TRACE;

	for( int y=0 ; y<h ; y++ )
	{
		for( int x=0 ; x<w ; x++ )
		{
			int src_y = h-y-1;
			dst[ y*w*4 + x*4 + 0 ] = src[ src_y*wb + x*4 + 2 ];
			dst[ y*w*4 + x*4 + 1 ] = src[ src_y*wb + x*4 + 1 ];
			dst[ y*w*4 + x*4 + 2 ] = src[ src_y*wb + x*4 + 0 ];
			dst[ y*w*4 + x*4 + 3 ] = src[ src_y*wb + x*4 + 3 ];
		}
	}
}

static PyObject * Image_fromString( PyObject * self, PyObject * args )
{
	FUNC_TRACE;

	int width;
	int height;
	const char * buf;
	unsigned int bufsize;
	PyObject * py_transparent_color = NULL;
	int halftone=0;

	if( ! PyArg_ParseTuple(args,"(ii)s#|Oi", &width, &height, &buf, &bufsize, &py_transparent_color, &halftone ) )
		return NULL;
		
	if( py_transparent_color==Py_None )
	{
		py_transparent_color=NULL;
	}

	if( (unsigned)(width * 4 * height) > bufsize )
	{
		PyErr_SetString( PyExc_ValueError, "insufficient buffer length." );
		return NULL;
	}

	COLORREF transparent_color = RGB(0,0,0);
	if(py_transparent_color)
	{
		int r, g, b;
	    if( ! PyArg_ParseTuple( py_transparent_color, "iii", &r, &g, &b ) )
	        return NULL;

	    transparent_color = RGB(r,g,b);
	}

	Image * image;

	if( width<=0 || height<=0 )
	{
		image = new Image( 0, 0, NULL, false );
	}
	else
	{
		void * dib_buf = malloc( width * 4 * height );

		_Image_ConvertRGBAtoDIB( (char*)dib_buf, (const char*)buf, width, height, width*4 );

		image = new Image( width, height, (const char *)dib_buf, py_transparent_color ? &transparent_color : 0, halftone!=0 );

		free(dib_buf);
	}

	Image_Object * pyimg;
	pyimg = PyObject_New( Image_Object, &Image_Type );
	pyimg->p = image;
	image->AddRef();

	return (PyObject*)pyimg;
}

static PyMethodDef Image_methods[] = {
	{ "getSize", Image_getSize, METH_VARARGS, "" },
	{ "fromString", Image_fromString, METH_STATIC|METH_VARARGS, "" },
	{NULL,NULL}
};

PyTypeObject Image_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Image",			/* tp_name */
	sizeof(Image_Object), /* tp_basicsize */
	0,					/* tp_itemsize */
	Image_dealloc,		/* tp_dealloc */
	0,					/* tp_print */
	0,					/* tp_getattr */
	0,					/* tp_setattr */
	0,					/* tp_reserved */
	0, 					/* tp_repr */
	0,					/* tp_as_number */
	0,					/* tp_as_sequence */
	0,					/* tp_as_mapping */
	0,					/* tp_hash */
	0,					/* tp_call */
	0,					/* tp_str */
	PyObject_GenericGetAttr,/* tp_getattro */
	PyObject_GenericSetAttr,/* tp_setattro */
	0,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,/* tp_flags */
	"",					/* tp_doc */
	0,					/* tp_traverse */
	0,					/* tp_clear */
	0,					/* tp_richcompare */
	0,					/* tp_weaklistoffset */
	0,					/* tp_iter */
	0,					/* tp_iternext */
	Image_methods,		/* tp_methods */
	0,					/* tp_members */
	0,					/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
	0,					/* tp_dictoffset */
	0,					/* tp_init */
	0,					/* tp_alloc */
	PyType_GenericNew,	/* tp_new */
	0,					/* tp_free */
};

// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------

static void Icon_dealloc(PyObject* self)
{
	FUNC_TRACE;

    Icon * icon = ((Icon_Object*)self)->p;
    icon->Release();
	self->ob_type->tp_free(self);
}

static PyObject * Icon_fromFilename( PyObject * self, PyObject * args )
{
	FUNC_TRACE;

	PyObject * pyfilename;

	if( ! PyArg_ParseTuple(args,"O", &pyfilename ) )
		return NULL;

    std::wstring filename;
    if(pyfilename)
    {
        if( !PythonUtil::PyStringToWideString( pyfilename, &filename ) )
        {
            return NULL;
        }
    }

	Icon * icon = new Icon( filename.c_str() );;

	Icon_Object * pyicon;
	pyicon = PyObject_New( Icon_Object, &Icon_Type );
	pyicon->p = icon;
	icon->AddRef();

	return (PyObject*)pyicon;
}

static PyMethodDef Icon_methods[] = {
	{ "fromFilename", Icon_fromFilename, METH_STATIC|METH_VARARGS, "" },
	{NULL,NULL}
};

PyTypeObject Icon_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Icon",				/* tp_name */
	sizeof(Icon_Object), /* tp_basicsize */
	0,					/* tp_itemsize */
	Icon_dealloc,		/* tp_dealloc */
	0,					/* tp_print */
	0,					/* tp_getattr */
	0,					/* tp_setattr */
	0,					/* tp_reserved */
	0, 					/* tp_repr */
	0,					/* tp_as_number */
	0,					/* tp_as_sequence */
	0,					/* tp_as_mapping */
	0,					/* tp_hash */
	0,					/* tp_call */
	0,					/* tp_str */
	PyObject_GenericGetAttr,/* tp_getattro */
	PyObject_GenericSetAttr,/* tp_setattro */
	0,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,/* tp_flags */
	"",					/* tp_doc */
	0,					/* tp_traverse */
	0,					/* tp_clear */
	0,					/* tp_richcompare */
	0,					/* tp_weaklistoffset */
	0,					/* tp_iter */
	0,					/* tp_iternext */
	Icon_methods,		/* tp_methods */
	0,					/* tp_members */
	0,					/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
	0,					/* tp_dictoffset */
	0,					/* tp_init */
	0,					/* tp_alloc */
	PyType_GenericNew,	/* tp_new */
	0,					/* tp_free */
};

// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------

static int Plane_instance_count = 0;

static int Plane_init( PyObject * self, PyObject * args, PyObject * kwds)
{
	FUNC_TRACE;

	Plane_instance_count ++;
	PRINTF("Plane_instance_count=%d\n", Plane_instance_count);

    PyObject * window;
    int x, y, width, height;
    float priority;

    if(!PyArg_ParseTuple( args, "O(ii)(ii)f",
    	&window,
        &x,
        &y,
        &width,
        &height,
        &priority
    ))
    {
        return -1;
    }

    Plane * plane = new Plane( ((Window_Object*)window)->p, x, y, width, height, priority );

    plane->SetPyObject(self);

    ((Plane_Object*)self)->p = plane;

	std::list<Plane*>::iterator i;
	std::list<Plane*> & plane_list = ((Window_Object*)window)->p->plane_list;
	for( i=plane_list.begin() ; i!=plane_list.end() ; i++ )
	{
		if( (*i)->priority <= priority )
		{
			break;
		}
	}
    plane_list.insert( i, plane );

    return 0;
}

static void Plane_dealloc(PyObject* self)
{
	FUNC_TRACE;

	Plane_instance_count --;
	PRINTF("Plane_instance_count=%d\n", Plane_instance_count);

    Plane * plane = ((Plane_Object*)self)->p;
	if(plane){ plane->SetPyObject(NULL); }
	self->ob_type->tp_free(self);
}

static PyObject * Plane_destroy(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Plane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	((Plane_Object*)self)->p->window->plane_list.remove( ((Plane_Object*)self)->p );

	delete ((Plane_Object*)self)->p;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Plane_show(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	int show;
	if( ! PyArg_ParseTuple(args, "i", &show ) )
        return NULL;

	if( ! ((Plane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    ((Plane_Object*)self)->p->Show( show!=0 );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Plane_getPos(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Plane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	return Py_BuildValue( "(ii)", ((Plane_Object*)self)->p->x, ((Plane_Object*)self)->p->y );
}

static PyObject * Plane_setPos(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	int x, y;
	if( ! PyArg_ParseTuple(args, "(ii)", &x, &y ) )
        return NULL;

	if( ! ((Plane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    ((Plane_Object*)self)->p->SetPos( x, y );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Plane_getSize(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Plane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	return Py_BuildValue( "(ii)", ((Plane_Object*)self)->p->width, ((Plane_Object*)self)->p->height );
}

static PyObject * Plane_setSize(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	int width, height;
	if( ! PyArg_ParseTuple(args, "(ii)", &width, &height ) )
        return NULL;

	if( ! ((Plane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    ((Plane_Object*)self)->p->SetSize( width, height );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Plane_getPriority(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Plane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	return Py_BuildValue( "f", ((Plane_Object*)self)->p->priority );
}

static PyObject * Plane_setPriority(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	float priority;
	if( ! PyArg_ParseTuple(args, "f", &priority ) )
        return NULL;

	if( ! ((Plane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    ((Plane_Object*)self)->p->SetPriority( priority );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Plane_getImage(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Plane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Image * image = ((Plane_Object*)self)->p->image;
    if(image)
    {
		Image_Object * pyimg;
		pyimg = PyObject_New( Image_Object, &Image_Type );
		pyimg->p = image;
		image->AddRef();
		return (PyObject*)pyimg;
    }
    else
    {
	    Py_INCREF(Py_None);
	    return Py_None;
    }
}

static PyObject * Plane_setImage(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

    PyObject * image;
	if( ! PyArg_ParseTuple(args, "O", &image ) )
        return NULL;

	if( ! ((Plane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    ((Plane_Object*)self)->p->SetImage( ((Image_Object*)image)->p );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Plane_getIcon(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Plane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Icon * icon = ((Plane_Object*)self)->p->icon;
    if(icon)
    {
		Icon_Object * pyicon;
		pyicon = PyObject_New( Icon_Object, &Icon_Type );
		pyicon->p = icon;
		icon->AddRef();
		return (PyObject*)pyicon;
    }
    else
    {
	    Py_INCREF(Py_None);
	    return Py_None;
    }
}

static PyObject * Plane_setIcon(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

    PyObject * icon;
	if( ! PyArg_ParseTuple(args, "O", &icon ) )
        return NULL;

	if( ! ((Plane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    ((Plane_Object*)self)->p->SetIcon( ((Icon_Object*)icon)->p );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef Plane_methods[] = {

	{ "destroy", Plane_destroy, METH_VARARGS, "" },

	{ "show", Plane_show, METH_VARARGS, "" },

	{ "getPos", Plane_getPos, METH_VARARGS, "" },
	{ "getSize", Plane_getSize, METH_VARARGS, "" },
	{ "getPriority", Plane_getPriority, METH_VARARGS, "" },
	{ "getImage", Plane_getImage, METH_VARARGS, "" },
	{ "getIcon", Plane_getIcon, METH_VARARGS, "" },

	{ "setPos", Plane_setPos, METH_VARARGS, "" },
	{ "setSize", Plane_setSize, METH_VARARGS, "" },
	{ "setPriority", Plane_setPriority, METH_VARARGS, "" },
	{ "setImage", Plane_setImage, METH_VARARGS, "" },
	{ "setIcon", Plane_setIcon, METH_VARARGS, "" },

	{NULL,NULL}
};

PyTypeObject Plane_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Plane",			/* tp_name */
	sizeof(Plane_Object), /* tp_basicsize */
	0,					/* tp_itemsize */
	Plane_dealloc,		/* tp_dealloc */
	0,					/* tp_print */
	0,					/* tp_getattr */
	0,					/* tp_setattr */
	0,					/* tp_reserved */
	0, 					/* tp_repr */
	0,					/* tp_as_number */
	0,					/* tp_as_sequence */
	0,					/* tp_as_mapping */
	0,					/* tp_hash */
	0,					/* tp_call */
	0,					/* tp_str */
	PyObject_GenericGetAttr,/* tp_getattro */
	PyObject_GenericSetAttr,/* tp_setattro */
	0,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,/* tp_flags */
	"",					/* tp_doc */
	0,					/* tp_traverse */
	0,					/* tp_clear */
	0,					/* tp_richcompare */
	0,					/* tp_weaklistoffset */
	0,					/* tp_iter */
	0,					/* tp_iternext */
	Plane_methods,		/* tp_methods */
	0,					/* tp_members */
	0,					/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
	0,					/* tp_dictoffset */
	Plane_init,			/* tp_init */
	0,					/* tp_alloc */
	PyType_GenericNew,	/* tp_new */
	0,					/* tp_free */
};

// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------

static int MenuNode_instance_count = 0;

static int MenuNode_init( PyObject * self, PyObject * args, PyObject * kwds)
{
	FUNC_TRACE;
	
	MenuNode_instance_count ++;
	PRINTF("MenuNode_instance_count=%d\n", MenuNode_instance_count);

	PyObject * pystr_name = NULL;
	PyObject * pystr_text = NULL;
	PyObject * command = NULL;
	PyObject * visible = NULL;
	PyObject * enabled = NULL;
	PyObject * checked = NULL;
	PyObject * items = NULL;
	int separator = 0;

    static char * kwlist[] = {
        "name",
        "text",
        "command",
        "visible",
        "enabled",
        "checked",
        "items",
    	"separator",
        NULL
    };

    if(!PyArg_ParseTupleAndKeywords( args, kwds, "|OOOOOOOi", kwlist,
        &pystr_name,
        &pystr_text,
        &command,
        &visible,
        &enabled,
        &checked,
        &items,
        &separator
    ))
    {
        return -1;
    }

    std::wstring str_name;
    if(pystr_name)
    {
        if( !PythonUtil::PyStringToWideString( pystr_name, &str_name ) )
        {
            return -1;
        }
    }

    std::wstring str_text;
    if(pystr_text)
    {
        if( !PythonUtil::PyStringToWideString( pystr_text, &str_text ) )
        {
            return -1;
        }
    }

	MenuNode::Param param;
	if(separator){ param.separator = true; }
	param.name = str_name;
	param.text = str_text;
	param.command = command;
	param.visible = visible;
	param.enabled = enabled;
	param.checked = checked;
	param.items = items;

    MenuNode * menu_node = new MenuNode(param);
    ((MenuNode_Object*)self)->p = menu_node;
    menu_node->AddRef();

    return 0;
}

static void MenuNode_dealloc(PyObject* self)
{
	FUNC_TRACE;

	MenuNode_instance_count --;
	PRINTF("MenuNode_instance_count=%d\n", MenuNode_instance_count);

    MenuNode * menu_node = ((MenuNode_Object*)self)->p;
    menu_node->Release();
	self->ob_type->tp_free(self);
}

static PyObject * MenuNode_GetAttrString( PyObject * self, const char * attr_name )
{
	switch(attr_name[0])
	{
	case 'n':
		if( strcmp(attr_name,"name")==0 )
		{
			PyObject * value = Py_BuildValue( "u", ((MenuNode_Object*)self)->p->name.c_str() );
			return value;
		}
		break;

	case 'i':
		if( strcmp(attr_name,"items")==0 )
		{
			PyObject * value = ((MenuNode_Object*)self)->p->items;
			if(!value)
			{
				value = Py_None;
			}
			Py_INCREF(value);
			return value;
		}
		break;
	}
	
	PyErr_SetString( PyExc_AttributeError, attr_name );
	return NULL;
}

static int MenuNode_SetAttrString( PyObject * self, const char * attr_name, PyObject * value )
{
	switch(attr_name[0])
	{
	case 'n':
		if( strcmp(attr_name,"name")==0 )
		{
		    std::wstring name;
		    if( !PythonUtil::PyStringToWideString( value, &name ) )
		    {
				return -1;
		    }
			((MenuNode_Object*)self)->p->name = name;
			return 0;
		}
		break;
	
	case 'i':
		if( strcmp(attr_name,"items")==0 )
		{
			Py_XDECREF( ((MenuNode_Object*)self)->p->items );
			((MenuNode_Object*)self)->p->items = value;
			Py_XINCREF( ((MenuNode_Object*)self)->p->items );
			return 0;
		}
		break;
	}
	
	PyErr_SetString( PyExc_AttributeError, attr_name );
	return -1;
}

static PyMethodDef MenuNode_methods[] = {
	{NULL,NULL}
};

PyTypeObject MenuNode_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"MenuNode",		/* tp_name */
	sizeof(MenuNode_Object), /* tp_basicsize */
	0,					/* tp_itemsize */
	MenuNode_dealloc,		/* tp_dealloc */
	0,					/* tp_print */
	(getattrfunc)MenuNode_GetAttrString,/* tp_getattr */
	(setattrfunc)MenuNode_SetAttrString,/* tp_setattr */
	0,					/* tp_reserved */
	0, 					/* tp_repr */
	0,					/* tp_as_number */
	0,					/* tp_as_sequence */
	0,					/* tp_as_mapping */
	0,					/* tp_hash */
	0,					/* tp_call */
	0,					/* tp_str */
	0,					/* tp_getattro */
	0,					/* tp_setattro */
	0,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,/* tp_flags */
	"",					/* tp_doc */
	0,					/* tp_traverse */
	0,					/* tp_clear */
	0,					/* tp_richcompare */
	0,					/* tp_weaklistoffset */
	0,					/* tp_iter */
	0,					/* tp_iternext */
	MenuNode_methods,	/* tp_methods */
	0,					/* tp_members */
	0,					/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
	0,					/* tp_dictoffset */
	MenuNode_init,	/* tp_init */
	0,					/* tp_alloc */
	PyType_GenericNew,	/* tp_new */
	0,					/* tp_free */
};

// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------

static int Window_instance_count = 0;

static int Window_init( PyObject * self, PyObject * args, PyObject * kwds)
{
	FUNC_TRACE;

	Window_instance_count ++;
	PRINTF("Window_instance_count=%d\n", Window_instance_count);

    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int width = 80;
    int height = 24;
    int origin = 0;
    PyObject * parent_window = NULL;
    PyObject * pystr_font_name = NULL;
    int font_size = 12;
    PyObject * pybg_color = NULL;
    PyObject * pyframe_color = NULL;
    PyObject * pycursor0_color = NULL;
    PyObject * pycursor1_color = NULL;
    int border_size = 1;
    PyObject * pytransparency = NULL;
    PyObject * pytransparent_color = NULL;
    int title_bar = 1;
    PyObject * pystr_title = NULL;
	int show = 1;
    int resizable = 1;
    int noframe = 0;
    int minimizebox = 1;
    int maximizebox = 1;
    int cursor = 0;
    int sysmenu = 0;
    int tool = 0;
    int ncpaint = 0;
    PyObject * activate_handler = NULL;
    PyObject * close_handler = NULL;
    PyObject * endsession_handler = NULL;
    PyObject * move_handler = NULL;
    PyObject * size_handler = NULL;
    PyObject * dropfiles_handler = NULL;
    PyObject * ipc_handler = NULL;
    PyObject * keydown_handler = NULL;
    PyObject * keyup_handler = NULL;
    PyObject * char_handler = NULL;
    PyObject * lbuttondown_handler = NULL;
    PyObject * lbuttonup_handler = NULL;
    PyObject * mbuttondown_handler = NULL;
    PyObject * mbuttonup_handler = NULL;
    PyObject * rbuttondown_handler = NULL;
    PyObject * rbuttonup_handler = NULL;
    PyObject * lbuttondoubleclick_handler = NULL;
    PyObject * mbuttondoubleclick_handler = NULL;
    PyObject * rbuttondoubleclick_handler = NULL;
    PyObject * mousemove_handler = NULL;
    PyObject * mousewheel_handler = NULL;
    PyObject * nchittest_handler = NULL;

    static char * kwlist[] = {

        "x",
        "y",
        "width",
        "height",
        "origin",

        "parent_window",
        "font_name",
        "font_size",
        "bg_color",
        "frame_color",
        "cursor0_color",
        "cursor1_color",

        "border_size",
        "transparency",
        "transparent_color",
        "title_bar",
        "title",

        "show",
        "resizable",
        "noframe",
        "minimizebox",
        "maximizebox",
        "cursor",
        "sysmenu",
        "tool",
        "ncpaint",

        "activate_handler",
        "close_handler",
        "endsession_handler",
        "move_handler",
        "size_handler",
        "dropfiles_handler",
        "ipc_handler",
        "keydown_handler",
        "keyup_handler",
        "char_handler",
        "lbuttondown_handler",
        "lbuttonup_handler",
        "mbuttondown_handler",
        "mbuttonup_handler",
        "rbuttondown_handler",
        "rbuttonup_handler",
        "lbuttondoubleclick_handler",
        "mbuttondoubleclick_handler",
        "rbuttondoubleclick_handler",
        "mousemove_handler",
        "mousewheel_handler",
        "nchittest_handler",
        NULL
    };

    if(!PyArg_ParseTupleAndKeywords( args, kwds, 
    	"|iiiii"
    	"OOiOOOO"
    	"iOOiO"
    	"iiiiiiiii"
    	"OOOOOOOOOOOOOOOOOOOOOO", kwlist,

        &x,
        &y,
        &width,
        &height,
        &origin,

        &parent_window,
        &pystr_font_name,
        &font_size,
        &pybg_color,
        &pyframe_color,
        &pycursor0_color,
        &pycursor1_color,

        &border_size,
        &pytransparency,
        &pytransparent_color,
        &title_bar,
        &pystr_title,

        &show,
        &resizable,
        &noframe,
        &minimizebox,
        &maximizebox,
        &cursor,
        &sysmenu,
        &tool,
        &ncpaint,

	    &activate_handler,
	    &close_handler,
	    &endsession_handler,
	    &move_handler,
	    &size_handler,
	    &dropfiles_handler,
	    &ipc_handler,
	    &keydown_handler,
	    &keyup_handler,
	    &char_handler,
        &lbuttondown_handler,
        &lbuttonup_handler,
        &mbuttondown_handler,
        &mbuttonup_handler,
        &rbuttondown_handler,
        &rbuttonup_handler,
        &lbuttondoubleclick_handler,
        &mbuttondoubleclick_handler,
        &rbuttondoubleclick_handler,
        &mousemove_handler,
        &mousewheel_handler,
        &nchittest_handler
    ))
    {
        return -1;
    }

	if( parent_window!=0 && !PyLong_Check(parent_window) && !Window_Check(parent_window) )
	{
		PyErr_SetString( PyExc_TypeError, "must be HWND or Window object." );
        return -1;
	}

    std::wstring str_font_name;
    if(pystr_font_name)
    {
        if( !PythonUtil::PyStringToWideString( pystr_font_name, &str_font_name ) )
        {
            return -1;
        }
    }

    std::wstring str_title;
    if(pystr_title)
    {
        if( !PythonUtil::PyStringToWideString( pystr_title, &str_title ) )
        {
            return -1;
        }
    }

    Window::Param param;
    param.is_winpos_given = true;
    param.winpos_x = x;
    param.winpos_y = y;
    param.winsize_char_w = width;
    param.winsize_char_h = height;
    param.origin = origin;
    if(!parent_window)
    {
    	param.parent_window = NULL;
    	param.parent_window_hwnd = NULL;
    }
    else if(PyLong_Check(parent_window))
    {
    	param.parent_window = NULL;
    	param.parent_window_hwnd = (HWND)PyLong_AS_LONG(parent_window);
    }
    else
    {
    	param.parent_window = ((Window_Object*)parent_window)->p;
    	param.parent_window_hwnd = param.parent_window->hwnd;
    }
    if(pystr_font_name) param.font_name = str_font_name;
    param.font_size = font_size;
	if(pybg_color)
	{
		int r, g, b;
	    if( ! PyArg_ParseTuple( pybg_color, "iii", &r, &g, &b ) )
	        return -1;
	    param.bg_color = RGB(r,g,b);
	}
	if(pyframe_color)
	{
		int r, g, b;
	    if( ! PyArg_ParseTuple( pyframe_color, "iii", &r, &g, &b ) )
	        return -1;
	    param.frame_color = RGB(r,g,b);
	}
	if(pycursor0_color)
	{
		int r, g, b;
	    if( ! PyArg_ParseTuple( pycursor0_color, "iii", &r, &g, &b ) )
	        return -1;
	    param.cursor0_color = RGB(r,g,b);
	}
	if(pycursor1_color)
	{
		int r, g, b;
	    if( ! PyArg_ParseTuple( pycursor1_color, "iii", &r, &g, &b ) )
	        return -1;
	    param.cursor1_color = RGB(r,g,b);
	}
    param.border_size = border_size;

	if(pytransparency && PyLong_Check(pytransparency))
	{
	    param.transparency = PyLong_AS_LONG(pytransparency);
	    param.is_transparency_given = true;
	}

	if(pytransparent_color)
	{
		int r, g, b;
	    if( ! PyArg_ParseTuple( pytransparent_color, "iii", &r, &g, &b ) )
	        return -1;
	    param.transparent_color = RGB(r,g,b);
	    param.is_transparent_color_given = true;
	}
    param.title_bar = title_bar!=0;
    param.title = str_title;
    param.show = show!=0;
    param.resizable = resizable!=0;
    param.noframe = noframe!=0;
    param.minimizebox = minimizebox!=0;
    param.minimizebox = minimizebox!=0;
    param.cursor = cursor!=0;
    param.sysmenu = sysmenu!=0;
    param.tool = tool!=0;
    param.ncpaint = ncpaint!=0;
    param.activate_handler = activate_handler;
    param.close_handler = close_handler;
    param.endsession_handler = endsession_handler;
    param.move_handler = move_handler;
    param.size_handler = size_handler;
    param.dropfiles_handler = dropfiles_handler;
    param.ipc_handler = ipc_handler;
    param.keydown_handler = keydown_handler;
    param.keyup_handler = keyup_handler;
    param.char_handler = char_handler;
    param.lbuttondown_handler = lbuttondown_handler;
    param.lbuttonup_handler = lbuttonup_handler;
    param.mbuttondown_handler = mbuttondown_handler;
    param.mbuttonup_handler = mbuttonup_handler;
    param.rbuttondown_handler = rbuttondown_handler;
    param.rbuttonup_handler = rbuttonup_handler;
    param.lbuttondoubleclick_handler = lbuttondoubleclick_handler;
    param.mbuttondoubleclick_handler = mbuttondoubleclick_handler;
    param.rbuttondoubleclick_handler = rbuttondoubleclick_handler;
    param.mousemove_handler = mousemove_handler;
    param.mousewheel_handler = mousewheel_handler;
    param.nchittest_handler = nchittest_handler;

    Window * window = new Window(param);

    window->SetPyObject(self);

    ((Window_Object*)self)->p = window;

    return 0;
}

static void Window_dealloc(PyObject* self)
{
	FUNC_TRACE;

	Window_instance_count --;
	PRINTF("Window_instance_count=%d\n", Window_instance_count);

    Window * window = ((Window_Object*)self)->p;
	if(window){ window->SetPyObject(NULL); }

    self->ob_type->tp_free(self);
}

static PyObject * Window_getHWND(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    HWND hwnd = window->getHWND();

    PyObject * pyret = Py_BuildValue("i",hwnd);
    return pyret;
}

static PyObject * Window_getHINSTANCE(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    HINSTANCE hinstance = GetModuleHandle(NULL);

    PyObject * pyret = Py_BuildValue("i",hinstance);
    return pyret;
}

static PyObject * Window_width(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    PyObject * pyret = Py_BuildValue("i",window->width());
    return pyret;
}

static PyObject * Window_height(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    PyObject * pyret = Py_BuildValue("i",window->height());
    return pyret;
}

static PyObject * Window_getSize(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

	int w, h;
	window->getSize( &w, &h );
	PyObject * pyret = Py_BuildValue( "(ii)", w, h );
    return pyret;
}

static PyObject * Window_getNormalSize(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

	int w, h;
	window->getNormalSize( &w, &h );
	PyObject * pyret = Py_BuildValue( "(ii)", w, h );
    return pyret;
}

static PyObject * Window_show(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    int show;
    int activate=1;

    if( ! PyArg_ParseTuple(args, "i|i", &show, &activate ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    window->show(show!=0,activate!=0);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_enable(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    int enable;

    if( ! PyArg_ParseTuple(args, "i", &enable ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    window->enable(enable!=0);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_destroy(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    window->destroy();

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_activate(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    window->activate();

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_inactivate(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    window->inactivate();

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_foreground(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    window->foreground();

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_restore(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    window->restore();

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_maximize(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    window->maximize();

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_minimize(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    window->minimize();

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_topmost(PyObject* self, PyObject* args)
{
	FUNC_TRACE;
	
	int topmost;

    if( ! PyArg_ParseTuple(args, "i", &topmost ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    window->topmost( topmost!=0 );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_isEnabled(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    if( window->isEnabled() )
	{
		Py_INCREF(Py_True);
		return Py_True;
	}
	else
	{
		Py_INCREF(Py_False);
		return Py_False;
	}
}

static PyObject * Window_isVisible(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    if( window->isVisible() )
	{
		Py_INCREF(Py_True);
		return Py_True;
	}
	else
	{
		Py_INCREF(Py_False);
		return Py_False;
	}
}

static PyObject * Window_isMaximized(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    if( window->isMaximized() )
	{
		Py_INCREF(Py_True);
		return Py_True;
	}
	else
	{
		Py_INCREF(Py_False);
		return Py_False;
	}
}

static PyObject * Window_isMinimized(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    if( window->isMinimized() )
	{
		Py_INCREF(Py_True);
		return Py_True;
	}
	else
	{
		Py_INCREF(Py_False);
		return Py_False;
	}
}

static PyObject * Window_isActive(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    if( window->isActive() )
	{
		Py_INCREF(Py_True);
		return Py_True;
	}
	else
	{
		Py_INCREF(Py_False);
		return Py_False;
	}
}

static PyObject * Window_isForeground(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    if( window->isForeground() )
	{
		Py_INCREF(Py_True);
		return Py_True;
	}
	else
	{
		Py_INCREF(Py_False);
		return Py_False;
	}
}

static PyObject * Window_clear(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    window->clear();

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_putString(PyObject* self, PyObject* args, PyObject * kwds)
{
	FUNC_TRACE;

	int x;
	int y;
	int width;
	int height;
	PyObject * pyattr;
	PyObject * pystr;
	int offset=0;

    static char * kwlist[] = {
        "x",
        "y",
        "width",
        "height",
        "attr",
        "str",
        "offset",
        NULL
    };

    if( ! PyArg_ParseTupleAndKeywords( args, kwds, "iiiiOO|i", kwlist, 
    	&x, &y, &width, &height, &pyattr, &pystr,
    	&offset
	))
	{
        return NULL;
	}

    if( !Attribute_Check(pyattr) )
    {
        PyErr_SetString( PyExc_TypeError, "arg 4 must be a Attribute object.");
    	return NULL;
    }

    std::wstring str;
    if( !PythonUtil::PyStringToWideString( pystr, &str ) )
    {
    	return NULL;
    }

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    window->putString( x, y, width, height, ((Attribute_Object*)pyattr)->attr, str.c_str(), offset );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_flushPaint(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

	window->flushPaint();

    Py_INCREF(Py_None);
    return Py_None;
}

// オフスクリーンをそのままフロントにコピーする (描画システムのデバッグ用)
static PyObject * Window_bitBlt(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;
    
	HDC hDC = GetDC( window->hwnd );

    RECT client_rect;
    GetClientRect(window->hwnd, &client_rect);

	BitBlt( 
    	hDC, 
    	client_rect.left, 
    	client_rect.top, 
    	client_rect.right-client_rect.left, 
    	client_rect.bottom-client_rect.top, 
    	window->offscreen_dc, 
    	client_rect.left, 
    	client_rect.top, 
    	SRCCOPY );

	ReleaseDC( window->hwnd, hDC );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_scroll(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	int x, y, width, height, delta_x, delta_y;

    if( ! PyArg_ParseTuple(args, "iiiiii", &x, &y, &width, &height, &delta_x, &delta_y ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

	window->scroll( x, y, width, height, delta_x, delta_y );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_setCursorPos(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	int x;
	int y;

    if( ! PyArg_ParseTuple(args, "ii", &x, &y ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    window->setCursorPos( x, y );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_getCursorPos(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

	int x, y;
    window->getCursorPos( &x, &y );

	PyObject * pyret = Py_BuildValue( "(ii)", x, y );
	return pyret;
}

static PyObject * Window_setImeRect(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	PyObject * pyrect;

    if( ! PyArg_ParseTuple(args, "O", &pyrect ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;
	
	RECT rect;

	if( pyrect==Py_None )
	{
		memset( &rect, 0, sizeof(rect) );
		window->setImeRect(rect);
	}
	else if( pyrect && PySequence_Check(pyrect) )
	{
	    if( ! PyArg_ParseTuple( pyrect, "iiii", &rect.left, &rect.top, &rect.right, &rect.bottom ) )
	        return NULL;
		window->setImeRect(rect);
	}
	else
	{
		PyErr_SetString( PyExc_TypeError, "invalid argument type." );
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Window_enableIme(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	int enable;

    if( ! PyArg_ParseTuple(args, "i", &enable ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    window->enableIme( enable!=0 );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_setPosSize(PyObject* self, PyObject* args, PyObject * kwds)
{
	//FUNC_TRACE;

	int x;
	int y;
	int width;
	int height;
	int origin;

    static char * kwlist[] = {
        "x",
        "y",
        "width",
        "height",
        "origin",
        NULL
    };

    if(!PyArg_ParseTupleAndKeywords( args, kwds, "iiiii", kwlist,
        &x,
        &y,
        &width,
        &height,
        &origin
    ))
    {
        return NULL;
	}

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    window->setPosSize( x, y, width, height, origin );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_getStringWidth(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	PyObject * pystr;
	int tab_width = 4;
	int offset = 0;

    if( ! PyArg_ParseTuple(args, "O|ii", &pystr, &tab_width, &offset ) )
        return NULL;

    std::wstring str;
    if( !PythonUtil::PyStringToWideString( pystr, &str ) )
    {
    	return NULL;
    }

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

    int width = window->getStringWidth( str.c_str(), tab_width, offset );

    PyObject * pyret = Py_BuildValue("i",width);
    return pyret;
}

static PyObject * Window_getStringColumns(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	PyObject * pystr;
	int tab_width = 4;
	int offset = 0;

    if( ! PyArg_ParseTuple(args, "O|ii", &pystr, &tab_width, &offset ) )
        return NULL;

    std::wstring str;
    if( !PythonUtil::PyStringToWideString( pystr, &str ) )
    {
    	return NULL;
    }

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;

	int num = str.length()+1;
	int * columns = new int[num];

    window->getStringWidth( str.c_str(), tab_width, offset, columns );

	PyObject * pyret = PyTuple_New(num);
	for(int i=0 ; i<num ; ++i )
	{
		PyTuple_SET_ITEM( pyret, i, PyLong_FromLong(columns[i]) );
	}
	delete [] columns;
	
	return pyret;
}

static PyObject * Window_messageLoop(PyObject* self, PyObject* args, PyObject * kwds)
{
	//FUNC_TRACE;

	PyObject * continue_cond_func = NULL;

    static char * kwlist[] = {
        "continue_cond_func",
        NULL
    };

    if(!PyArg_ParseTupleAndKeywords( args, kwds, "|O", kwlist,
        &continue_cond_func
    ))
    {
        return NULL;
	}

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    MSG msg;
    for(;;)
    {
        while( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
        {
            if(msg.message==WM_QUIT)
            {
                goto end;
            }

            Window * window = ((Window_Object*)self)->p;

            if( window==NULL )
            {
                goto end;
            }

            if( window->quit_requested )
            {
                window->quit_requested = false;
                goto end;
            }

            if( continue_cond_func )
            {
                PyObject * pyarglist = Py_BuildValue("()");
                PyObject * pyresult = PyEval_CallObject( continue_cond_func, pyarglist );
                Py_DECREF(pyarglist);
                if(pyresult)
                {
                    int result;
                    PyArg_Parse(pyresult,"i", &result );
                    Py_DECREF(pyresult);
                    if(!result)
                    {
                        goto end;
                    }
                }
                else
                {
                    PyErr_Print();
                }
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        Py_BEGIN_ALLOW_THREADS

        WaitMessage();

        Py_END_ALLOW_THREADS
    }

    end:

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_removeKeyMessage(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple( args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    MSG msg;
    while( PeekMessage( &msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE ) )
    {
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_quit(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;
	
    if( ! PyArg_ParseTuple( args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;

    window->quit_requested = true;
    
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_getWindowRect(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;

	RECT rect;
	window->getWindowRect(&rect);

	PyObject * pyret = Py_BuildValue( "(iiii)", rect.left, rect.top, rect.right, rect.bottom );
	return pyret;
}

static PyObject * Window_getNormalWindowRect(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;

	RECT rect;
	window->getNormalWindowRect(&rect);

	PyObject * pyret = Py_BuildValue( "(iiii)", rect.left, rect.top, rect.right, rect.bottom );
	return pyret;
}

static PyObject * Window_getClientRect(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;

	RECT rect;
	::GetClientRect(window->hwnd,&rect);

	PyObject * pyret = Py_BuildValue( "(iiii)", rect.left, rect.top, rect.right, rect.bottom );
	return pyret;
}

static PyObject * Window_getCharSize(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;

	PyObject * pyret = Py_BuildValue( "(ii)", window->font_w, window->font_h );
	return pyret;
}

static PyObject * Window_charToScreen(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	int x;
	int y;

	if( ! PyArg_ParseTuple(args, "ii", &x, &y ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;

	POINT point;
	point.x = x * window->font_w + window->border_left;
	point.y = y * window->font_h + window->border_top;

	ClientToScreen( window->hwnd, &point );

	PyObject * pyret = Py_BuildValue( "(ii)", point.x, point.y );
	return pyret;
}

static PyObject * Window_charToClient(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	int x;
	int y;

	if( ! PyArg_ParseTuple(args, "ii", &x, &y ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;

	POINT point;
	point.x = x * window->font_w + window->border_left;
	point.y = y * window->font_h + window->border_top;

	PyObject * pyret = Py_BuildValue( "(ii)", point.x, point.y );
	return pyret;
}

static PyObject * Window_screenToClient(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	int x;
	int y;

	if( ! PyArg_ParseTuple(args, "ii", &x, &y ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;

	POINT point;
	point.x = x;
	point.y = y;

	ScreenToClient( window->hwnd, &point );

	PyObject * pyret = Py_BuildValue( "(ii)", point.x, point.y );
	return pyret;
}

static PyObject * Window_clientToScreen(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	int x;
	int y;

	if( ! PyArg_ParseTuple(args, "ii", &x, &y ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;

	POINT point;
	point.x = x;
	point.y = y;

	ClientToScreen( window->hwnd, &point );

	PyObject * pyret = Py_BuildValue( "(ii)", point.x, point.y );
	return pyret;
}

static PyObject * Window_setTimer( PyObject * self, PyObject * args )
{
	//FUNC_TRACE;

	PyObject * func;
	int interval;

	if( ! PyArg_ParseTuple(args,"Oi", &func, &interval ) )
		return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;

	Py_XINCREF(func);
	window->timer_list.push_back(func);

	SetTimer( window->hwnd, (UINT_PTR)func, interval, NULL );

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Window_killTimer( PyObject * self, PyObject * args )
{
	//FUNC_TRACE;

	PyObject * func;

	if( ! PyArg_ParseTuple(args,"O", &func ) )
		return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;

	for( std::list<TimerInfo>::iterator i=window->timer_list.begin(); i!=window->timer_list.end() ; i++ )
	{
		if( (i->pyobj)==NULL ) continue;

		if( PyObject_RichCompareBool( func, (i->pyobj), Py_EQ )==1 )
		{
			KillTimer( window->hwnd, (UINT_PTR)(i->pyobj) );

			Py_XDECREF( i->pyobj );
			i->pyobj = NULL;

			break;
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Window_delayedCall( PyObject * self, PyObject * args )
{
	//FUNC_TRACE;

	PyObject * func;
	int timeout;

	if( ! PyArg_ParseTuple(args,"Oi", &func, &timeout ) )
		return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;

	Py_XINCREF(func);
	window->delayed_call_list.push_back( DelayedCallInfo(func,timeout) );

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Window_setHotKey( PyObject * self, PyObject * args )
{
	//FUNC_TRACE;

	int vk;
	int mod;
	PyObject * func;

	if( ! PyArg_ParseTuple(args,"iiO", &vk, &mod, &func ) )
		return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;
	
	// 0x0000 - 0xBFFF の範囲で、使われていないIDを検索する
	int id;
	for( id=0 ; id<0xc000 ; ++id )
	{
		bool dup = false;
		for( std::list<HotKeyInfo>::const_iterator i=window->hotkey_list.begin(); i!=window->hotkey_list.end() ; ++i )
		{
			if( i->id==id )
			{
				dup = true;
				break;
			}
		}
		
		if(!dup)
		{
			break;
		}
	}
	
	if(id<0xc000)
	{
		Py_XINCREF(func);
		window->hotkey_list.push_back( HotKeyInfo(func,id) );

		RegisterHotKey( window->hwnd, id, mod, vk );
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Window_killHotKey( PyObject * self, PyObject * args )
{
	//FUNC_TRACE;

	PyObject * func;

	if( ! PyArg_ParseTuple(args,"O", &func ) )
		return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;

	for( std::list<HotKeyInfo>::iterator i=window->hotkey_list.begin(); i!=window->hotkey_list.end() ; i++ )
	{
		if( i->pyobj==NULL ) continue;

		if( PyObject_RichCompareBool( func, (i->pyobj), Py_EQ )==1 )
		{
			UnregisterHotKey( window->hwnd, i->id );

			Py_XDECREF( i->pyobj );
			i->pyobj = NULL;

			break;
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Window_setTitle(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	PyObject * pystr;

    if( ! PyArg_ParseTuple(args, "O", &pystr ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    std::wstring str;
    if( !PythonUtil::PyStringToWideString( pystr, &str ) )
    {
    	return NULL;
    }

	Window * window = ((Window_Object*)self)->p;

	::SetWindowText(window->hwnd,str.c_str());

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Window_setBGColor(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	int r, g, b;

	if( ! PyArg_ParseTuple(args, "(iii)", &r, &g, &b ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;
	window->setBGColor( RGB(r,g,b) );

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Window_setFrameColor(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	PyObject * pycolor;

    if( ! PyArg_ParseTuple(args, "O", &pycolor ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;
	
	if( pycolor && PySequence_Check(pycolor) )
	{
		int r, g, b;
	    if( ! PyArg_ParseTuple( pycolor, "iii", &r, &g, &b ) )
	        return NULL;

		window->setFrameColor( RGB(r,g,b) );
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Window_setCursorColor(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	PyObject * pycolor0;
	PyObject * pycolor1;

    if( ! PyArg_ParseTuple(args, "OO", &pycolor0, &pycolor1 ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;
	
	if( pycolor0 && PySequence_Check(pycolor0) && pycolor1 && PySequence_Check(pycolor1) )
	{
		int r0, g0, b0;
	    if( ! PyArg_ParseTuple( pycolor0, "iii", &r0, &g0, &b0 ) )
	        return NULL;

		int r1, g1, b1;
	    if( ! PyArg_ParseTuple( pycolor1, "iii", &r1, &g1, &b1 ) )
	        return NULL;

		window->setCursorColor( RGB(r0,g0,b0), RGB(r1,g1,b1) );
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Window_setMenu(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	PyObject * pymenu;

    if( ! PyArg_ParseTuple(args, "O", &pymenu ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;
	
	if( MenuNode_Check(pymenu) )
	{
		window->setMenu(pymenu);
	}
	else if(pymenu==Py_None)
	{
		window->setMenu(NULL);
	}
	else
	{
		PyErr_SetString( PyExc_TypeError, "invalid argument type." );
		return NULL;
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Window_setCapture(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;

	window->setCapture();

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Window_releaseCapture(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;

	window->releaseCapture();

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Window_setMouseCursor(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;
	
	int id;

    if( ! PyArg_ParseTuple(args, "i", &id ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;

	window->setMouseCursor(id);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Window_drag(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;
	
	int x,y;

    if( ! PyArg_ParseTuple(args, "ii", &x, &y ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;

	window->drag( x, y );

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Window_enumFonts(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	Window * window = ((Window_Object*)self)->p;

	std::vector<std::wstring> font_list;
	window->enumFonts( &font_list );

	std::sort( font_list.begin(), font_list.end() );

	PyObject * pyret = PyList_New(0);
	for( std::vector<std::wstring>::const_iterator i=font_list.begin() ; i!=font_list.end() ; i++ )
	{
		PyObject * data = Py_BuildValue( "u", (*i).c_str() );
		
		PyList_Append( pyret, data );
		
		Py_XDECREF(data);
	}
	return pyret;
}

static PyObject * Window_setFont(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	PyObject * pyfontname;
	int font_size;

    if( ! PyArg_ParseTuple(args, "Oi", &pyfontname, &font_size ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    std::wstring fontname;
    if( !PythonUtil::PyStringToWideString( pyfontname, &fontname ) )
    {
    	return NULL;
    }

	Window * window = ((Window_Object*)self)->p;

    window->setFont( fontname.c_str(), font_size );

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Window_popupMenu(PyObject* self, PyObject* args, PyObject * kwds)
{
	//FUNC_TRACE;

	int x;
	int y;
	PyObject * items;

    static char * kwlist[] = {
        "x",
        "y",
        "items",
        NULL
    };

    if(!PyArg_ParseTupleAndKeywords( args, kwds, "iiO", kwlist,
        &x,
        &y,
        &items
    ))
    {
        return NULL;
	}

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    Window * window = ((Window_Object*)self)->p;
	
	HMENU menu;
	menu = CreatePopupMenu();
	
	window->_clearPopupMenuCommands();
	
	if( PySequence_Check(items) )
	{
		int num_items = (int)PySequence_Length(items);
		for( int i=0 ; i<num_items ; ++i )
		{
			PyObject * item = PySequence_GetItem( items, i );

			PyObject * pyname;
			PyObject * func;
			PyObject * pyoption=NULL;
		    if( ! PyArg_ParseTuple( item, "OO|O", &pyname, &func, &pyoption ) )
		    {
				Py_XDECREF(item);
				DestroyMenu(menu);
		        return NULL;
		    }
		    
		    std::wstring name;
		    if( !PythonUtil::PyStringToWideString( pyname, &name ) )
		    {
				Py_XDECREF(item);
				DestroyMenu(menu);
		    	return NULL;
		    }
		    
		    if(name==L"-")
		    {
				AppendMenu( menu, MF_SEPARATOR, 0, L"" );
		    }
		    else
		    {
			    if( ID_POPUP_MENUITEM + window->popup_menu_commands.size() > ID_POPUP_MENUITEM_MAX )
			    {
					Py_XDECREF(item);
					DestroyMenu(menu);
					PyErr_SetString( PyExc_ValueError, "too many menu items." );
			    	return NULL;
			    }
			    
			    if(pyoption)
			    {
			    	if(pyoption==Py_True || pyoption==Py_False)
			    	{
						AppendMenu( menu, MF_ENABLED|((pyoption==Py_True)?MF_CHECKED:MF_UNCHECKED), ID_POPUP_MENUITEM + window->popup_menu_commands.size(), name.c_str() );
			    	}
			    	else
			    	{
						Py_XDECREF(item);
						DestroyMenu(menu);
						PyErr_SetString( PyExc_TypeError, "invalid menu option type." );
				    	return NULL;
			    	}
			    }
			    else
			    {
					AppendMenu( menu, MF_ENABLED, ID_POPUP_MENUITEM + window->popup_menu_commands.size(), name.c_str() );
			    }

				window->popup_menu_commands.push_back(func);
				Py_INCREF(func);
		    }
		
			Py_XDECREF(item);
		}
	}

	{
		HWND hwnd = window->hwnd;
	
		SetForegroundWindow(hwnd); //ウィンドウをフォアグラウンドに持ってきます。
		SetFocus(hwnd);	//これをしないと、メニューが消えなくなります。

		TrackPopupMenu( menu, TPM_TOPALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON, x, y, 0, hwnd, 0 );

		PostMessage(hwnd,WM_NULL,0,0); //これをしないと、２度目のメニューがすぐ消えちゃいます。
	}

	DestroyMenu(menu);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_sendIpc(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	HWND hwnd;
	char * buf;
	unsigned int bufsize;

    if( ! PyArg_ParseTuple(args, "is#", &hwnd, &buf, &bufsize ) )
        return NULL;

	COPYDATASTRUCT data;
	data.dwData = 0x101;
	data.cbData = bufsize;
	data.lpData = buf;
	SendMessage( hwnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&data );

	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef Window_methods[] = {
    { "getHWND", Window_getHWND, METH_VARARGS, "" },
    { "getHINSTANCE", Window_getHINSTANCE, METH_VARARGS, "" },
    { "width", Window_width, METH_VARARGS, "" },
    { "height", Window_height, METH_VARARGS, "" },
    { "getSize", Window_getSize, METH_VARARGS, "" },
    { "getNormalSize", Window_getNormalSize, METH_VARARGS, "" },
    { "show", Window_show, METH_VARARGS, "" },
    { "enable", Window_enable, METH_VARARGS, "" },
    { "destroy", Window_destroy, METH_VARARGS, "" },
    { "activate", Window_activate, METH_VARARGS, "" },
    { "inactivate", Window_inactivate, METH_VARARGS, "" },
    { "foreground", Window_foreground, METH_VARARGS, "" },
    { "restore", Window_restore, METH_VARARGS, "" },
    { "maximize", Window_maximize, METH_VARARGS, "" },
    { "minimize", Window_minimize, METH_VARARGS, "" },
    { "topmost", Window_topmost, METH_VARARGS, "" },
    { "isEnabled", Window_isEnabled, METH_VARARGS, "" },
    { "isVisible", Window_isVisible, METH_VARARGS, "" },
    { "isMaximized", Window_isMaximized, METH_VARARGS, "" },
    { "isMinimized", Window_isMinimized, METH_VARARGS, "" },
    { "isActive", Window_isActive, METH_VARARGS, "" },
    { "isForeground", Window_isForeground, METH_VARARGS, "" },
    { "clear", Window_clear, METH_VARARGS, "" },
    { "putString", (PyCFunction)Window_putString, METH_VARARGS|METH_KEYWORDS, "" },
    { "flushPaint", Window_flushPaint, METH_VARARGS, "" },
    { "bitBlt", Window_bitBlt, METH_VARARGS, "" },
    { "messageLoop", (PyCFunction)Window_messageLoop, METH_VARARGS|METH_KEYWORDS, "" },
    { "removeKeyMessage", Window_removeKeyMessage, METH_VARARGS, "" },
    { "quit", Window_quit, METH_VARARGS, "" },
    { "getWindowRect", Window_getWindowRect, METH_VARARGS, "" },
    { "getNormalWindowRect", Window_getNormalWindowRect, METH_VARARGS, "" },
    { "getClientRect", Window_getClientRect, METH_VARARGS, "" },
    { "getCharSize", Window_getCharSize, METH_VARARGS, "" },
    { "charToScreen", Window_charToScreen, METH_VARARGS, "" },
    { "charToClient", Window_charToClient, METH_VARARGS, "" },
    { "screenToClient", Window_screenToClient, METH_VARARGS, "" },
    { "clientToScreen", Window_clientToScreen, METH_VARARGS, "" },
    { "getStringWidth", Window_getStringWidth, METH_VARARGS, "" },
    { "getStringColumns", Window_getStringColumns, METH_VARARGS, "" },
    { "setTimer", Window_setTimer, METH_VARARGS, "" },
    { "killTimer", Window_killTimer, METH_VARARGS, "" },
    { "delayedCall", Window_delayedCall, METH_VARARGS, "" },
    { "setHotKey", Window_setHotKey, METH_VARARGS, "" },
    { "killHotKey", Window_killHotKey, METH_VARARGS, "" },
	{ "scroll", Window_scroll, METH_VARARGS, "" },
	{ "setCursorPos", Window_setCursorPos, METH_VARARGS, "" },
    { "getCursorPos", Window_getCursorPos, METH_VARARGS, "" },
    { "setImeRect", Window_setImeRect, METH_VARARGS, "" },
    { "enableIme", Window_enableIme, METH_VARARGS, "" },
    { "setPosSize", (PyCFunction)Window_setPosSize, METH_VARARGS|METH_KEYWORDS, "" },
    { "setTitle", (PyCFunction)Window_setTitle, METH_VARARGS, "" },
    { "setBGColor", (PyCFunction)Window_setBGColor, METH_VARARGS, "" },
    { "setFrameColor", (PyCFunction)Window_setFrameColor, METH_VARARGS, "" },
    { "setCursorColor", (PyCFunction)Window_setCursorColor, METH_VARARGS, "" },
    { "setMenu", (PyCFunction)Window_setMenu, METH_VARARGS, "" },
    { "setCapture", (PyCFunction)Window_setCapture, METH_VARARGS, "" },
    { "releaseCapture", (PyCFunction)Window_releaseCapture, METH_VARARGS, "" },
    { "setMouseCursor", (PyCFunction)Window_setMouseCursor, METH_VARARGS, "" },
    { "drag", (PyCFunction)Window_drag, METH_VARARGS, "" },
    { "enumFonts", (PyCFunction)Window_enumFonts, METH_VARARGS, "" },
    { "setFont", (PyCFunction)Window_setFont, METH_VARARGS, "" },
    { "popupMenu", (PyCFunction)Window_popupMenu, METH_VARARGS|METH_KEYWORDS, "" },
    { "sendIpc", (PyCFunction)Window_sendIpc, METH_STATIC|METH_VARARGS, "" },
    {NULL,NULL}
};

PyTypeObject Window_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
    "Window",			/* tp_name */
    sizeof(Window_Object), /* tp_basicsize */
    0,					/* tp_itemsize */
    (destructor)Window_dealloc,/* tp_dealloc */
    0,					/* tp_print */
    0,					/* tp_getattr */
    0,					/* tp_setattr */
    0,					/* tp_reserved */
    0, 					/* tp_repr */
    0,					/* tp_as_number */
    0,					/* tp_as_sequence */
    0,					/* tp_as_mapping */
    0,					/* tp_hash */
    0,					/* tp_call */
    0,					/* tp_str */
    PyObject_GenericGetAttr,/* tp_getattro */
    PyObject_GenericSetAttr,/* tp_setattro */
    0,					/* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,/* tp_flags */
    "",					/* tp_doc */
    0,					/* tp_traverse */
    0,					/* tp_clear */
    0,					/* tp_richcompare */
    0,					/* tp_weaklistoffset */
    0,					/* tp_iter */
    0,					/* tp_iternext */
    Window_methods,	/* tp_methods */
    0,					/* tp_members */
    0,					/* tp_getset */
    0,					/* tp_base */
    0,					/* tp_dict */
    0,					/* tp_descr_get */
    0,					/* tp_descr_set */
    0,					/* tp_dictoffset */
    Window_init,		/* tp_init */
    0,					/* tp_alloc */
    PyType_GenericNew,	/* tp_new */
    0,					/* tp_free */
};

// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------

static int TaskTrayIcon_instance_count = 0;

static int TaskTrayIcon_init( PyObject * self, PyObject * args, PyObject * kwds)
{
	FUNC_TRACE;

	TaskTrayIcon_instance_count ++;
	PRINTF("TaskTrayIcon_instance_count=%d\n", TaskTrayIcon_instance_count);

    PyObject * pystr_title = NULL;
    PyObject * lbuttondown_handler = NULL;
    PyObject * lbuttonup_handler = NULL;
    PyObject * rbuttondown_handler = NULL;
    PyObject * rbuttonup_handler = NULL;
    PyObject * lbuttondoubleclick_handler= NULL;

    static char * kwlist[] = {
        "title",
        "lbuttondown_handler",
        "lbuttonup_handler",
        "rbuttondown_handler",
        "rbuttonup_handler",
        "lbuttondoubleclick_handler",
        NULL
    };

    if(!PyArg_ParseTupleAndKeywords( args, kwds, "|OOOOOO", kwlist,
        &pystr_title,
        &lbuttondown_handler,
        &lbuttonup_handler,
        &rbuttondown_handler,
        &rbuttonup_handler,
        &lbuttondoubleclick_handler
    ))
    {
        return -1;
    }

    std::wstring str_title;
    if(pystr_title)
    {
        if( !PythonUtil::PyStringToWideString( pystr_title, &str_title ) )
        {
            return -1;
        }
    }

	TaskTrayIcon::Param param;
	param.title = str_title;
    param.lbuttondown_handler = lbuttondown_handler;
    param.lbuttonup_handler = lbuttonup_handler;
    param.rbuttondown_handler = rbuttondown_handler;
    param.rbuttonup_handler = rbuttonup_handler;
    param.lbuttondoubleclick_handler = lbuttondoubleclick_handler;

    TaskTrayIcon * task_tray_icon = new TaskTrayIcon(param);

    task_tray_icon->SetPyObject(self);

    ((TaskTrayIcon_Object*)self)->p = task_tray_icon;

    return 0;
}

static void TaskTrayIcon_dealloc(PyObject* self)
{
	FUNC_TRACE;

	TaskTrayIcon_instance_count --;
	PRINTF("TaskTrayIcon_instance_count=%d\n", TaskTrayIcon_instance_count);

    TaskTrayIcon * task_tray_icon = ((TaskTrayIcon_Object*)self)->p;
	if(task_tray_icon){ task_tray_icon->SetPyObject(NULL); }

    self->ob_type->tp_free(self);
}

static PyObject * TaskTrayIcon_destroy(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    TaskTrayIcon * task_tray_icon = ((TaskTrayIcon_Object*)self)->p;

    task_tray_icon->destroy();

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * TaskTrayIcon_popupMenu(PyObject* self, PyObject* args, PyObject * kwds)
{
	FUNC_TRACE;

	int x;
	int y;
	PyObject * items;

    static char * kwlist[] = {
        "x",
        "y",
        "items",
        NULL
    };

    if(!PyArg_ParseTupleAndKeywords( args, kwds, "iiO", kwlist,
        &x,
        &y,
        &items
    ))
    {
        return NULL;
	}

	if( ! ((TaskTrayIcon_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    TaskTrayIcon * task_tray_icon = ((TaskTrayIcon_Object*)self)->p;
	
	HMENU menu;
	menu = CreatePopupMenu();
	
	task_tray_icon->_clearPopupMenuCommands();
	
	if( PySequence_Check(items) )
	{
		int num_items = (int)PySequence_Length(items);
		for( int i=0 ; i<num_items ; ++i )
		{
			PyObject * item = PySequence_GetItem( items, i );

			PyObject * pyname;
			PyObject * func;
			PyObject * pyoption=NULL;
		    if( ! PyArg_ParseTuple( item, "OO|O", &pyname, &func, &pyoption ) )
		    {
				Py_XDECREF(item);
				DestroyMenu(menu);
		        return NULL;
		    }
		    
		    std::wstring name;
		    if( !PythonUtil::PyStringToWideString( pyname, &name ) )
		    {
				Py_XDECREF(item);
				DestroyMenu(menu);
		    	return NULL;
		    }
		    
		    if(name==L"-")
		    {
				AppendMenu( menu, MF_SEPARATOR, 0, L"" );
		    }
		    else
		    {
			    if( ID_POPUP_MENUITEM + task_tray_icon->popup_menu_commands.size() > ID_POPUP_MENUITEM_MAX )
			    {
					Py_XDECREF(item);
					DestroyMenu(menu);
					PyErr_SetString( PyExc_ValueError, "too many menu items." );
			    	return NULL;
			    }

			    if(pyoption)
			    {
			    	if(pyoption==Py_True || pyoption==Py_False)
			    	{
						AppendMenu( menu, MF_ENABLED|((pyoption==Py_True)?MF_CHECKED:MF_UNCHECKED), ID_POPUP_MENUITEM + task_tray_icon->popup_menu_commands.size(), name.c_str() );
			    	}
			    	else
			    	{
						Py_XDECREF(item);
						DestroyMenu(menu);
						PyErr_SetString( PyExc_TypeError, "invalid menu option type." );
				    	return NULL;
			    	}
			    }
			    else
			    {
					AppendMenu( menu, MF_ENABLED, ID_POPUP_MENUITEM + task_tray_icon->popup_menu_commands.size(), name.c_str() );
			    }

				task_tray_icon->popup_menu_commands.push_back(func);
				Py_INCREF(func);
		    }
		
			Py_XDECREF(item);
		}
	}
	
	{
		HWND hwnd = task_tray_icon->hwnd;
	
		SetForegroundWindow(hwnd); //ウィンドウをフォアグラウンドに持ってきます。
		SetFocus(hwnd);	//これをしないと、メニューが消えなくなります。

		TrackPopupMenu( menu, TPM_TOPALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON, x, y, 0, hwnd, 0 );

		PostMessage(hwnd,WM_NULL,0,0); //これをしないと、２度目のメニューがすぐ消えちゃいます。
	}

	DestroyMenu(menu);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef TaskTrayIcon_methods[] = {
    { "destroy", TaskTrayIcon_destroy, METH_VARARGS, "" },
    { "popupMenu", (PyCFunction)TaskTrayIcon_popupMenu, METH_VARARGS|METH_KEYWORDS, "" },
    {NULL,NULL}
};

PyTypeObject TaskTrayIcon_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
    "TaskTrayIcon",		/* tp_name */
    sizeof(TaskTrayIcon_Object), /* tp_basicsize */
    0,					/* tp_itemsize */
    (destructor)TaskTrayIcon_dealloc,/* tp_dealloc */
    0,					/* tp_print */
    0,					/* tp_getattr */
    0,					/* tp_setattr */
    0,					/* tp_reserved */
    0, 					/* tp_repr */
    0,					/* tp_as_number */
    0,					/* tp_as_sequence */
    0,					/* tp_as_mapping */
    0,					/* tp_hash */
    0,					/* tp_call */
    0,					/* tp_str */
    PyObject_GenericGetAttr,/* tp_getattro */
    PyObject_GenericSetAttr,/* tp_setattro */
    0,					/* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,/* tp_flags */
    "",					/* tp_doc */
    0,					/* tp_traverse */
    0,					/* tp_clear */
    0,					/* tp_richcompare */
    0,					/* tp_weaklistoffset */
    0,					/* tp_iter */
    0,					/* tp_iternext */
    TaskTrayIcon_methods,/* tp_methods */
    0,					/* tp_members */
    0,					/* tp_getset */
    0,					/* tp_base */
    0,					/* tp_dict */
    0,					/* tp_descr_get */
    0,					/* tp_descr_set */
    0,					/* tp_dictoffset */
    TaskTrayIcon_init,	/* tp_init */
    0,					/* tp_alloc */
    PyType_GenericNew,	/* tp_new */
    0,					/* tp_free */
};

// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------

enum
{
	Line_End_CR   = 1<<0,
	Line_End_LF   = 1<<1,
	Line_End_CRLF = Line_End_CR | Line_End_LF,

	Line_BG_1 = 1<<2,
	Line_BG_2 = 1<<3,
	Line_BG_3 = Line_BG_1 | Line_BG_2,

	Line_Modified = 1<<4,

	Line_Bookmark_1 = 1<<5,
	Line_Bookmark_2 = 1<<6,
	Line_Bookmark_3 = 1<<7,
};

enum
{
	Line_End_Bit = 0,
	Line_BG_Bit = 2,
	Line_Modified_Bit = 4,
	Line_Bookmark_Bit = 5,
};

static PyObject * line_cr = NULL;
static PyObject * line_lf = NULL;
static PyObject * line_crlf = NULL;
static PyObject * line_empty = NULL;

static void Line_static_init()
{
	line_cr   = Py_BuildValue( "u", L"\r" );
	line_lf   = Py_BuildValue( "u", L"\n" );
	line_crlf = Py_BuildValue( "u", L"\r\n" );
	line_empty = Py_BuildValue( "u", L"" );
}

static void Line_static_term()
{
	Py_DECREF(line_cr);
	Py_DECREF(line_lf);
	Py_DECREF(line_crlf);
	Py_DECREF(line_empty);
}

static int _Line_CheckLineEnd( const wchar_t * s, int len )
{
	if( len>=2 && s[len-2]=='\r' && s[len-1]=='\n' )
	{
		return Line_End_CRLF;
	}
	else if( len>=1 && s[len-1]=='\n' )
	{
		return Line_End_LF;
	}
	else if( len>=1 && s[len-1]=='\r' )
	{
		return Line_End_CR;
	}
	return 0;
}

static int Line_init( PyObject * self, PyObject * args, PyObject * kwds)
{
	const wchar_t * s;
	int len;

    static char * kwlist[] = {
        "s",
        NULL
    };

    if(!PyArg_ParseTupleAndKeywords( args, kwds, "u#", kwlist,
        &s, &len
    ))
    {
        return -1;
	}
	
	int lineend = _Line_CheckLineEnd( s, len );
	switch(lineend)
	{
	case Line_End_CR:
	case Line_End_LF:
		len -= 1;
		break;
		
	case Line_End_CRLF:
		len -= 2;
		break;
	}

	((Line_Object*)self)->s = Py_BuildValue( "u#", s, len );
	((Line_Object*)self)->ctx = NULL;
	((Line_Object*)self)->tokens = NULL;
	((Line_Object*)self)->flags = lineend;

	return 0;
}

static void Line_dealloc(PyObject* self)
{
	FUNC_TRACE;
	
	Py_XDECREF(((Line_Object*)self)->s);
	Py_XDECREF(((Line_Object*)self)->ctx);
	Py_XDECREF(((Line_Object*)self)->tokens);

    self->ob_type->tp_free(self);
}

static PyObject * Line_GetAttrString( PyObject * self, const char * attr_name )
{
	switch(attr_name[0])
	{
	case 's':
		if( strcmp(attr_name,"s")==0 )
		{
			PyObject * value = ((Line_Object*)self)->s;
			if(!value)
			{
				value = Py_None;
			}
			Py_INCREF(value);
			return value;
		}
		break;

	case 'e':
		if( strcmp(attr_name,"end")==0 )
		{
			int lineend = ((Line_Object*)self)->flags & ( Line_End_CR | Line_End_LF );
			
			switch(lineend)
			{
			case Line_End_CR:
				Py_INCREF(line_cr);
				return line_cr;
			
			case Line_End_LF:
				Py_INCREF(line_lf);
				return line_lf;
			
			case Line_End_CRLF:
				Py_INCREF(line_crlf);
				return line_crlf;
			}
			
			Py_INCREF(line_empty);
			return line_empty;
		}
		break;

	case 'c':
		if( strcmp(attr_name,"ctx")==0 )
		{
			PyObject * value = ((Line_Object*)self)->ctx;
			if(!value)
			{
				value = Py_None;
			}
			Py_INCREF(value);
			return value;
		}
		break;

	case 't':
		if( strcmp(attr_name,"tokens")==0 )
		{
			PyObject * value = ((Line_Object*)self)->tokens;
			if(!value)
			{
				value = Py_None;
			}
			Py_INCREF(value);
			return value;
		}
		break;

	case 'b':
		if( strcmp(attr_name,"bg")==0 )
		{
			int bg = (((Line_Object*)self)->flags & ( Line_BG_1 | Line_BG_2 )) >> Line_BG_Bit;
			return Py_BuildValue( "i", bg );
		}
		else if( strcmp(attr_name,"bookmark")==0 )
		{
			int bookmark = (((Line_Object*)self)->flags & ( Line_Bookmark_1 | Line_Bookmark_2 | Line_Bookmark_3 )) >> Line_Bookmark_Bit;
			return Py_BuildValue( "i", bookmark );
		}
		break;

	case 'm':
		if( strcmp(attr_name,"modified")==0 )
		{
			if( ((Line_Object*)self)->flags & Line_Modified )
			{
				Py_INCREF(Py_True);
				return Py_True;
			}
			else
			{
				Py_INCREF(Py_False);
				return Py_False;
			}
		}
		break;
	}
	
	PyErr_SetString( PyExc_AttributeError, attr_name );
	return NULL;
}

static int Line_SetAttrString( PyObject * self, const char * attr_name, PyObject * value )
{
	switch(attr_name[0])
	{
	case 's':
		if( strcmp(attr_name,"s")==0 )
		{
			Py_XDECREF( ((Line_Object*)self)->s );
			((Line_Object*)self)->s = value;
			Py_XINCREF( ((Line_Object*)self)->s );
			return 0;
		}
		break;

	case 'e':
		if( strcmp(attr_name,"end")==0 )
		{
			if(PyUnicode_Check(value))
			{
				int lineend = _Line_CheckLineEnd( PyUnicode_AS_UNICODE(value), PyUnicode_GET_SIZE(value) );
				
				((Line_Object*)self)->flags &= ~(Line_End_CR|Line_End_LF);
				((Line_Object*)self)->flags |= lineend;
				
				return 0;
			}
			else
			{
				PyErr_SetString( PyExc_TypeError, "'end' must be a unicode object." );
				return -1;
			}
		}
		break;

	case 'c':
		if( strcmp(attr_name,"ctx")==0 )
		{
			Py_XDECREF( ((Line_Object*)self)->ctx );
			((Line_Object*)self)->ctx = value;
			Py_XINCREF( ((Line_Object*)self)->ctx );
			return 0;
		}
		break;

	case 't':
		if( strcmp(attr_name,"tokens")==0 )
		{
			Py_XDECREF( ((Line_Object*)self)->tokens );
			((Line_Object*)self)->tokens = value;
			Py_XINCREF( ((Line_Object*)self)->tokens );
			return 0;
		}
		break;

	case 'b':
		if( strcmp(attr_name,"bg")==0 )
		{
			int bg;
			if( PyLong_Check(value) )
			{
				bg = PyLong_AS_LONG(value);
			}
			else if( PyLong_Check(value) )
			{
				bg = PyLong_AsLong(value);
			}
			else
			{
				PyErr_SetString( PyExc_TypeError, "'bg' must be a int object." );
				return -1;
			}

			((Line_Object*)self)->flags &= ~(Line_BG_1|Line_BG_2);
			((Line_Object*)self)->flags |= bg << Line_BG_Bit;
			
			return 0;
		}
		else if( strcmp(attr_name,"bookmark")==0 )
		{
			int bookmark;
			if( PyLong_Check(value) )
			{
				bookmark = PyLong_AS_LONG(value);
			}
			else if( PyLong_Check(value) )
			{
				bookmark = PyLong_AsLong(value);
			}
			else
			{
				PyErr_SetString( PyExc_TypeError, "'bookmark' must be a int object." );
				return -1;
			}

			((Line_Object*)self)->flags &= ~(Line_Bookmark_1|Line_Bookmark_2|Line_Bookmark_3);
			((Line_Object*)self)->flags |= bookmark << Line_Bookmark_Bit;
			
			return 0;
		}
		break;

	case 'm':
		if( strcmp(attr_name,"modified")==0 )
		{
			int modified;
			if( value==Py_True )
			{
				modified = Line_Modified;
			}
			else if( value==Py_False )
			{
				modified = 0;
			}
			else
			{
				PyErr_SetString( PyExc_TypeError, "'modified' must be True or False." );
				return -1;
			}

			((Line_Object*)self)->flags &= ~(Line_Modified);
			((Line_Object*)self)->flags |= modified;
			
			return 0;
		}
		break;
	}
	
	PyErr_SetString( PyExc_AttributeError, attr_name );
	return -1;
}

static PyMethodDef Line_methods[] = {
    {NULL,NULL}
};

PyTypeObject Line_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
    "Line",				/* tp_name */
    sizeof(Line_Object), /* tp_basicsize */
    0,					/* tp_itemsize */
    (destructor)Line_dealloc,/* tp_dealloc */
    0,					/* tp_print */
    (getattrfunc)Line_GetAttrString,/* tp_getattr */
    (setattrfunc)Line_SetAttrString,/* tp_setattr */
    0,					/* tp_reserved */
    0, 					/* tp_repr */
    0,					/* tp_as_number */
    0,					/* tp_as_sequence */
    0,					/* tp_as_mapping */
    0,					/* tp_hash */
    0,					/* tp_call */
    0,					/* tp_str */
    0,					/* tp_getattro */
    0,					/* tp_setattro */
    0,					/* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,/* tp_flags */
    "",					/* tp_doc */
    0,					/* tp_traverse */
    0,					/* tp_clear */
    0,					/* tp_richcompare */
    0,					/* tp_weaklistoffset */
    0,					/* tp_iter */
    0,					/* tp_iternext */
    Line_methods,		/* tp_methods */
    0,					/* tp_members */
    0,					/* tp_getset */
    0,					/* tp_base */
    0,					/* tp_dict */
    0,					/* tp_descr_get */
    0,					/* tp_descr_set */
    0,					/* tp_dictoffset */
    Line_init,			/* tp_init */
    0,					/* tp_alloc */
    PyType_GenericNew,	/* tp_new */
    0,					/* tp_free */
};

// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------

static PyObject * _registerWindowClass( PyObject * self, PyObject * args )
{
	FUNC_TRACE;

    PyObject * pyprefix;

    if( ! PyArg_ParseTuple(args, "O", &pyprefix ) )
        return NULL;

    std::wstring prefix;
    if(pyprefix)
    {
        if( !PythonUtil::PyStringToWideString( pyprefix, &prefix ) )
        {
            return NULL;
        }
    }

	WINDOW_CLASS_NAME 			= prefix + L"WindowClass";
	TASKTRAY_WINDOW_CLASS_NAME  = prefix + L"TaskTrayWindowClass";

    if( ! Window::_registerWindowClass() )
    {
		PyErr_SetFromWindowsErr(0);
        return NULL;
    }

    if( ! TaskTrayIcon::_registerWindowClass() )
    {
		PyErr_SetFromWindowsErr(0);
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * _registerCommandInfoConstructor( PyObject * self, PyObject * args )
{
	PyObject * _command_info_constructor;

	if( ! PyArg_ParseTuple(args,"O", &_command_info_constructor ) )
		return NULL;
	
	Py_XDECREF(command_info_constructor);
	command_info_constructor = _command_info_constructor;
	Py_INCREF(command_info_constructor);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * _setGlobalOption( PyObject * self, PyObject * args )
{
	FUNC_TRACE;

    int option;
    int enable;

    if( ! PyArg_ParseTuple(args, "ii", &option, &enable ) )
        return NULL;

	switch(option)
	{
	case GLOBAL_OPTION_WALKAROUND_KB436093:
		{
			walkaround_kb436093 = (enable!=0);
		}
		break;
	
	default:
		{
			PyErr_SetString( PyExc_ValueError, "unknown option id." );
			return NULL;
		}
	}

    Py_INCREF(Py_None);
    return Py_None;
}

static int _blockDetectorCallback( PyObject * py_report_func, PyFrameObject * frame, int what, PyObject * arg )
{
	static DWORD tick_prev = GetTickCount();
	DWORD tick = GetTickCount();
	DWORD delta = tick-tick_prev;
	tick_prev = tick;
	
	if( delta > 1000 )
	{
		PyObject * pyarglist = Py_BuildValue("()");
		PyObject * pyresult = PyEval_CallObject( py_report_func, pyarglist );
		Py_DECREF(pyarglist);
		if(pyresult)
		{
			Py_DECREF(pyresult);
		}
		else
		{
			PyErr_Print();
		}
	}
	
	return 0;
}

static PyObject * py_report_func;

static PyObject * _enableBlockDetector( PyObject * self, PyObject * args )
{
	PyObject * _py_report_func;

	if( ! PyArg_ParseTuple(args,"O", &_py_report_func ) )
		return NULL;
	
	Py_XDECREF(py_report_func);
	py_report_func = _py_report_func;
	Py_INCREF(py_report_func);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * _setBlockDetector( PyObject * self, PyObject * args )
{
	if( ! PyArg_ParseTuple(args,"") )
		return NULL;

	if(py_report_func)
	{
		PyEval_SetProfile( _blockDetectorCallback, py_report_func );
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef ckit_funcs[] =
{
    { "registerWindowClass", _registerWindowClass, METH_VARARGS, "" },
    { "registerCommandInfoConstructor", _registerCommandInfoConstructor, METH_VARARGS, "" },
    { "setGlobalOption", _setGlobalOption, METH_VARARGS, "" },
    { "enableBlockDetector", _enableBlockDetector, METH_VARARGS, "" },
    { "setBlockDetector", _setBlockDetector, METH_VARARGS, "" },
    {NULL,NULL}
};

// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------

static PyModuleDef ckitcore_module =
{
    PyModuleDef_HEAD_INIT,
    MODULE_NAME,
    "ckitcore module.",
    -1,
    ckit_funcs,
	NULL, NULL, NULL, NULL
};

extern "C" PyMODINIT_FUNC PyInit_ckitcore(void)
{
	FUNC_TRACE;

    if( PyType_Ready(&Attribute_Type)<0 ) return NULL;
    if( PyType_Ready(&Image_Type)<0 ) return NULL;
    if( PyType_Ready(&Icon_Type)<0 ) return NULL;
    if( PyType_Ready(&Plane_Type)<0 ) return NULL;
    if( PyType_Ready(&MenuNode_Type)<0 ) return NULL;
    if( PyType_Ready(&Window_Type)<0 ) return NULL;
    if( PyType_Ready(&TaskTrayIcon_Type)<0 ) return NULL;
    if( PyType_Ready(&Line_Type)<0 ) return NULL;

    PyObject *m, *d;

    m = PyModule_Create(&ckitcore_module);
    if(m == NULL) return NULL;

    Py_INCREF(&Attribute_Type);
    PyModule_AddObject( m, "Attribute", (PyObject*)&Attribute_Type );

    Py_INCREF(&Image_Type);
    PyModule_AddObject( m, "Image", (PyObject*)&Image_Type );

    Py_INCREF(&Icon_Type);
    PyModule_AddObject( m, "Icon", (PyObject*)&Icon_Type );

    Py_INCREF(&Plane_Type);
    PyModule_AddObject( m, "Plane", (PyObject*)&Plane_Type );

    Py_INCREF(&MenuNode_Type);
    PyModule_AddObject( m, "MenuNode", (PyObject*)&MenuNode_Type );

    Py_INCREF(&Window_Type);
    PyModule_AddObject( m, "Window", (PyObject*)&Window_Type );

    Py_INCREF(&TaskTrayIcon_Type);
    PyModule_AddObject( m, "TaskTrayIcon", (PyObject*)&TaskTrayIcon_Type );

    Py_INCREF(&Line_Type);
    PyModule_AddObject( m, "Line", (PyObject*)&Line_Type );

	Line_static_init();

    d = PyModule_GetDict(m);

    Error = PyErr_NewException( MODULE_NAME".Error", NULL, NULL);
    PyDict_SetItemString( d, "Error", Error );

    if( PyErr_Occurred() )
    {
        Py_FatalError( "can't initialize module "MODULE_NAME );
    }

	return m;
}
