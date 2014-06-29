#include <vector>
#include <algorithm>

#include <windows.h>

#include "pythonutil.h"
#include "ckitcore_win.h"

using namespace ckit;

//-----------------------------------------------------------------------------

#define MODULE_NAME "ckitcore"

static std::wstring WINDOW_CLASS_NAME 			= L"CkitWindowClass";
static std::wstring TASKTRAY_WINDOW_CLASS_NAME  = L"CkitTaskTrayWindowClass";

const int WM_USER_NTFYICON  = WM_USER + 100;
const int ID_MENUITEM       = 256;
const int ID_MENUITEM_MAX   = 256+1024-1;
const int ID_POPUP_MENUITEM       = ID_MENUITEM_MAX+1;
const int ID_POPUP_MENUITEM_MAX   = ID_POPUP_MENUITEM+1024;

const int TIMER_PAINT		   			= 0x101;
const int TIMER_PAINT_INTERVAL 			= 10;
const int TIMER_CARET_BLINK   			= 0x102;

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

ImageWin::ImageWin( int _width, int _height, const char * pixels, const Color * _transparent_color, bool _halftone )
	:
	ImageBase(_width,_height,pixels,_transparent_color,_halftone)
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

ImageWin::~ImageWin()
{
	FUNC_TRACE;

	DeleteObject(handle);
}

//-----------------------------------------------------------------------------

FontWin::FontWin( const wchar_t * name, int height )
	:
	FontBase(),
	handle(0)
{
	FUNC_TRACE;

    memset( &logfont, 0, sizeof(logfont) );
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

    handle = CreateFontIndirect(&logfont);
    
    HDC	hDC = GetDC(NULL);
    HGDIOBJ	oldfont = SelectObject(hDC, handle);

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
		char_width = width;
	    char_height = met.tmHeight;

		zenkaku_table.clear();
	    for( int i=0 ; i<=0xffff ; i++ )
	    {
			zenkaku_table.push_back( char_width_table[i] > char_width );
	    }

	    free(char_width_table);
	}

    SelectObject(hDC, oldfont);
    ReleaseDC(NULL, hDC);
}

FontWin::~FontWin()
{
	FUNC_TRACE;

	DeleteObject(handle);
}

//-----------------------------------------------------------------------------

ImagePlaneWin::ImagePlaneWin( WindowBase * _window, int _x, int _y, int _width, int _height, float _priority )
	:
	ImagePlaneBase(_window,_x,_y,_width,_height,_priority)
{
}

ImagePlaneWin::~ImagePlaneWin()
{
}

void ImagePlaneWin::Draw( const Rect & paint_rect )
{
	WindowWin * window = (WindowWin*)this->window;
	ImageWin * image = (ImageWin*)this->image;

	if( !show )
	{
		return;
	}

    RECT plane_rect = { x, y, x+width, y+height };
   	if( plane_rect.left>=paint_rect.right || plane_rect.right<=paint_rect.left
   		|| plane_rect.top>=paint_rect.bottom || plane_rect.bottom<=paint_rect.top )
	{
		return;
	}

   	if( image )
   	{
		HDC compatibleDC = CreateCompatibleDC(window->offscreen_dc);

		SelectObject( compatibleDC, image->handle );

		if(0)
		{
			printf( "StretchBlt : %d %d %d %d  %d %d %d %d\n",
				x, y, width, height,
				0, 0, image->width, image->height );
		}

		if( image->halftone )	
		{
			SetStretchBltMode( window->offscreen_dc, STRETCH_HALFTONE );
		}
		else
		{
			SetStretchBltMode( window->offscreen_dc, STRETCH_DELETESCANS );
		}

		if( image->transparent )
		{
			if(0)
			{
				BLENDFUNCTION bf;
				bf.BlendOp = AC_SRC_OVER;
				bf.BlendFlags = 0;
				bf.AlphaFormat = AC_SRC_ALPHA;
				bf.SourceConstantAlpha = 255;
				
				AlphaBlend(
					window->offscreen_dc, x, y, width, height,
					compatibleDC, 0, 0, image->width, image->height,
					bf
					);
			}

			TransparentBlt(
				window->offscreen_dc, x, y, width, height,
				compatibleDC, 0, 0, image->width, image->height,
				image->transparent_color
				);

			window->perf_fillrect_count++;
		}
		else
		{
			StretchBlt(
				window->offscreen_dc, x, y, width, height,
				compatibleDC, 0, 0, image->width, image->height,
				SRCCOPY
				);

			window->perf_fillrect_count++;
		}

	    DeleteDC(compatibleDC);
   	}
}

//-----------------------------------------------------------------------------

TextPlaneWin::TextPlaneWin( WindowBase * _window, int _x, int _y, int _width, int _height, float _priority )
	:
	TextPlaneBase(_window,_x,_y,_width,_height,_priority),
	offscreen_dc(0),
	offscreen_bmp(0),
    offscreen_buf(0)
{
	FUNC_TRACE;

	offscreen_size.cx = 0;
	offscreen_size.cy = 0;
}

