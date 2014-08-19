#include <vector>
#include <algorithm>

#if defined(_DEBUG)
#undef _DEBUG
#include "python.h"
#include "structmember.h"
#include "frameobject.h"
#define _DEBUG
#else
#include "python.h"
#include "structmember.h"
#include "frameobject.h"
#endif

#include "basictypes.h"
#include "pythonutil.h"
#include "ckitcore.h"

#if defined(PLATFORM_WIN32)
# include "ckitcore_win.h"
#elif defined(PLATFORM_MAC)
# include "ckitcore_mac.h"
#endif // PLATFORM_XXX

using namespace ckit;

//-----------------------------------------------------------------------------

#define MODULE_NAME "ckitcore"

static std::wstring WINDOW_CLASS_NAME 			= L"CkitWindowClass";
static std::wstring TASKTRAY_WINDOW_CLASS_NAME  = L"CkitTaskTrayWindowClass";

namespace ckit
{
	GlobalBase * g = new Global();
};

#if defined(PLATFORM_WIN32)
// FIXME : Windows用ソースに移動させる
const int WM_USER_NTFYICON  = WM_USER + 100;
const int ID_MENUITEM       = 256;
const int ID_MENUITEM_MAX   = 256+1024-1;
const int ID_POPUP_MENUITEM       = ID_MENUITEM_MAX+1;
const int ID_POPUP_MENUITEM_MAX   = ID_POPUP_MENUITEM+1024;

const int TIMER_PAINT		   			= 0x101;
const int TIMER_PAINT_INTERVAL 			= 10;
const int TIMER_CARET_BLINK   			= 0x102;
#endif //PLATFORM_WIN32