TextPlaneWin::~TextPlaneWin()
{
	FUNC_TRACE;

	if(offscreen_bmp) { DeleteObject(offscreen_bmp); }
	if(offscreen_dc) { DeleteObject(offscreen_dc); }
}

void TextPlaneWin::Scroll( int x, int y, int width, int height, int delta_x, int delta_y )
{
	FUNC_TRACE;

	BOOL ret;

	// テキスト用オフスクリーンにまだ描いてないものがあれば描く
    DrawOffscreen();

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
		x * font->char_width, 
		y * font->char_height, 
		(x+width) * font->char_width, 
		(y+height) * font->char_height };

	ret = ScrollDC( 
		offscreen_dc, 
		delta_x * font->char_width,
		delta_y * font->char_height,
		&src_rect,
		NULL, NULL, NULL );
	assert(ret);

	RECT dirty_rect = { 
		(x+delta_x) * font->char_width + this->x, 
		(y+delta_y) * font->char_height + this->y, 
		(x+width+delta_x) * font->char_width + this->x, 
		(y+height+delta_y) * font->char_height + this->y };
	
	window->appendDirtyRect( dirty_rect );
}

void TextPlaneWin::DrawOffscreen()
{
	FUNC_TRACE;

	WindowWin * window = (WindowWin*)this->window;
	FontWin * font = (FontWin*)this->font;

	if(!dirty){ return; }

	bool offscreen_rebuilt = false;

	// オフスクリーンバッファ生成
	if( offscreen_dc==NULL || offscreen_bmp==NULL || offscreen_size.cx!=width || offscreen_size.cy!=height )
	{
		if(offscreen_dc){ DeleteObject(offscreen_dc); }
		if(offscreen_bmp){ DeleteObject(offscreen_bmp); }

		offscreen_size.cx = width;
		offscreen_size.cy = height;

		if(offscreen_size.cx<1){ offscreen_size.cx=1; }
		if(offscreen_size.cy<1){ offscreen_size.cy=1; }

		offscreen_dc = CreateCompatibleDC(window->offscreen_dc);
		BITMAPINFO bmi;
		ZeroMemory(&bmi, sizeof(BITMAPINFO));
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = offscreen_size.cx;
		bmi.bmiHeader.biHeight = offscreen_size.cy;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		bmi.bmiHeader.biSizeImage = offscreen_size.cx * offscreen_size.cy * 4;
		offscreen_bmp = CreateDIBSection( offscreen_dc, &bmi, DIB_RGB_COLORS, (void**)&offscreen_buf, NULL, 0 );
		SelectObject(offscreen_dc, offscreen_bmp);

		offscreen_rebuilt = true;
	}

	int text_width = width / font->char_width;
	int text_height = height / font->char_height;

    wchar_t * work_text = new wchar_t[ text_width ];
    int * work_width = new int [ text_width ];
    unsigned int work_len;
	bool work_dirty;

	SelectObject(offscreen_dc, font->handle);

    for( int y=0 ; y<(int)char_buffer.size() && y<text_height ; ++y )
    {
        Line & line = *(char_buffer[y]);

        for( int x=0 ; x<(int)line.size() && x<text_width ; ++x )
        {
            Char & chr = line[x];

            work_len = 0;
			work_dirty = false;

            int x2;
            for( x2=x ; x2<(int)line.size() && x2<text_width ; ++x2 )
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
                work_width[work_len] = (!font->zenkaku_table[chr2.c]) ? font->char_width : font->char_width*2;
                work_len ++;

				if(chr2.dirty)
				{
					work_dirty = true;
					chr2.dirty = false;
				}
            }

			if(work_dirty || offscreen_rebuilt)
			{
				if( chr.attr.bg & Attribute::BG_Flat )
				{
					HBRUSH hBrush = CreateSolidBrush( chr.attr.bg_color[0] );
					HPEN hPen = CreatePen( PS_SOLID, 0, chr.attr.bg_color[0] );

            		SelectObject( offscreen_dc, hBrush );
            		SelectObject( offscreen_dc, hPen );

					Rectangle(
						offscreen_dc,
						x * font->char_width,
						y * font->char_height,
						x2 * font->char_width,
                		(y+1) * font->char_height
						);

					DeleteObject(hBrush);
					DeleteObject(hPen);
					
					window->perf_fillrect_count ++;
				}
				else if( chr.attr.bg & Attribute::BG_Gradation )
				{
					TRIVERTEX v[4];

					v[0].x = x * font->char_width;
					v[0].y = y * font->char_height;
					v[0].Red = GetRValue(chr.attr.bg_color[0])<<8;
					v[0].Green = GetGValue(chr.attr.bg_color[0])<<8;
					v[0].Blue = GetBValue(chr.attr.bg_color[0])<<8;
					v[0].Alpha = 255<<8;

					v[1].x = x2 * font->char_width;
					v[1].y = y * font->char_height;
					v[1].Red = GetRValue(chr.attr.bg_color[1])<<8;
					v[1].Green = GetGValue(chr.attr.bg_color[1])<<8;
					v[1].Blue = GetBValue(chr.attr.bg_color[1])<<8;
					v[1].Alpha = 255<<8;

					v[2].x = x * font->char_width;
					v[2].y = (y+1) * font->char_height;
					v[2].Red = GetRValue(chr.attr.bg_color[2])<<8;
					v[2].Green = GetGValue(chr.attr.bg_color[2])<<8;
					v[2].Blue = GetBValue(chr.attr.bg_color[2])<<8;
					v[2].Alpha = 255<<8;

					v[3].x = x2 * font->char_width;
					v[3].y = (y+1) * font->char_height;
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
						offscreen_dc,
						v,
						4,
						triangle,
						2,
						GRADIENT_FILL_TRIANGLE
					);

					window->perf_fillrect_count ++;
				}
				else
				{
					HBRUSH brush = (HBRUSH)GetStockObject( BLACK_BRUSH );

					RECT rect = {
						x * font->char_width,
						y * font->char_height,
						x2 * font->char_width,
                		(y+1) * font->char_height
					};

					FillRect( offscreen_dc, &rect, brush );

					window->perf_fillrect_count ++;
				}

				if(chr.attr.bg)
				{
					SetTextColor( offscreen_dc, chr.attr.fg_color );
				}
				else
				{
					SetTextColor( offscreen_dc, RGB(0xff, 0xff, 0xff) );
				}

	            SetBkMode( offscreen_dc, TRANSPARENT );

	            ExtTextOut(
	                offscreen_dc,
	                x * font->char_width,
	                y * font->char_height,
	                0, NULL,
	                (LPCWSTR)work_text,
	                (UINT)(work_len),
	                work_width );

				window->perf_drawtext_count ++;

				if(chr.attr.bg)
				{
					RECT rect = {
						x * font->char_width,
						y * font->char_height,
						x2 * font->char_width,
                		(y+1) * font->char_height
					};

					// Alphaを 255 で埋める
					for( int py=rect.top ; py<rect.bottom ; ++py )
					{
						for( int px=rect.left ; px<rect.right ; ++px )
						{
							offscreen_buf[ (offscreen_size.cy-py-1) * offscreen_size.cx * 4 + px * 4 + 3 ] = 0xff;
						}
					}
				}
				else
				{
					RECT rect = {
						x * font->char_width,
						y * font->char_height,
						x2 * font->char_width,
                		(y+1) * font->char_height
					};

					// Premultiplied Alpha 処理
					for( int py=rect.top ; py<rect.bottom ; ++py )
					{
						for( int px=rect.left ; px<rect.right ; ++px )
						{
							int alpha = offscreen_buf[ (offscreen_size.cy-py-1) * offscreen_size.cx * 4 + px * 4 + 1 ];
							offscreen_buf[ (offscreen_size.cy-py-1) * offscreen_size.cx * 4 + px * 4 + 0 ] = GetBValue(chr.attr.fg_color) * alpha / 255;
							offscreen_buf[ (offscreen_size.cy-py-1) * offscreen_size.cx * 4 + px * 4 + 1 ] = GetGValue(chr.attr.fg_color) * alpha / 255;
							offscreen_buf[ (offscreen_size.cy-py-1) * offscreen_size.cx * 4 + px * 4 + 2 ] = GetRValue(chr.attr.fg_color) * alpha / 255;
							offscreen_buf[ (offscreen_size.cy-py-1) * offscreen_size.cx * 4 + px * 4 + 3 ] = alpha;
						}
					}
				}

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

		            	SelectObject( offscreen_dc, hPen );

						if(chr.attr.line[line] & Attribute::Line_Left)
						{
							DrawVerticalLine( 
								x * font->char_width, 
								y * font->char_height, 
								(y+1) * font->char_height, 
								chr.attr.line_color[line], 
								(chr.attr.line[line] & Attribute::Line_Dot)!=0 );
						}

						if(chr.attr.line[line] & Attribute::Line_Bottom)
						{
							DrawHorizontalLine( 
								x * font->char_width, 
								(y+1) * font->char_height - 1, 
								x2 * font->char_width, 
								chr.attr.line_color[line], 
								(chr.attr.line[line] & Attribute::Line_Dot)!=0 );
						}

						if(chr.attr.line[line] & Attribute::Line_Right)
						{
							DrawVerticalLine( 
								x2 * font->char_width - 1, 
			                	y * font->char_height,
								(y+1) * font->char_height, 
								chr.attr.line_color[line], 
								(chr.attr.line[line] & Attribute::Line_Dot)!=0 );
						}

						if(chr.attr.line[line] & Attribute::Line_Top)
						{
							DrawHorizontalLine( 
								x * font->char_width, 
			                	y * font->char_height,
				            	x2 * font->char_width,
								chr.attr.line_color[line], 
								(chr.attr.line[line] & Attribute::Line_Dot)!=0 );
						}

			            DeleteObject(hPen);
		            }
				}
			}
			
            x = x2 - 1;
        }
    }

    delete [] work_width;
    delete [] work_text;

	dirty = false;
}

void TextPlaneWin::DrawHorizontalLine( int x1, int y1, int x2, COLORREF color, bool dotted )
{
	color = (0xff << 24) | (GetRValue(color) << 16) | (GetGValue(color) << 8) | GetBValue(color);
	int step;
	if(dotted) { step=2; } else { step=1; }

	for( int x=x1 ; x<x2 ; x+=step )
	{
		*(int*)(&offscreen_buf[ (offscreen_size.cy-y1-1) * offscreen_size.cx * 4 + x * 4 + 0 ]) = color;
	}
}

void TextPlaneWin::DrawVerticalLine( int x1, int y1, int y2, COLORREF color, bool dotted )
{
	color = (0xff << 24) | (GetRValue(color) << 16) | (GetGValue(color) << 8) | GetBValue(color);
	int step;
	if(dotted) { step=2; } else { step=1; }

	for( int y=y1 ; y<y2 ; y+=step )
	{
		*(int*)(&offscreen_buf[ (offscreen_size.cy-y-1) * offscreen_size.cx * 4 + x1 * 4 + 0 ]) = color;
	}
}

void TextPlaneWin::Draw( const RECT & paint_rect )
{
	FUNC_TRACE;

	WindowWin * window = (WindowWin*)this->window;

	if( !show )
	{
		return;
	}

    RECT plane_rect = { x, y, x+width, y+height };
   	if( plane_rect.left>=paint_rect.right || plane_rect.right<=paint_rect.left
   		|| plane_rect.top>=paint_rect.bottom || plane_rect.bottom<=paint_rect.top )
	{
		return;
	}

	// テキスト用オフスクリーンバッファの描画
	DrawOffscreen();

	// テキスト用オフスクリーンバッファからの AlphaBlend
	{
		RECT _paint_rect = paint_rect;
		if( _paint_rect.left < plane_rect.left ){ _paint_rect.left = plane_rect.left; }
		if( _paint_rect.right > plane_rect.right ){ _paint_rect.right = plane_rect.right; }
		if( _paint_rect.top < plane_rect.top ){ _paint_rect.top = plane_rect.top; }
		if( _paint_rect.bottom > plane_rect.bottom ){ _paint_rect.bottom = plane_rect.bottom; }

		int paint_width = _paint_rect.right - _paint_rect.left;
		int paint_height = _paint_rect.bottom - _paint_rect.top;

		BLENDFUNCTION bf;
		bf.BlendOp = AC_SRC_OVER;
		bf.BlendFlags = 0;
		bf.AlphaFormat = AC_SRC_ALPHA;
		bf.SourceConstantAlpha = 255;
	
		AlphaBlend(
			window->offscreen_dc, _paint_rect.left, _paint_rect.top, paint_width, paint_height,
			offscreen_dc, _paint_rect.left-x, _paint_rect.top-y, paint_width, paint_height,
			bf
			);
	}
}