const int GLOBAL_OPTION_XXXX = 0x101;

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

			printf( "FuncTrace : Enter : %s(%d)\n", funcname, lineno );
		}

		~FuncTrace()
		{
			printf( "FuncTrace : Leave : %s(%d)\n", funcname, lineno );
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

ImageBase::ImageBase( int _width, int _height, const char * pixels, const Color * _transparent_color, bool _halftone )
	:
	width(_width),
	height(_height),
	transparent(_transparent_color!=0),
	halftone(_halftone),
    transparent_color( _transparent_color ? *_transparent_color : Color::Zero() ),
	ref_count(0)
{
}

ImageBase::~ImageBase()
{
}

//-----------------------------------------------------------------------------

FontBase::FontBase()
	:
	char_width(0),
	char_height(0),
	ref_count(0)
{

}

FontBase::~FontBase()
{
}

//-----------------------------------------------------------------------------

PlaneBase::PlaneBase( WindowBase * _window, int _x, int _y, int _width, int _height, float _priority )
	:
	window(_window),
	show(true),
	x(_x),
	y(_y),
	width(_width),
	height(_height),
	priority(_priority)
{
	FUNC_TRACE;
}

PlaneBase::~PlaneBase()
{
	FUNC_TRACE;

	Rect dirty_rect = { x, y, x+width, y+height };
	window->appendDirtyRect( dirty_rect );
}

void PlaneBase::Show( bool _show )
{
	FUNC_TRACE;

	if( show == _show ) return;

	show = _show;

	Rect dirty_rect = { x, y, x+width, y+height };
	window->appendDirtyRect( dirty_rect );
}

void PlaneBase::SetPosition( int _x, int _y )
{
	FUNC_TRACE;

	if( x==_x && y==_y ) return;

	Rect dirty_rect = { x, y, x+width, y+height };
	window->appendDirtyRect( dirty_rect );

	x = _x;
	y = _y;

	Rect dirty_rect2 = { x, y, x+width, y+height };
	window->appendDirtyRect( dirty_rect2 );
}

void PlaneBase::SetSize( int _width, int _height )
{
	FUNC_TRACE;

	if( width==_width && height==_height ) return;

	Rect dirty_rect = { x, y, x+width, y+height };
	window->appendDirtyRect( dirty_rect );

	width = _width;
	height = _height;

	Rect dirty_rect2 = { x, y, x+width, y+height };
	window->appendDirtyRect( dirty_rect2 );
}

void PlaneBase::SetPriority( float _priority )
{
	FUNC_TRACE;

	if( priority==_priority ) return;

	priority = _priority;

	Rect dirty_rect = { x, y, x+width, y+height };
	window->appendDirtyRect( dirty_rect );
}

//-----------------------------------------------------------------------------

ImagePlaneBase::ImagePlaneBase( WindowBase * _window, int _x, int _y, int _width, int _height, float _priority )
	:
	PlaneBase(_window,_x,_y,_width,_height,_priority),
	pyobj(NULL),
	image(NULL)
{
	FUNC_TRACE;
}

ImagePlaneBase::~ImagePlaneBase()
{
	FUNC_TRACE;

	if(image) image->Release();

	((ImagePlane_Object*)pyobj)->p = NULL;
	Py_XDECREF(pyobj); pyobj=NULL;
}

void ImagePlaneBase::SetPyObject( PyObject * _pyobj )
{
	FUNC_TRACE;

	if( pyobj != _pyobj )
	{
		Py_XDECREF(pyobj);
		pyobj = _pyobj;
		Py_XINCREF(pyobj);
	}
}

void ImagePlaneBase::SetImage( ImageBase * _image )
{
	FUNC_TRACE;

	if( image == _image ) return;

	if(image) image->Release();

	image = _image;

	if(image) image->AddRef();

	Rect dirty_rect = { x, y, x+width, y+height };
	window->appendDirtyRect( dirty_rect );
}

//-----------------------------------------------------------------------------

TextPlaneBase::TextPlaneBase( WindowBase * _window, int _x, int _y, int _width, int _height, float _priority )
	:
	PlaneBase(_window,_x,_y,_width,_height,_priority),
	pyobj(NULL),
	font(NULL),
	dirty(true)
{
}

TextPlaneBase::~TextPlaneBase()
{
	FUNC_TRACE;

	if(font){ font->Release(); }

	{
		std::vector<Line*>::iterator i;
		for( i=char_buffer.begin() ; i!=char_buffer.end() ; i++ )
		{
			delete (*i);
		}
		char_buffer.clear();
	}

	((TextPlane_Object*)pyobj)->p = NULL;
	Py_XDECREF(pyobj); pyobj=NULL;
}

void TextPlaneBase::SetPyObject( PyObject * _pyobj )
{
	FUNC_TRACE;

	if( pyobj != _pyobj )
	{
		Py_XDECREF(pyobj);
		pyobj = _pyobj;
		Py_XINCREF(pyobj);
	}
}

void TextPlaneBase::SetFont( FontBase * _font )
{
	FUNC_TRACE;

	// FIXME : SetFontしないで TextPlane を使うと死んでしまうのを直したい

	if( font == _font ) return;

	if(font) font->Release();

	font = _font;

	if(font) font->AddRef();

	// FIXME : オフスクリーン全域の強制再描画
	dirty = true;

	Rect dirty_rect = { x, y, x+width, y+height };
	window->appendDirtyRect( dirty_rect );
}

static inline void PutChar( Line * line, int pos, const Char & chr, bool * modified )
{
	if( (int)line->size() <= pos )
	{
        line->push_back(chr);
        *modified = true;
	}
	else
	{
		if( (*line)[pos] != chr )
		{
			(*line)[pos] = chr;
	        *modified = true;
		}
	}
}

void TextPlaneBase::PutString( int x, int y, int width, int height, const Attribute & attr, const wchar_t * str, int offset )
{
	FUNC_TRACE;
	
	if( x<0 || y<0 )
	{
		return;
	}

	bool modified = false;

	while( (int)char_buffer.size() <= y )
	{
        Line * line = new Line();
        char_buffer.push_back(line);

        modified = true;
	}

    Line * line = char_buffer[y];

	// 埋まっていなかったら空文字で進める
	while( (int)line->size() < x )
	{
        line->push_back( Char( L' ' ) );

        modified = true;
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
    	if( font->zenkaku_table[str[i]] && pos + 2 > x + width )
    	{
	        PutChar( line, pos, Char( L' ', attr ), &modified );
    		break;
    	}

		if( pos>=x )
		{
	        PutChar( line, pos, Char( str[i], attr ), &modified );
			pos++;

	        // 全角文字は、幅を合わせるために、後ろに無駄な文字を入れる
	        if(font->zenkaku_table[str[i]])
	        {
		        PutChar( line, pos, Char(0), &modified );
				pos++;
	        }
		}
		else
		{
			pos++;
	        if(font->zenkaku_table[str[i]])
	        {
	        	// 左端で全角文字の半分の位置から描画範囲に入る場合はスペースで埋める
	        	if( pos>=x )
	        	{
			        PutChar( line, pos, Char( L' ', attr ), &modified );
	        	}
				pos++;
	        }
		}
    }

    if( modified )
    {
		dirty = true;

		// FIXME : PutString全域ではなく、変更のあった最小限の領域を dirty_rect にしたい
		Rect dirty_rect = { x * font->char_width + this->x, y * font->char_height + this->y, pos * font->char_width + this->x, (y+height) * font->char_height + this->y };
		window->appendDirtyRect( dirty_rect );
    }
}

int TextPlaneBase::GetStringWidth( const wchar_t * str, int tab_width, int offset, int columns[] )
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

        if(font->zenkaku_table[str[i]])
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

void TextPlaneBase::SetCaretPosition( int caret_x, int caret_y )
{
	Rect caret_rect = { 
		caret_x * font->char_width + x, 
		caret_y * font->char_height + y,
		caret_x * font->char_width + x + 2,
		(caret_y+1) * font->char_height + y
	};

	window->setCaretRect(caret_rect);
	window->setImeFont(font);
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

WindowBase::Param::Param()
{
	FUNC_TRACE;

    is_winpos_given = false;
    winpos_x = 0;
    winpos_y = 0;
    winsize_w = 100;
    winsize_h = 100;
    origin = 0;
    parent_window = NULL;
    parent_window_hwnd = NULL;
    bg_color = Color::FromRgb(0x01, 0x01, 0x01);
    frame_color = Color::FromRgb(0xff, 0xff, 0xff);
    caret0_color = Color::FromRgb(0xff, 0xff, 0xff);
    caret1_color = Color::FromRgb(0xff, 0x00, 0x00);
    title_bar = true;
    show = true;
    resizable = true;
    noframe = false;
    minimizebox = true;
    maximizebox = true;
    caret = false;
    is_transparency_given = false;
    transparency = 255;
    is_transparent_color_given = false;
    transparent_color = Color::Zero();
    is_top_most = false;
    sysmenu = false;
    tool = false;
    ncpaint = false;

    activate_handler = NULL;
    close_handler = NULL;
    endsession_handler = NULL;
    move_handler = NULL;
    sizing_handler = NULL;
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

WindowBase::WindowBase( Param & param )
{
	FUNC_TRACE;

    pyobj = NULL;

    quit_requested = false;
    active = false;
    
	caret = param.caret;
    caret_blink = 1;
    ime_on = false;
    memset( &caret_rect, 0, sizeof(caret_rect) );
    memset( &ime_rect, 0, sizeof(ime_rect) );
    dirty = false;
    memset( &dirty_rect, 0, sizeof(dirty_rect) );
	perf_fillrect_count = 0;
	perf_drawtext_count = 0;
	perf_drawplane_count = 0;
    ncpaint = param.ncpaint;
    
    activate_handler = param.activate_handler; Py_XINCREF(activate_handler);
    close_handler = param.close_handler; Py_XINCREF(close_handler);
    endsession_handler = param.endsession_handler; Py_XINCREF(endsession_handler);
    move_handler = param.move_handler; Py_XINCREF(move_handler);
    sizing_handler = param.sizing_handler; Py_XINCREF(sizing_handler);
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

	menu = NULL;
}

WindowBase::~WindowBase()
{
	FUNC_TRACE;

	clearPlanes();
    clearCallables();

	((Window_Object*)pyobj)->p = NULL;
    Py_XDECREF(pyobj); pyobj=NULL;
}

void WindowBase::SetPyObject( PyObject * _pyobj )
{
	FUNC_TRACE;

	if( pyobj != _pyobj )
	{
		Py_XDECREF(pyobj);
		pyobj = _pyobj;
		Py_XINCREF(pyobj);
	}
}

void WindowBase::appendDirtyRect( const Rect & rect )
{
	FUNC_TRACE;

	if(dirty)
	{
		if( dirty_rect.left > rect.left ) dirty_rect.left = rect.left;
		if( dirty_rect.top > rect.top ) dirty_rect.top = rect.top;
		if( dirty_rect.right < rect.right ) dirty_rect.right = rect.right;
		if( dirty_rect.bottom < rect.bottom ) dirty_rect.bottom = rect.bottom;
	}
	else
	{
		dirty_rect = rect;
		dirty = true;
	}
}

void WindowBase::clearDirtyRect()
{
	if(dirty)
	{
		if(0)
		{
			printf( "perf info\n" );
			printf( "  dirty_rect : %d, %d, %d, %d\n", dirty_rect.left, dirty_rect.top, dirty_rect.right, dirty_rect.bottom );
			printf( "  perf_fillrect_count : %d\n", perf_fillrect_count );
			printf( "  perf_drawtext_count : %d\n", perf_drawtext_count );
			printf( "  perf_drawplane_count : %d\n", perf_drawplane_count );
		}

		perf_fillrect_count = 0;
		perf_drawtext_count = 0;
		perf_drawplane_count = 0;

		dirty = false;
	}
}

void WindowBase::clearCallables()
{
    Py_XDECREF(activate_handler); activate_handler=NULL;
    Py_XDECREF(close_handler); close_handler=NULL;
    Py_XDECREF(endsession_handler); endsession_handler=NULL;
    Py_XDECREF(move_handler); move_handler=NULL;
    Py_XDECREF(sizing_handler); sizing_handler=NULL;
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

	clearMenuCommands();
	clearPopupMenuCommands();
}

void WindowBase::clearMenuCommands()
{
	for( unsigned int i=0 ; i<menu_commands.size() ; ++i )
	{
		Py_XDECREF(menu_commands[i]);
	}
	menu_commands.clear();
}

void WindowBase::clearPopupMenuCommands()
{
	for( unsigned int i=0 ; i<popup_menu_commands.size() ; ++i )
	{
		Py_XDECREF(popup_menu_commands[i]);
	}
	popup_menu_commands.clear();
}

void WindowBase::clearPlanes()
{
	std::list<PlaneBase*>::iterator i;
	for( i=plane_list.begin() ; i!=plane_list.end() ; i++ )
	{
		delete (*i);
	}
	plane_list.clear();
}

void WindowBase::clear()
{
	FUNC_TRACE;

	clearPlanes();

    memset( &caret_rect, 0, sizeof(caret_rect) );

	Size size;
	getClientSize(&size);

	Rect dirty_rect = { 0, 0, size.cx, size.cy };
	appendDirtyRect( dirty_rect );
}

void WindowBase::setCaretRect( const Rect & rect )
{
	FUNC_TRACE;

	caret_blink = 2;

	appendDirtyRect( caret_rect );

	caret_rect = rect;
	
	appendDirtyRect( caret_rect );
}

void WindowBase::setImeRect( const Rect & rect )
{
	FUNC_TRACE;

	ime_rect = rect;
}

// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------

#if defined(PLATFORM_WIN32)

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

static int getModKey()
{
	int mod = 0;
	if(GetKeyState(VK_MENU)&0xf0)       mod |= 1;
	if(GetKeyState(VK_CONTROL)&0xf0)    mod |= 2;
	if(GetKeyState(VK_SHIFT)&0xf0)      mod |= 4;
	if(GetKeyState(VK_LWIN)&0xf0)       mod |= 8;
	if(GetKeyState(VK_RWIN)&0xf0)       mod |= 8;
	return mod;
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
					if(g.command_info_constructor)
					{
						PyObject * pyarglist2 = Py_BuildValue( "()" );
						command_info = PyEval_CallObject( g.command_info_constructor, pyarglist2 );
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
					int mod = getModKey();

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
					int mod = getModKey();

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
					int mod = getModKey();

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
					int mod = getModKey();

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
					int mod = getModKey();

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

#endif //PLATFORM_WIN32

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

    static const char * kwlist[] = {
        "fg",
        "bg",
        "bg_gradation",
        "line0",
        "line1",
        NULL
    };

    if(!PyArg_ParseTupleAndKeywords( args, kwds, "|OOOOO", (char**)kwlist,
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

	    ((Attribute_Object*)self)->attr.fg_color = Color::FromRgb(r,g,b);
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
	    ((Attribute_Object*)self)->attr.bg_color[0] = Color::FromRgb(r[0],g[0],b[0]);
	    ((Attribute_Object*)self)->attr.bg_color[1] = Color::FromRgb(r[1],g[1],b[1]);
	    ((Attribute_Object*)self)->attr.bg_color[2] = Color::FromRgb(r[2],g[2],b[2]);
	    ((Attribute_Object*)self)->attr.bg_color[3] = Color::FromRgb(r[3],g[3],b[3]);
	}
	else if( bg && PySequence_Check(bg) )
	{
		int r, g, b;
	    if( ! PyArg_ParseTuple( bg, "iii", &r, &g, &b ) )
	        return -1;

	    ((Attribute_Object*)self)->attr.bg = Attribute::BG_Flat;
	    ((Attribute_Object*)self)->attr.bg_color[0] = Color::FromRgb(r,g,b);
	}

	if( line0 && PySequence_Check(line0) )
	{
		int line;
		int r, g, b;
	    if( ! PyArg_ParseTuple( line0, "i(iii)", &line, &r, &g, &b ) )
	        return -1;

	    ((Attribute_Object*)self)->attr.line[0] = line;
	    ((Attribute_Object*)self)->attr.line_color[0] = Color::FromRgb(r,g,b);
	}

	if( line1 && PySequence_Check(line1) )
	{
		int line;
		int r, g, b;
	    if( ! PyArg_ParseTuple( line1, "i(iii)", &line, &r, &g, &b ) )
	        return -1;

	    ((Attribute_Object*)self)->attr.line[1] = line;
	    ((Attribute_Object*)self)->attr.line_color[1] = Color::FromRgb(r,g,b);
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

    ImageBase * image = ((Image_Object*)self)->p;
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

#if defined(PLATFORM_WIN32)
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
#endif // PLATFORM

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

	Color transparent_color = Color::FromRgb(0,0,0);
	if(py_transparent_color)
	{
		int r, g, b;
	    if( ! PyArg_ParseTuple( py_transparent_color, "iii", &r, &g, &b ) )
	        return NULL;

	    transparent_color = Color::FromRgb(r,g,b);
	}

	Image * image;

	if( width<=0 || height<=0 )
	{
		image = new ckit::Image( 0, 0, NULL );
	}
	else
	{
        #if defined(PLATFORM_WIN32)
        // FIXME : Windows のコードに引っ越し
		void * dib_buf = malloc( width * 4 * height );

		_Image_ConvertRGBAtoDIB( (char*)dib_buf, (const char*)buf, width, height, width*4 );

		image = new Image( width, height, (const char *)dib_buf, py_transparent_color ? &transparent_color : 0, halftone!=0 );

		free(dib_buf);
        #else
		image = new Image( width, height, buf, py_transparent_color ? &transparent_color : 0, halftone!=0 );
        #endif
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

static int Font_init( PyObject * self, PyObject * args, PyObject * kwds)
{
	FUNC_TRACE;

    PyObject * pystr_name;
	int size;

    if(!PyArg_ParseTuple( args, "Oi", &pystr_name, &size ))
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

	Font * font = new Font( str_name.c_str(), size );

	((Font_Object*)self)->p = font;
	font->AddRef();

    return 0;
}

static void Font_dealloc(PyObject* self)
{
	FUNC_TRACE;

    FontBase * font = ((Font_Object*)self)->p;
    font->Release();
	self->ob_type->tp_free(self);
}

static PyObject * Font_getCharSize(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	FontBase * font = ((Font_Object*)self)->p;

	PyObject * pyret = Py_BuildValue( "(ii)", font->char_width, font->char_height );
	return pyret;
}

static PyMethodDef Font_methods[] = {
    { "getCharSize", Font_getCharSize, METH_VARARGS, "" },
    {NULL,NULL}
};

PyTypeObject Font_Type = {
    PyVarObject_HEAD_INIT(NULL,0)
    "Font",				/* tp_name */
    sizeof(Font_Object), /* tp_basicsize */
    0,					/* tp_itemsize */
    (destructor)Font_dealloc,/* tp_dealloc */
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
    Font_methods,		/* tp_methods */
    0,					/* tp_members */
    0,					/* tp_getset */
    0, 					/* tp_base */
    0,					/* tp_dict */
    0,					/* tp_descr_get */
    0,					/* tp_descr_set */
    0,					/* tp_dictoffset */
    Font_init,			/* tp_init */
    0,					/* tp_alloc */
    PyType_GenericNew,	/* tp_new */
    0,					/* tp_free */
};

// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------

static int ImagePlane_instance_count = 0;

static int ImagePlane_init( PyObject * self, PyObject * args, PyObject * kwds)
{
	FUNC_TRACE;

	ImagePlane_instance_count ++;
	PRINTF("ImagePlane_instance_count=%d\n", ImagePlane_instance_count);

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

    ImagePlane * plane = new ImagePlane( ((Window_Object*)window)->p, x, y, width, height, priority );

    plane->SetPyObject(self);

    ((ImagePlane_Object*)self)->p = plane;

	std::list<PlaneBase*>::iterator i;
	std::list<PlaneBase*> & plane_list = ((Window_Object*)window)->p->plane_list;
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

static void ImagePlane_dealloc(PyObject* self)
{
	FUNC_TRACE;

	ImagePlane_instance_count --;
	PRINTF("ImagePlane_instance_count=%d\n", ImagePlane_instance_count);

    ImagePlaneBase * plane = ((ImagePlane_Object*)self)->p;
	if(plane){ plane->SetPyObject(NULL); }
	self->ob_type->tp_free(self);
}

static PyObject * ImagePlane_destroy(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((ImagePlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	((ImagePlane_Object*)self)->p->window->plane_list.remove( ((ImagePlane_Object*)self)->p );

	delete ((ImagePlane_Object*)self)->p;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * ImagePlane_show(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	int show;
	if( ! PyArg_ParseTuple(args, "i", &show ) )
        return NULL;

	if( ! ((ImagePlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    ((ImagePlane_Object*)self)->p->Show( show!=0 );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * ImagePlane_getPosition(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((ImagePlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	return Py_BuildValue( "(ii)", ((ImagePlane_Object*)self)->p->x, ((ImagePlane_Object*)self)->p->y );
}

static PyObject * ImagePlane_setPosition(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	int x, y;
	if( ! PyArg_ParseTuple(args, "(ii)", &x, &y ) )
        return NULL;

	if( ! ((ImagePlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    ((ImagePlane_Object*)self)->p->SetPosition( x, y );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * ImagePlane_getSize(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((ImagePlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	return Py_BuildValue( "(ii)", ((ImagePlane_Object*)self)->p->width, ((ImagePlane_Object*)self)->p->height );
}

static PyObject * ImagePlane_setSize(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	int width, height;
	if( ! PyArg_ParseTuple(args, "(ii)", &width, &height ) )
        return NULL;

	if( ! ((ImagePlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    ((ImagePlane_Object*)self)->p->SetSize( width, height );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * ImagePlane_getPriority(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((ImagePlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	return Py_BuildValue( "f", ((ImagePlane_Object*)self)->p->priority );
}

static PyObject * ImagePlane_setPriority(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	float priority;
	if( ! PyArg_ParseTuple(args, "f", &priority ) )
        return NULL;

	if( ! ((ImagePlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    ((ImagePlane_Object*)self)->p->SetPriority( priority );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * ImagePlane_getImage(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((ImagePlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    ImageBase * image = ((ImagePlane_Object*)self)->p->image;
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

static PyObject * ImagePlane_setImage(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

    PyObject * image;
	if( ! PyArg_ParseTuple(args, "O", &image ) )
        return NULL;

	if( ! ((ImagePlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    ((ImagePlane_Object*)self)->p->SetImage( ((Image_Object*)image)->p );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef ImagePlane_methods[] = {

	{ "destroy", ImagePlane_destroy, METH_VARARGS, "" },

	{ "show", ImagePlane_show, METH_VARARGS, "" },

	{ "getPosition", ImagePlane_getPosition, METH_VARARGS, "" },
	{ "getSize", ImagePlane_getSize, METH_VARARGS, "" },
	{ "getPriority", ImagePlane_getPriority, METH_VARARGS, "" },
	{ "getImage", ImagePlane_getImage, METH_VARARGS, "" },

	{ "setPosition", ImagePlane_setPosition, METH_VARARGS, "" },
	{ "setSize", ImagePlane_setSize, METH_VARARGS, "" },
	{ "setPriority", ImagePlane_setPriority, METH_VARARGS, "" },
	{ "setImage", ImagePlane_setImage, METH_VARARGS, "" },

	{NULL,NULL}
};

PyTypeObject ImagePlane_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ImagePlane",		/* tp_name */
	sizeof(ImagePlane_Object), /* tp_basicsize */
	0,					/* tp_itemsize */
	ImagePlane_dealloc,		/* tp_dealloc */
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
	ImagePlane_methods,		/* tp_methods */
	0,					/* tp_members */
	0,					/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
	0,					/* tp_dictoffset */
	ImagePlane_init,			/* tp_init */
	0,					/* tp_alloc */
	PyType_GenericNew,	/* tp_new */
	0,					/* tp_free */
};

// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------

static int TextPlane_instance_count = 0;

static int TextPlane_init( PyObject * self, PyObject * args, PyObject * kwds)
{
	FUNC_TRACE;

	TextPlane_instance_count ++;
	PRINTF("TextPlane_instance_count=%d\n", TextPlane_instance_count);

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

    TextPlane * plane = new TextPlane( ((Window_Object*)window)->p, x, y, width, height, priority );

    plane->SetPyObject(self);

    ((TextPlane_Object*)self)->p = plane;

	std::list<PlaneBase*>::iterator i;
	std::list<PlaneBase*> & plane_list = ((Window_Object*)window)->p->plane_list;
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

static void TextPlane_dealloc(PyObject* self)
{
	FUNC_TRACE;

	TextPlane_instance_count --;
	PRINTF("TextPlane_instance_count=%d\n", TextPlane_instance_count);

    TextPlaneBase * plane = ((TextPlane_Object*)self)->p;
	if(plane){ plane->SetPyObject(NULL); }
	self->ob_type->tp_free(self);
}

static PyObject * TextPlane_destroy(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	((TextPlane_Object*)self)->p->window->plane_list.remove( ((TextPlane_Object*)self)->p );

	delete ((TextPlane_Object*)self)->p;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * TextPlane_show(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	int show;
	if( ! PyArg_ParseTuple(args, "i", &show ) )
        return NULL;

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    ((TextPlane_Object*)self)->p->Show( show!=0 );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * TextPlane_getPosition(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	return Py_BuildValue( "(ii)", ((TextPlane_Object*)self)->p->x, ((TextPlane_Object*)self)->p->y );
}

static PyObject * TextPlane_setPosition(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	int x, y;
	if( ! PyArg_ParseTuple(args, "(ii)", &x, &y ) )
        return NULL;

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    ((TextPlane_Object*)self)->p->SetPosition( x, y );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * TextPlane_getSize(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	return Py_BuildValue( "(ii)", ((TextPlane_Object*)self)->p->width, ((TextPlane_Object*)self)->p->height );
}

static PyObject * TextPlane_setSize(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	int width, height;
	if( ! PyArg_ParseTuple(args, "(ii)", &width, &height ) )
        return NULL;

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    ((TextPlane_Object*)self)->p->SetSize( width, height );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * TextPlane_getPriority(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	return Py_BuildValue( "f", ((TextPlane_Object*)self)->p->priority );
}

static PyObject * TextPlane_setPriority(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	float priority;
	if( ! PyArg_ParseTuple(args, "f", &priority ) )
        return NULL;

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    ((TextPlane_Object*)self)->p->SetPriority( priority );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * TextPlane_getFont(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    FontBase * font = ((TextPlane_Object*)self)->p->font;
    if(font)
    {
		Font_Object * pyfont;
		pyfont = PyObject_New( Font_Object, &Font_Type );
		pyfont->p = font;
		font->AddRef();
		return (PyObject*)pyfont;
    }
    else
    {
	    Py_INCREF(Py_None);
	    return Py_None;
    }
}

static PyObject * TextPlane_setFont(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

    PyObject * font;
	if( ! PyArg_ParseTuple(args, "O", &font ) )
        return NULL;

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    ((TextPlane_Object*)self)->p->SetFont( ((Font_Object*)font)->p );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * TextPlane_putString(PyObject* self, PyObject* args, PyObject * kwds)
{
	FUNC_TRACE;

	int x;
	int y;
	int width;
	int height;
	PyObject * pyattr;
	PyObject * pystr;
	int offset=0;

    static const char * kwlist[] = {
        "x",
        "y",
        "width",
        "height",
        "attr",
        "str",
        "offset",
        NULL
    };

    if( ! PyArg_ParseTupleAndKeywords( args, kwds, "iiiiOO|i", (char**)kwlist,
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

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    TextPlaneBase * textPlane = ((TextPlane_Object*)self)->p;

    textPlane->PutString( x, y, width, height, ((Attribute_Object*)pyattr)->attr, str.c_str(), offset );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * TextPlane_scroll(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

	int x, y, width, height, delta_x, delta_y;

    if( ! PyArg_ParseTuple(args, "iiiiii", &x, &y, &width, &height, &delta_x, &delta_y ) )
        return NULL;

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    TextPlaneBase * textPlane = ((TextPlane_Object*)self)->p;

	textPlane->Scroll( x, y, width, height, delta_x, delta_y );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * TextPlane_getCharSize(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	TextPlaneBase * textPlane = ((TextPlane_Object*)self)->p;

	PyObject * pyret = Py_BuildValue( "(ii)", textPlane->font->char_width, textPlane->font->char_height );
	return pyret;
}

static PyObject * TextPlane_charToScreen(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	int x;
	int y;

	if( ! PyArg_ParseTuple(args, "ii", &x, &y ) )
        return NULL;

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	TextPlaneBase * textPlane = ((TextPlane_Object*)self)->p;

    ckit::Point point;
	point.x = x * textPlane->font->char_width + textPlane->x;
	point.y = y * textPlane->font->char_height + textPlane->y;

	textPlane->window->clientToScreen(&point);

	PyObject * pyret = Py_BuildValue( "(ii)", point.x, point.y );
	return pyret;
}

static PyObject * TextPlane_charToClient(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	int x;
	int y;

	if( ! PyArg_ParseTuple(args, "ii", &x, &y ) )
        return NULL;

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	TextPlaneBase * textPlane = ((TextPlane_Object*)self)->p;

    ckit::Point point;
	point.x = x * textPlane->font->char_width + textPlane->x;
	point.y = y * textPlane->font->char_height + textPlane->y;

	PyObject * pyret = Py_BuildValue( "(ii)", point.x, point.y );
	return pyret;
}

static PyObject * TextPlane_getStringWidth(PyObject* self, PyObject* args)
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

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    TextPlaneBase * textPlane = ((TextPlane_Object*)self)->p;

    int width = textPlane->GetStringWidth( str.c_str(), tab_width, offset );

    PyObject * pyret = Py_BuildValue("i",width);
    return pyret;
}

static PyObject * TextPlane_getStringColumns(PyObject* self, PyObject* args)
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

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    TextPlaneBase * textPlane = ((TextPlane_Object*)self)->p;

	size_t num = str.length()+1;
	int * columns = new int[num];

    textPlane->GetStringWidth( str.c_str(), tab_width, offset, columns );

	PyObject * pyret = PyTuple_New(num);
	for(int i=0 ; i<num ; ++i )
	{
		PyTuple_SET_ITEM( pyret, i, PyLong_FromLong(columns[i]) );
	}
	delete [] columns;
	
	return pyret;
}

static PyObject * TextPlane_setCaretPosition(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	int x;
	int y;

    if( ! PyArg_ParseTuple(args, "ii", &x, &y ) )
        return NULL;

	if( ! ((TextPlane_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    TextPlaneBase * textPlane = ((TextPlane_Object*)self)->p;

    textPlane->SetCaretPosition( x, y );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef TextPlane_methods[] = {

	{ "destroy", TextPlane_destroy, METH_VARARGS, "" },

	{ "show", TextPlane_show, METH_VARARGS, "" },

	{ "getPosition", TextPlane_getPosition, METH_VARARGS, "" },
	{ "getSize", TextPlane_getSize, METH_VARARGS, "" },
	{ "getPriority", TextPlane_getPriority, METH_VARARGS, "" },
	{ "getFont", TextPlane_getFont, METH_VARARGS, "" },

	{ "setPosition", TextPlane_setPosition, METH_VARARGS, "" },
	{ "setSize", TextPlane_setSize, METH_VARARGS, "" },
	{ "setPriority", TextPlane_setPriority, METH_VARARGS, "" },
	{ "setFont", TextPlane_setFont, METH_VARARGS, "" },

    { "putString", (PyCFunction)TextPlane_putString, METH_VARARGS|METH_KEYWORDS, "" },
	{ "scroll", TextPlane_scroll, METH_VARARGS, "" },

    { "getCharSize", TextPlane_getCharSize, METH_VARARGS, "" },
    { "charToScreen", TextPlane_charToScreen, METH_VARARGS, "" },
    { "charToClient", TextPlane_charToClient, METH_VARARGS, "" },
    { "getStringWidth", TextPlane_getStringWidth, METH_VARARGS, "" },
    { "getStringColumns", TextPlane_getStringColumns, METH_VARARGS, "" },

	{ "setCaretPosition", TextPlane_setCaretPosition, METH_VARARGS, "" },

	{NULL,NULL}
};

PyTypeObject TextPlane_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"TextPlane",		/* tp_name */
	sizeof(TextPlane_Object), /* tp_basicsize */
	0,					/* tp_itemsize */
	TextPlane_dealloc,		/* tp_dealloc */
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
	TextPlane_methods,		/* tp_methods */
	0,					/* tp_members */
	0,					/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
	0,					/* tp_dictoffset */
	TextPlane_init,			/* tp_init */
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

    static const char * kwlist[] = {
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

    if(!PyArg_ParseTupleAndKeywords( args, kwds, "|OOOOOOOi", (char**)kwlist,
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

    #if defined(PLATFORM_WIN32)
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    #elif defined(PLATFORM_MAC)
    int x = 0; // FIXME: auto-layout
    int y = 0; // FIXME: auto-layout
    #endif
    int width = 100;
    int height = 100;
    int origin = 0;
    PyObject * parent_window = NULL;
    PyObject * pybg_color = NULL;
    PyObject * pyframe_color = NULL;
    PyObject * pycaret0_color = NULL;
    PyObject * pycaret1_color = NULL;
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
    int caret = 0;
    int sysmenu = 0;
    int tool = 0;
    int ncpaint = 0;
    PyObject * activate_handler = NULL;
    PyObject * close_handler = NULL;
    PyObject * endsession_handler = NULL;
    PyObject * move_handler = NULL;
    PyObject * sizing_handler = NULL;
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

    static const char * kwlist[] = {

        "x",
        "y",
        "width",
        "height",
        "origin",

        "parent_window",
        "bg_color",
        "frame_color",
        "caret0_color",
        "caret1_color",

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
        "caret",
        "sysmenu",
        "tool",
        "ncpaint",

        "activate_handler",
        "close_handler",
        "endsession_handler",
        "move_handler",
        "sizing_handler",
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
    	"OOOOO"
    	"iOOiO"
    	"iiiiiiiii"
    	"OOOOOOOOOOOOOOOOOOOOOOO", (char**)kwlist,

        &x,
        &y,
        &width,
        &height,
        &origin,

        &parent_window,
        &pybg_color,
        &pyframe_color,
        &pycaret0_color,
        &pycaret1_color,

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
        &caret,
        &sysmenu,
        &tool,
        &ncpaint,

	    &activate_handler,
	    &close_handler,
	    &endsession_handler,
	    &move_handler,
	    &sizing_handler,
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
    param.winsize_w = width;
    param.winsize_h = height;
    param.origin = origin;
    if(!parent_window)
    {
    	param.parent_window = NULL;
    	param.parent_window_hwnd = NULL;
    }
    else if(PyLong_Check(parent_window))
    {
    	param.parent_window = NULL;
    	param.parent_window_hwnd = (WindowHandle)PyLong_AS_LONG(parent_window);
    }
    else
    {
    	param.parent_window = ((Window_Object*)parent_window)->p;
    	param.parent_window_hwnd = param.parent_window->getHandle();
    }
	if(pybg_color)
	{
		int r, g, b;
	    if( ! PyArg_ParseTuple( pybg_color, "iii", &r, &g, &b ) )
	        return -1;
	    param.bg_color = Color::FromRgb(r,g,b);
	}
	if(pyframe_color)
	{
		int r, g, b;
	    if( ! PyArg_ParseTuple( pyframe_color, "iii", &r, &g, &b ) )
	        return -1;
	    param.frame_color = Color::FromRgb(r,g,b);
	}
	if(pycaret0_color)
	{
		int r, g, b;
	    if( ! PyArg_ParseTuple( pycaret0_color, "iii", &r, &g, &b ) )
	        return -1;
	    param.caret0_color = Color::FromRgb(r,g,b);
	}
	if(pycaret1_color)
	{
		int r, g, b;
	    if( ! PyArg_ParseTuple( pycaret1_color, "iii", &r, &g, &b ) )
	        return -1;
	    param.caret1_color = Color::FromRgb(r,g,b);
	}
    param.border_size = border_size;

	if(pytransparency && PyLong_Check(pytransparency))
	{
	    param.transparency = (int)PyLong_AS_LONG(pytransparency);
	    param.is_transparency_given = true;
	}

	if(pytransparent_color)
	{
		int r, g, b;
	    if( ! PyArg_ParseTuple( pytransparent_color, "iii", &r, &g, &b ) )
	        return -1;
	    param.transparent_color = Color::FromRgb(r,g,b);
	    param.is_transparent_color_given = true;
	}
    param.title_bar = title_bar!=0;
    param.title = str_title;
    param.show = show!=0;
    param.resizable = resizable!=0;
    param.noframe = noframe!=0;
    param.minimizebox = minimizebox!=0;
    param.minimizebox = minimizebox!=0;
    param.caret = caret!=0;
    param.sysmenu = sysmenu!=0;
    param.tool = tool!=0;
    param.ncpaint = ncpaint!=0;
    param.activate_handler = activate_handler;
    param.close_handler = close_handler;
    param.endsession_handler = endsession_handler;
    param.move_handler = move_handler;
    param.sizing_handler = sizing_handler;
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

    WindowBase * window = ((Window_Object*)self)->p;
	if(window){ window->SetPyObject(NULL); }

    self->ob_type->tp_free(self);
}

static PyObject * Window_getHandle(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

    WindowBase * window = ((Window_Object*)self)->p;

    WindowHandle hwnd = window->getHandle();

    PyObject * pyret = Py_BuildValue("i",hwnd);
    return pyret;
}

static PyObject * Window_getInstanceHandle(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

#if defined(PLATFORM_WIN32)
    HINSTANCE hinstance = GetModuleHandle(NULL);
#else
    int hinstance = 0;
#endif

    PyObject * pyret = Py_BuildValue("i",hinstance);
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

    WindowBase * window = ((Window_Object*)self)->p;

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

    WindowBase * window = ((Window_Object*)self)->p;

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

    WindowBase * window = ((Window_Object*)self)->p;

    delete window;

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

    WindowBase * window = ((Window_Object*)self)->p;

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

    WindowBase * window = ((Window_Object*)self)->p;

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

    WindowBase * window = ((Window_Object*)self)->p;

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

    WindowBase * window = ((Window_Object*)self)->p;

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

    WindowBase * window = ((Window_Object*)self)->p;

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

    WindowBase * window = ((Window_Object*)self)->p;

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

    WindowBase * window = ((Window_Object*)self)->p;

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

    WindowBase * window = ((Window_Object*)self)->p;

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

    WindowBase * window = ((Window_Object*)self)->p;

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

    WindowBase * window = ((Window_Object*)self)->p;

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

    WindowBase * window = ((Window_Object*)self)->p;

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

    WindowBase * window = ((Window_Object*)self)->p;

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

    WindowBase * window = ((Window_Object*)self)->p;

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

    WindowBase * window = ((Window_Object*)self)->p;

    window->clear();

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

    WindowBase * window = ((Window_Object*)self)->p;

	window->flushPaint();

    Py_INCREF(Py_None);
    return Py_None;
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

	WindowBase * window = ((Window_Object*)self)->p;
	
    ckit::Rect rect;

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

    WindowBase * window = ((Window_Object*)self)->p;

    window->enableIme( enable!=0 );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_setPositionAndSize(PyObject* self, PyObject* args, PyObject * kwds)
{
	//FUNC_TRACE;

	int x;
	int y;
	int width;
	int height;
	int origin;

    static const char * kwlist[] = {
        "x",
        "y",
        "width",
        "height",
        "origin",
        NULL
    };

    if(!PyArg_ParseTupleAndKeywords( args, kwds, "iiiii", (char**)kwlist,
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

    WindowBase * window = ((Window_Object*)self)->p;

    window->setPositionAndSize( x, y, width, height, origin );

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Window_messageLoop(PyObject* self, PyObject* args, PyObject * kwds)
{
	//FUNC_TRACE;

	PyObject * continue_cond_func = NULL;

    static const char * kwlist[] = {
        "continue_cond_func",
        NULL
    };

    if(!PyArg_ParseTupleAndKeywords( args, kwds, "|O", (char**)kwlist,
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

    WindowBase * window = ((Window_Object*)self)->p;

    #if defined(PLATFORM_WIN32)
    // FIXME : Windows用ソースに移動
    MSG msg;
    for(;;)
    {
        while( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
        {
            if(msg.message==WM_QUIT)
            {
                goto end;
            }

            WindowBase * window = ((Window_Object*)self)->p;

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
    #elif defined(PLATFORM_MAC)
    window->messageLoop(continue_cond_func);
    #endif //PLATFORM_WIN32

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

    WindowBase * window = ((Window_Object*)self)->p;
    
    #if defined(PLATFORM_WIN32)
    MSG msg;
    while( PeekMessage( &msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE ) )
    {
    }
    #else
    window->removeKeyMessage();
    #endif // PLATFORM

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

	WindowBase * window = ((Window_Object*)self)->p;

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

	WindowBase * window = ((Window_Object*)self)->p;

    ckit::Rect rect;
	window->getWindowRect(&rect);

	PyObject * pyret = Py_BuildValue( "(iiii)", rect.left, rect.top, rect.right, rect.bottom );
	return pyret;
}

static PyObject * Window_getClientSize(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	WindowBase * window = ((Window_Object*)self)->p;

    ckit::Size size;
	window->getClientSize(&size);

	PyObject * pyret = Py_BuildValue( "(ii)", size.cx, size.cy );
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

	WindowBase * window = ((Window_Object*)self)->p;

    ckit::Rect rect;
	window->getNormalWindowRect(&rect);

	PyObject * pyret = Py_BuildValue( "(iiii)", rect.left, rect.top, rect.right, rect.bottom );
	return pyret;
}

static PyObject * Window_getNormalClientSize(PyObject* self, PyObject* args)
{
	//FUNC_TRACE;

	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;

	if( ! ((Window_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}

	WindowBase * window = ((Window_Object*)self)->p;

    ckit::Size size;
	window->getNormalClientSize(&size);

	PyObject * pyret = Py_BuildValue( "(ii)", size.cx, size.cy );
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

	WindowBase * window = ((Window_Object*)self)->p;

    ckit::Point point;
	point.x = x;
	point.y = y;

	window->screenToClient(&point);

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

	WindowBase * window = ((Window_Object*)self)->p;

    ckit::Point point;
	point.x = x;
	point.y = y;

	window->clientToScreen(&point);

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

	WindowBase * window = ((Window_Object*)self)->p;

	Py_XINCREF(func);

    TimerInfo timer_info;
    timer_info.pyobj = func;
    timer_info.interval = interval;
    
	window->setTimer( &timer_info );
    
	window->timer_list.push_back(timer_info);

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

	WindowBase * window = ((Window_Object*)self)->p;

	for( std::list<TimerInfo>::iterator i=window->timer_list.begin(); i!=window->timer_list.end() ; i++ )
	{
		if( (i->pyobj)==NULL ) continue;

		if( PyObject_RichCompareBool( func, (i->pyobj), Py_EQ )==1 )
		{
			window->killTimer(&*i);

			Py_XDECREF( i->pyobj );
			i->pyobj = NULL;

			// Note : リストからの削除は、ここではなく呼び出しの後、timer_list_ref_count が 0 であることを確認しながら行う

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

	WindowBase * window = ((Window_Object*)self)->p;

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

	WindowBase * window = ((Window_Object*)self)->p;

	window->setHotKey(vk,mod,func);
	
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

	WindowBase * window = ((Window_Object*)self)->p;

	window->killHotKey(func);

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

	WindowBase * window = ((Window_Object*)self)->p;

	window->setText( str.c_str() );

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

	WindowBase * window = ((Window_Object*)self)->p;
	window->setBGColor( Color::FromRgb(r,g,b) );

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

	WindowBase * window = ((Window_Object*)self)->p;
	
	if( pycolor && PySequence_Check(pycolor) )
	{
		int r, g, b;
	    if( ! PyArg_ParseTuple( pycolor, "iii", &r, &g, &b ) )
	        return NULL;

		window->setFrameColor( Color::FromRgb(r,g,b) );
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Window_setCaretColor(PyObject* self, PyObject* args)
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

	WindowBase * window = ((Window_Object*)self)->p;
	
	if( pycolor0 && PySequence_Check(pycolor0) && pycolor1 && PySequence_Check(pycolor1) )
	{
		int r0, g0, b0;
	    if( ! PyArg_ParseTuple( pycolor0, "iii", &r0, &g0, &b0 ) )
	        return NULL;

		int r1, g1, b1;
	    if( ! PyArg_ParseTuple( pycolor1, "iii", &r1, &g1, &b1 ) )
	        return NULL;

		window->setCaretColor( Color::FromRgb(r0,g0,b0), Color::FromRgb(r1,g1,b1) );
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

	WindowBase * window = ((Window_Object*)self)->p;
	
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

	WindowBase * window = ((Window_Object*)self)->p;

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

	WindowBase * window = ((Window_Object*)self)->p;

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

	WindowBase * window = ((Window_Object*)self)->p;

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

	WindowBase * window = ((Window_Object*)self)->p;

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

	WindowBase * window = ((Window_Object*)self)->p;

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

static PyObject * Window_popupMenu(PyObject* self, PyObject* args, PyObject * kwds)
{
	//FUNC_TRACE;

	int x;
	int y;
	PyObject * items;

    static const char * kwlist[] = {
        "x",
        "y",
        "items",
        NULL
    };

    if(!PyArg_ParseTupleAndKeywords( args, kwds, "iiO", (char**)kwlist,
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

    WindowBase * window = ((Window_Object*)self)->p;

	bool result = window->popupMenu( x, y, items );
	if(result)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}
	else
	{
		return NULL;
	}
}

static PyObject * Window_sendIpc(PyObject* self, PyObject* args)
{
	FUNC_TRACE;

#if defined(PLATFORM_WIN32)
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
#endif // PLATFORM

	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef Window_methods[] = {
    { "getHandle", Window_getHandle, METH_VARARGS, "" },
    { "getInstanceHandle", Window_getInstanceHandle, METH_VARARGS, "" },
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
    { "flushPaint", Window_flushPaint, METH_VARARGS, "" },
    { "messageLoop", (PyCFunction)Window_messageLoop, METH_VARARGS|METH_KEYWORDS, "" },
    { "removeKeyMessage", Window_removeKeyMessage, METH_VARARGS, "" },
    { "quit", Window_quit, METH_VARARGS, "" },
    { "getWindowRect", Window_getWindowRect, METH_VARARGS, "" },
    { "getClientSize", Window_getClientSize, METH_VARARGS, "" },
    { "getNormalWindowRect", Window_getNormalWindowRect, METH_VARARGS, "" },
    { "getNormalClientSize", Window_getNormalClientSize, METH_VARARGS, "" },
    { "screenToClient", Window_screenToClient, METH_VARARGS, "" },
    { "clientToScreen", Window_clientToScreen, METH_VARARGS, "" },
    { "setTimer", Window_setTimer, METH_VARARGS, "" },
    { "killTimer", Window_killTimer, METH_VARARGS, "" },
    { "delayedCall", Window_delayedCall, METH_VARARGS, "" },
    { "setHotKey", Window_setHotKey, METH_VARARGS, "" },
    { "killHotKey", Window_killHotKey, METH_VARARGS, "" },
    { "setImeRect", Window_setImeRect, METH_VARARGS, "" },
    { "enableIme", Window_enableIme, METH_VARARGS, "" },
    { "setPositionAndSize", (PyCFunction)Window_setPositionAndSize, METH_VARARGS|METH_KEYWORDS, "" },
    { "setTitle", (PyCFunction)Window_setTitle, METH_VARARGS, "" },
    { "setBGColor", (PyCFunction)Window_setBGColor, METH_VARARGS, "" },
    { "setFrameColor", (PyCFunction)Window_setFrameColor, METH_VARARGS, "" },
    { "setCaretColor", (PyCFunction)Window_setCaretColor, METH_VARARGS, "" },
    { "setMenu", (PyCFunction)Window_setMenu, METH_VARARGS, "" },
    { "setCapture", (PyCFunction)Window_setCapture, METH_VARARGS, "" },
    { "releaseCapture", (PyCFunction)Window_releaseCapture, METH_VARARGS, "" },
    { "setMouseCursor", (PyCFunction)Window_setMouseCursor, METH_VARARGS, "" },
    { "drag", (PyCFunction)Window_drag, METH_VARARGS, "" },
    { "enumFonts", (PyCFunction)Window_enumFonts, METH_VARARGS, "" },
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

#if defined(PLATFORM_WIN32)

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

    static const char * kwlist[] = {
        "title",
        "lbuttondown_handler",
        "lbuttonup_handler",
        "rbuttondown_handler",
        "rbuttonup_handler",
        "lbuttondoubleclick_handler",
        NULL
    };

    if(!PyArg_ParseTupleAndKeywords( args, kwds, "|OOOOOO", (char**)kwlist,
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

#endif //PLATFORM_WIN32

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

#if defined(PLATFORM_WIN32)
// FIXME : not used ?
static void Line_static_term()
{
	Py_DECREF(line_cr);
	Py_DECREF(line_lf);
	Py_DECREF(line_crlf);
	Py_DECREF(line_empty);
}
#endif // PLATFORM

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

    static const char * kwlist[] = {
        "s",
        NULL
    };

    if(!PyArg_ParseTupleAndKeywords( args, kwds, "u#", (char**)kwlist,
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
				int lineend = _Line_CheckLineEnd( PyUnicode_AS_UNICODE(value), (int)PyUnicode_GET_SIZE(value) );
				
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
				bg = (int)PyLong_AS_LONG(value);
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
				bookmark = (int)PyLong_AS_LONG(value);
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

// FIXME : rename to initializeWindowSystem or something
static PyObject * Module_registerWindowClass( PyObject * self, PyObject * args )
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

    #if defined(PLATFORM_WIN32)
    
    // FIXME : prepare and use Window::initializeSystem instead.
    
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

    #elif defined(PLATFORM_MAC)
    
    Window::initializeSystem( prefix.c_str() );
    
    #endif

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * Module_registerCommandInfoConstructor( PyObject * self, PyObject * args )
{
	PyObject * _command_info_constructor;

	if( ! PyArg_ParseTuple(args,"O", &_command_info_constructor ) )
		return NULL;
	
	Py_XDECREF(g->command_info_constructor);
	g->command_info_constructor = _command_info_constructor;
	Py_INCREF(g->command_info_constructor);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Module_setGlobalOption( PyObject * self, PyObject * args )
{
	FUNC_TRACE;

    int option;
    int enable;

    if( ! PyArg_ParseTuple(args, "ii", &option, &enable ) )
        return NULL;

	switch(option)
	{
	case GLOBAL_OPTION_XXXX:
		{
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
#if defined(PLATFORM_WIN32)
	static DWORD tick_prev = GetTickCount();
	DWORD tick = GetTickCount();
	DWORD delta = tick-tick_prev;
	tick_prev = tick;
#else
    int delta = 0;
#endif
	
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

static PyObject * Module_enableBlockDetector( PyObject * self, PyObject * args )
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

static PyObject * Module_setBlockDetector( PyObject * self, PyObject * args )
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

static PyObject * Module_setClipboardText( PyObject * self, PyObject * args )
{
	FUNC_TRACE;
    
	PyObject * pystr;
    
    if( ! PyArg_ParseTuple(args, "O", &pystr ) )
        return NULL;
    
    std::wstring str;
    if( !PythonUtil::PyStringToWideString( pystr, &str ) )
    {
    	return NULL;
    }
    
	g->setClipboardText( str.c_str() );
    
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Module_getClipboardText( PyObject * self, PyObject * args )
{
	FUNC_TRACE;
    
    if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;
    
    std::wstring str = g->getClipboardText();

    PyObject * pyret = Py_BuildValue( "u", str.c_str() );
    return pyret;
}

static PyObject * Module_getClipboardChangeCount( PyObject * self, PyObject * args )
{
	FUNC_TRACE;
    
    if( ! PyArg_ParseTuple(args, "" ) )
    return NULL;
    
    int count = g->getClipboardChangeCount();
    
    PyObject * pyret = Py_BuildValue( "i", count );
    return pyret;
}

static PyObject * Module_beep( PyObject * self, PyObject * args )
{
	FUNC_TRACE;
    
    if( ! PyArg_ParseTuple(args, "" ) )
    return NULL;
    
    g->beep();
    
	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef ckit_funcs[] =
{
    { "registerWindowClass", Module_registerWindowClass, METH_VARARGS, "" },
    { "registerCommandInfoConstructor", Module_registerCommandInfoConstructor, METH_VARARGS, "" },
    { "setGlobalOption", Module_setGlobalOption, METH_VARARGS, "" },
    { "enableBlockDetector", Module_enableBlockDetector, METH_VARARGS, "" },
    { "setBlockDetector", Module_setBlockDetector, METH_VARARGS, "" },
    { "setClipboardText", Module_setClipboardText, METH_VARARGS, "" },
    { "getClipboardText", Module_getClipboardText, METH_VARARGS, "" },
    { "getClipboardChangeCount", Module_getClipboardChangeCount, METH_VARARGS, "" },
    { "beep", Module_beep, METH_VARARGS, "" },
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

PyMODINIT_FUNC PyInit_ckitcore(void)
{
	FUNC_TRACE;

    if( PyType_Ready(&Attribute_Type)<0 ) return NULL;
    if( PyType_Ready(&Image_Type)<0 ) return NULL;
    if( PyType_Ready(&Font_Type)<0 ) return NULL;
    if( PyType_Ready(&ImagePlane_Type)<0 ) return NULL;
    if( PyType_Ready(&TextPlane_Type)<0 ) return NULL;
    if( PyType_Ready(&MenuNode_Type)<0 ) return NULL;
    if( PyType_Ready(&Window_Type)<0 ) return NULL;
    if( PyType_Ready(&Line_Type)<0 ) return NULL;
#if defined(PLATFORM_WIN32)
    if( PyType_Ready(&TaskTrayIcon_Type)<0 ) return NULL;
#endif
    
    PyObject *m, *d;

    m = PyModule_Create(&ckitcore_module);
    if(m == NULL) return NULL;

    Py_INCREF(&Attribute_Type);
    PyModule_AddObject( m, "Attribute", (PyObject*)&Attribute_Type );

    Py_INCREF(&Image_Type);
    PyModule_AddObject( m, "Image", (PyObject*)&Image_Type );

    Py_INCREF(&Font_Type);
    PyModule_AddObject( m, "Font", (PyObject*)&Font_Type );

    Py_INCREF(&ImagePlane_Type);
    PyModule_AddObject( m, "ImagePlane", (PyObject*)&ImagePlane_Type );

    Py_INCREF(&TextPlane_Type);
    PyModule_AddObject( m, "TextPlane", (PyObject*)&TextPlane_Type );

    Py_INCREF(&MenuNode_Type);
    PyModule_AddObject( m, "MenuNode", (PyObject*)&MenuNode_Type );

    Py_INCREF(&Window_Type);
    PyModule_AddObject( m, "Window", (PyObject*)&Window_Type );

    Py_INCREF(&Line_Type);
    PyModule_AddObject( m, "Line", (PyObject*)&Line_Type );

#if defined(PLATFORM_WIN32)
    Py_INCREF(&TaskTrayIcon_Type);
    PyModule_AddObject( m, "TaskTrayIcon", (PyObject*)&TaskTrayIcon_Type );
#endif
    
	Line_static_init();

    d = PyModule_GetDict(m);

    g->Error = PyErr_NewException( MODULE_NAME".Error", NULL, NULL);
    PyDict_SetItemString( d, "Error", g->Error );

    if( PyErr_Occurred() )
    {
        Py_FatalError( "can't initialize module " MODULE_NAME );
    }

	return m;
}