//-----------------------------------------------------------------------------

WindowWin::WindowWin( Param & param )
	:
	WindowBase(param),
	hwnd(NULL),
	ime_context(NULL),
	style(0),
	exstyle(0),
	offscreen_dc(NULL),
	offscreen_bmp(NULL),
	bg_brush(NULL),
	frame_pen(NULL),
	caret0_brush(NULL),
	caret1_brush(NULL)
{
	FUNC_TRACE;

    memset( &window_frame_size, 0, sizeof(window_frame_size) );
    memset( &last_valid_window_rect, 0, sizeof(last_valid_window_rect) );
    memset( &offscreen_size, 0, sizeof(offscreen_size) );

	bg_brush = CreateSolidBrush(param.bg_color);
    frame_pen = CreatePen( PS_SOLID, 0, param.frame_color );
    caret0_brush = CreateSolidBrush(param.caret0_color);
    caret1_brush = CreateSolidBrush(param.caret1_color);

    if(! _createWindow(param))
    {
        printf("_createWindow failed\n");
        return;
    }
}

WindowWin::~WindowWin()
{
	FUNC_TRACE;

	// ウインドウ破棄中に activate_handler などを呼ばないように
	clearCallables();

    if(bg_brush) { DeleteObject(bg_brush); bg_brush = NULL; }
    if(frame_pen) { DeleteObject(frame_pen); frame_pen = NULL; }
    if(caret0_brush) { DeleteObject(caret0_brush); caret0_brush = NULL; }
    if(caret1_brush) { DeleteObject(caret1_brush); caret1_brush = NULL; }
	if(offscreen_bmp) { DeleteObject(offscreen_bmp); offscreen_bmp = NULL; };
	if(offscreen_dc) { DeleteObject(offscreen_dc); offscreen_dc = NULL; };

	DestroyWindow(hwnd);
}

WindowHandle WindowWin::getHandle() const
{
	return hwnd;
}

void WindowWin::_drawBackground( const Rect & paint_rect )
{
	FillRect( offscreen_dc, &paint_rect, bg_brush );
}

void WindowWin::_drawPlanes( const Rect & paint_rect )
{
	FUNC_TRACE;

	std::list<PlaneBase*>::const_iterator i;
    for( i=plane_list.begin() ; i!=plane_list.end() ; i++ )
    {
    	PlaneBase * plane = *i;
		plane->Draw( paint_rect );
    }
}

void WindowWin::_drawCaret( const Rect & paint_rect )
{
    if( caret && caret_blink )
    {
		RECT rect;
		IntersectRect( &rect, &paint_rect, &caret_rect );

		if(rect.right>0 || rect.bottom>0)
		{
			if(ime_on)
			{
				SelectObject( offscreen_dc, caret1_brush ) ;
			}
			else
			{
	            SelectObject( offscreen_dc, caret0_brush ) ;
			}

			PatBlt( offscreen_dc, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, PATINVERT );
		}
    }
}

void WindowWin::flushPaint()
{
	FUNC_TRACE;

    if(!dirty){ return; }

	HDC hDC = NULL;
	bool bitblt = true;

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

	// オフスクリーンバッファ生成
	if( offscreen_dc==NULL || offscreen_bmp==NULL || offscreen_size.cx!=client_rect.right-client_rect.left || offscreen_size.cy!=client_rect.bottom-client_rect.top )
	{
		if(offscreen_dc){ DeleteObject(offscreen_dc); }
		if(offscreen_bmp){ DeleteObject(offscreen_bmp); }
		
		int width = client_rect.right-client_rect.left;
		int height = client_rect.bottom-client_rect.top;
		if(width<1){ width=1; }
		if(height<1){ height=1; }

		// オフスクリーン
		offscreen_dc = CreateCompatibleDC(hDC);
		offscreen_bmp = CreateCompatibleBitmap(hDC, width, height);
		SelectObject(offscreen_dc, offscreen_bmp);

		offscreen_size.cx = client_rect.right-client_rect.left;
		offscreen_size.cy = client_rect.bottom-client_rect.top;
		
		// オフスクリーンを作った直後は全て描く
		dirty_rect = client_rect;
		FillRect( offscreen_dc, &dirty_rect, bg_brush );
	}

	// オフスクリーンへの描画
	{
	    IntersectClipRect( offscreen_dc, dirty_rect.left, dirty_rect.top, dirty_rect.right, dirty_rect.bottom );

		_drawBackground( dirty_rect );
		_drawPlanes( dirty_rect );
		_drawCaret( dirty_rect );

		SelectClipRgn( offscreen_dc, NULL );
	}

	// オフスクリーンからウインドウに転送
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
}

void WindowWin::_onNcPaint( HDC hDC )
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

void WindowWin::_onSizing( DWORD edge, LPRECT rect )
{
	FUNC_TRACE;

	int width = (rect->right - rect->left) - window_frame_size.cx;
    int height = (rect->bottom - rect->top) - window_frame_size.cy;

	if(sizing_handler)
	{
		PyObject * pysize = Py_BuildValue("[ii]", width, height );

		PyObject * pyarglist = Py_BuildValue("(O)", pysize );
		PyObject * pyresult = PyEval_CallObject( sizing_handler, pyarglist );
		Py_DECREF(pyarglist);
		if(pyresult)
		{
			Py_DECREF(pyresult);

			PyArg_Parse( pysize, "(ii)", &width, &height );
		}
		else
		{
			PyErr_Print();
		}

		Py_DECREF(pysize);
	}

    width += window_frame_size.cx;
    height += window_frame_size.cy;

	if(edge==WMSZ_LEFT || edge==WMSZ_TOPLEFT || edge==WMSZ_BOTTOMLEFT)
    {
        rect->left = rect->right - width;
    }
    else if(edge==WMSZ_RIGHT || edge==WMSZ_TOPRIGHT || edge==WMSZ_BOTTOMRIGHT)
    {
        rect->right = rect->left + width;
    }

    if(edge==WMSZ_TOP || edge==WMSZ_TOPLEFT || edge==WMSZ_TOPRIGHT)
    {
        rect->top = rect->bottom - height;
    }
    else if(edge==WMSZ_BOTTOM || edge==WMSZ_BOTTOMLEFT || edge==WMSZ_BOTTOMRIGHT)
    {
        rect->bottom = rect->top + height;
    }
}

void WindowWin::_onWindowPositionChange( WINDOWPOS * wndpos, bool send_event )
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

	        if( send_event && size_handler )
			{
				RECT client_rect;
				GetClientRect( hwnd, &client_rect );

				PyObject * pyarglist = Py_BuildValue("(ii)", client_rect.right  - client_rect.left, client_rect.bottom - client_rect.top );
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

void WindowWin::_updateWindowFrameRect()
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

	window_frame_size.cx = (window_rect.right-window_rect.left) - (client_rect.right-client_rect.left);
	window_frame_size.cy = (window_rect.bottom-window_rect.top) - (client_rect.bottom-client_rect.top);
}

void WindowWin::_setImePosition()
{
	FUNC_TRACE;

    HIMC imc = ImmGetContext(hwnd);

    COMPOSITIONFORM cf;
    
    if( ime_rect.left==0 && ime_rect.right==0 )
    {
    	cf.dwStyle = CFS_POINT;
	    cf.ptCurrentPos.x = caret_rect.left;
	    cf.ptCurrentPos.y = caret_rect.top;
    }
    else
    {
	    cf.dwStyle = CFS_RECT;
	    cf.ptCurrentPos.x = caret_rect.left;
	    cf.ptCurrentPos.y = caret_rect.top;
	    cf.rcArea = ime_rect;
    }
    
    ImmSetCompositionWindow(imc, &cf);

    ImmReleaseContext(hwnd, imc);
}

void WindowWin::_onTimerCaretBlink()
{
	FUNC_TRACE;

	if(caret_blink>0)
	{
		--caret_blink;
	}
	else
	{
		caret_blink = 1;
	}

	if( caret_rect.right>0 && caret_rect.bottom>0 )
	{
		appendDirtyRect( caret_rect );
	}
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

LRESULT CALLBACK WindowWin::_wndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
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

            SetTimer(hwnd, TIMER_PAINT, TIMER_PAINT_INTERVAL, NULL);
            SetTimer(hwnd, TIMER_CARET_BLINK, GetCaretBlinkTime(), NULL);
        }
        break;

    case WM_DESTROY:
        {
            KillTimer( hwnd, TIMER_PAINT );
            KillTimer( hwnd, TIMER_CARET_BLINK );

            RemoveProp( hwnd, L"ckit_userdata" );
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
			else if( ID_POPUP_MENUITEM<=wmId && wmId<ID_POPUP_MENUITEM+(int)window->popup_menu_commands.size() )
			{
				PyObject * func = window->popup_menu_commands[wmId-ID_POPUP_MENUITEM];
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
	    	else if(wp==TIMER_CARET_BLINK)
			{
		        window->_onTimerCaretBlink();
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
        break;

    case WM_SIZING:
		window->_onSizing((DWORD)wp, (LPRECT)lp);
        break;

    case WM_WINDOWPOSCHANGING:
        window->_onWindowPositionChange( (WINDOWPOS*)lp, false );
        break;

    case WM_WINDOWPOSCHANGED:
        window->_onWindowPositionChange( (WINDOWPOS*)lp, true );
        break;

    case WM_LBUTTONDOWN:
		if(window->lbuttondown_handler)
		{
			int mod = getModKey();
			
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
			int mod = getModKey();

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
			int mod = getModKey();

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
			int mod = getModKey();

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
			int mod = getModKey();

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
			int mod = getModKey();

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
			int mod = getModKey();

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
			int mod = getModKey();

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
			int mod = getModKey();

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
			int mod = getModKey();

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
			int mod = getModKey();

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
        if(wp == IMN_SETOPENSTATUS)
		{
            HIMC imc = ImmGetContext(hwnd);
            window->ime_on = ImmGetOpenStatus(imc) != FALSE;
            ImmReleaseContext(hwnd, imc);
            
			// FIXME : 再描画の位置をCaretの位置だけにしたい
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
			int mod = getModKey();

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
			int mod = getModKey();

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
			int mod = getModKey();

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

bool WindowWin::_registerWindowClass()
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

bool WindowWin::_createWindow( Param & param )
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
	
	// FIXME : _updateWindowFrameRect を使うべき？
	RECT window_frame_rect;
	memset( &window_frame_rect, 0, sizeof(window_frame_rect) );
    AdjustWindowRectEx( &window_frame_rect, style, (menu!=NULL), exstyle );
	window_frame_size.cx = window_frame_rect.right - window_frame_rect.left;
	window_frame_size.cy = window_frame_rect.bottom - window_frame_rect.top;

	w = param.winsize_w + window_frame_size.cx;
    h = param.winsize_h + window_frame_size.cy;

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

void WindowWin::enumFonts( std::vector<std::wstring> * font_list )
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

void WindowWin::setBGColor( COLORREF color )
{
    if(bg_brush){ DeleteObject(bg_brush); }
    bg_brush = CreateSolidBrush(color);

	RECT dirty_rect;
    GetClientRect( hwnd, &dirty_rect );
	appendDirtyRect( dirty_rect );

	RedrawWindow( hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE );
}

void WindowWin::setFrameColor( COLORREF color )
{
    if(frame_pen){ DeleteObject(frame_pen); }
    frame_pen = CreatePen( PS_SOLID, 0, color );

	RedrawWindow( hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE );
}

void WindowWin::setCaretColor( COLORREF color0, COLORREF color1 )
{
	FUNC_TRACE;

    if(caret0_brush){ DeleteObject(caret0_brush); }
    if(caret1_brush){ DeleteObject(caret1_brush); }

    caret0_brush = CreateSolidBrush(color0);
    caret1_brush = CreateSolidBrush(color1);

	appendDirtyRect( caret_rect );
}

void WindowWin::_buildMenu( HMENU menu_handle, PyObject * pysequence, int depth, bool parent_enabled )
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

void WindowWin::setMenu( PyObject * _menu )
{
	if( menu == _menu ) return;
	
	Py_XDECREF(menu);
	menu = _menu;
	Py_XINCREF(menu);
	
	HMENU old_menu_handle = GetMenu(hwnd);

	clearMenuCommands();

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

void WindowWin::_refreshMenu()
{
	clearMenuCommands();

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

void WindowWin::setPositionAndSize( int x, int y, int width, int height, int origin )
{
	FUNC_TRACE;
	
    int client_w = width;
    int client_h = height;
    int window_w = client_w + window_frame_size.cx;
    int window_h = client_h + window_frame_size.cy;

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

void WindowWin::setCapture()
{
	SetCapture(hwnd);
}

void WindowWin::releaseCapture()
{
    if(hwnd!=GetCapture())
    {
        return;
    }
    ReleaseCapture();
}

void WindowWin::setMouseCursor( int id )
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

void WindowWin::drag( int x, int y )
{
	PostMessage( hwnd, WM_NCLBUTTONDOWN, (WPARAM)HTCAPTION, ((y<<16)|x) );
}

void WindowWin::show( bool show, bool activate )
{
	FUNC_TRACE;

    ShowWindow( hwnd, show ? (activate?SW_SHOW:SW_SHOWNOACTIVATE) : SW_HIDE );
}

void WindowWin::enable( bool enable )
{
	FUNC_TRACE;

    EnableWindow( hwnd, enable );
}

void WindowWin::activate()
{
	FUNC_TRACE;

    SetActiveWindow(hwnd);
}

void WindowWin::inactivate()
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

void WindowWin::foreground()
{
	FUNC_TRACE;

    HWND hwnd_last_active = GetLastActivePopup( hwnd );
    SetForegroundWindow( hwnd_last_active );
}

void WindowWin::restore()
{
	FUNC_TRACE;

    ShowWindow( hwnd, SW_RESTORE );
}

void WindowWin::maximize()
{
	FUNC_TRACE;

    ShowWindow( hwnd, SW_MAXIMIZE );
}

void WindowWin::minimize()
{
	FUNC_TRACE;

    ShowWindow( hwnd, SW_MINIMIZE );
}

void WindowWin::topmost( bool topmost )
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

bool WindowWin::isEnabled()
{
	FUNC_TRACE;

	return ::IsWindowEnabled( hwnd )!=FALSE;
}

bool WindowWin::isVisible()
{
	FUNC_TRACE;

	return ::IsWindowVisible( hwnd )!=FALSE;
}

bool WindowWin::isMaximized()
{
	FUNC_TRACE;
	
	return ::IsZoomed( hwnd )!=FALSE;
}

bool WindowWin::isMinimized()
{
	FUNC_TRACE;

	return ::IsIconic( hwnd )!=FALSE;
}

bool WindowWin::isActive()
{
	FUNC_TRACE;
	
	return active;
}

bool WindowWin::isForeground()
{
	FUNC_TRACE;
	
	return hwnd==GetForegroundWindow();
}

void WindowWin::getWindowRect( Rect * rect )
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

void WindowWin::getClientSize( Size * size )
{
	FUNC_TRACE;
	
	RECT rect;
    GetClientRect( hwnd, &rect );

	size->cx = rect.right - rect.left;
	size->cy = rect.bottom - rect.top;
}

void WindowWin::getNormalWindowRect( RECT * rect )
{
	FUNC_TRACE;

	WINDOWPLACEMENT window_place;
	memset( &window_place, 0, sizeof(window_place) );
	window_place.length = sizeof(window_place);
	::GetWindowPlacement( hwnd, &window_place );

	*rect = window_place.rcNormalPosition;
}

void WindowWin::getNormalClientSize( SIZE * size )
{
	FUNC_TRACE;

	WINDOWPLACEMENT window_place;
	memset( &window_place, 0, sizeof(window_place) );
	window_place.length = sizeof(window_place);
	::GetWindowPlacement( hwnd, &window_place );

	size->cx = window_place.rcNormalPosition.right - window_place.rcNormalPosition.left - window_frame_size.cx;
	size->cy = window_place.rcNormalPosition.bottom - window_place.rcNormalPosition.top - window_frame_size.cy;
}

void WindowWin::clientToScreen(Point * point)
{
	FUNC_TRACE;

	ClientToScreen( hwnd, point );
}

void WindowWin::screenToClient(Point * point)
{
	FUNC_TRACE;

	ScreenToClient( hwnd, point );
}

void WindowWin::setTimer( PyObject * func, int interval )
{
	SetTimer( hwnd, (UINT_PTR)func, interval, NULL );
}

void WindowWin::killTimer( PyObject * func )
{
	KillTimer( hwnd, (UINT_PTR)(func) );
}

void WindowWin::setHotKey( int vk, int mod, PyObject * func )
{
	// 0x0000 - 0xBFFF の範囲で、使われていないIDを検索する
	int id;
	for( id=0 ; id<0xc000 ; ++id )
	{
		bool dup = false;
		for( std::list<HotKeyInfo>::const_iterator i=hotkey_list.begin(); i!=hotkey_list.end() ; ++i )
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
		hotkey_list.push_back( HotKeyInfo(func,id) );

		RegisterHotKey( hwnd, id, mod, vk );
	}
}

void WindowWin::killHotKey( PyObject * func )
{
	for( std::list<HotKeyInfo>::iterator i=hotkey_list.begin(); i!=hotkey_list.end() ; i++ )
	{
		if( i->pyobj==NULL ) continue;

		if( PyObject_RichCompareBool( func, (i->pyobj), Py_EQ )==1 )
		{
			UnregisterHotKey( hwnd, i->id );

			Py_XDECREF( i->pyobj );
			i->pyobj = NULL;

			// Note : リストからの削除は、ここではなく呼び出しの後、hotkey_list_ref_count が 0 であることを確認しながら行う

			break;
		}
	}
}

void WindowWin::setText( const wchar_t * text )
{
	SetWindowText( hwnd, text );
}

bool WindowWin::popupMenu( int x, int y, PyObject * items )
{
	HMENU menu;
	menu = CreatePopupMenu();
	
	clearPopupMenuCommands();
	
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
		        return false;
		    }
		    
		    std::wstring name;
		    if( !PythonUtil::PyStringToWideString( pyname, &name ) )
		    {
				Py_XDECREF(item);
				DestroyMenu(menu);
		    	return false;
		    }
		    
		    if(name==L"-")
		    {
				AppendMenu( menu, MF_SEPARATOR, 0, L"" );
		    }
		    else
		    {
			    if( ID_POPUP_MENUITEM + popup_menu_commands.size() > ID_POPUP_MENUITEM_MAX )
			    {
					Py_XDECREF(item);
					DestroyMenu(menu);
					PyErr_SetString( PyExc_ValueError, "too many menu items." );
			    	return false;
			    }
			    
			    if(pyoption)
			    {
			    	if(pyoption==Py_True || pyoption==Py_False)
			    	{
						AppendMenu( menu, MF_ENABLED|((pyoption==Py_True)?MF_CHECKED:MF_UNCHECKED), ID_POPUP_MENUITEM + popup_menu_commands.size(), name.c_str() );
			    	}
			    	else
			    	{
						Py_XDECREF(item);
						DestroyMenu(menu);
						PyErr_SetString( PyExc_TypeError, "invalid menu option type." );
				    	return false;
			    	}
			    }
			    else
			    {
					AppendMenu( menu, MF_ENABLED, ID_POPUP_MENUITEM + popup_menu_commands.size(), name.c_str() );
			    }

				popup_menu_commands.push_back(func);
				Py_INCREF(func);
		    }
		
			Py_XDECREF(item);
		}
	}

	{
		SetForegroundWindow(hwnd); //ウィンドウをフォアグラウンドに持ってきます。
		SetFocus(hwnd);	//これをしないと、メニューが消えなくなります。

		TrackPopupMenu( menu, TPM_TOPALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON, x, y, 0, hwnd, 0 );

		PostMessage(hwnd,WM_NULL,0,0); //これをしないと、２度目のメニューがすぐ消えちゃいます。
	}

	DestroyMenu(menu);

	return true;
}

void WindowWin::enableIme( bool enable )
{
	FUNC_TRACE;

	if(enable)
	{
		if(ime_context)
		{
			ImmSetOpenStatus( ime_context, FALSE );
            ImmSetCompositionFontW( ime_context, &ime_logfont );
			ImmAssociateContext( hwnd, ime_context );
			ime_context = NULL;
		}
	}
	else
	{
		if(!ime_context)
		{
			ime_context = ImmAssociateContext( hwnd, NULL );
		}
	}
}

void WindowWin::setImeFont( FontBase * _font )
{
	FUNC_TRACE;

	FontWin * font = (FontWin*)_font;

	ime_logfont = font->logfont;
}

