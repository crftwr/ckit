#include <wchar.h>
#include <vector>
#include <algorithm>

#include "pythonutil.h"
#include "ckitcore_mac.h"
#include "ckitcore_cocoa_export.h"

using namespace ckit;

//-----------------------------------------------------------------------------

#define MODULE_NAME "ckitcore"

//static std::wstring WINDOW_CLASS_NAME 			= L"CkitWindowClass";
//static std::wstring TASKTRAY_WINDOW_CLASS_NAME  = L"CkitTaskTrayWindowClass";

/*
const int WM_USER_NTFYICON  = WM_USER + 100;
const int ID_MENUITEM       = 256;
const int ID_MENUITEM_MAX   = 256+1024-1;
const int ID_POPUP_MENUITEM       = ID_MENUITEM_MAX+1;
const int ID_POPUP_MENUITEM_MAX   = ID_POPUP_MENUITEM+1024;

const int TIMER_PAINT		   			= 0x101;
const int TIMER_PAINT_INTERVAL 			= 10;
const int TIMER_CARET_BLINK   			= 0x102;
*/

//-----------------------------------------------------------------------------

#define PRINTF printf
//#define PRINTF(...)

#define TRACE printf("%s(%d) : %s\n",__FILE__,__LINE__,__FUNCTION__)
//#define TRACE

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

ImageMac::ImageMac( int _width, int _height, const char * pixels, const Color * _transparent_color, bool _halftone )
	:
	ImageBase(_width,_height,pixels,_transparent_color,_halftone)
{
	FUNC_TRACE;

    buffer = (unsigned char*)malloc(width * height * 4);
    memcpy(buffer, pixels, width * height * 4);
    
    CGDataProviderRef dataProviderRef = CGDataProviderCreateWithData(NULL, buffer, width*height*4, NULL);
    handle = CGImageCreate(width, height, 8, 32, width*4, CGColorSpaceCreateDeviceRGB(), kCGBitmapByteOrderDefault | kCGImageAlphaLast, dataProviderRef, NULL, 0, kCGRenderingIntentDefault);

    CFRelease(dataProviderRef);
    
    // FIXME : _transparent_color、_halftone を無視している
}

ImageMac::~ImageMac()
{
	FUNC_TRACE;

    CGImageRelease(handle);
    free(buffer);
}

//-----------------------------------------------------------------------------

FontMac::FontMac( const wchar_t * name, int height )
	:
	FontBase(),
	handle(0)
{
	FUNC_TRACE;

    size_t name_len = wcslen(name);

    CFStringRef name_str = CFStringCreateWithBytes(
        NULL,
        (const UInt8*)name,
        name_len * sizeof(wchar_t),
        kCFStringEncodingUTF32LE,
        false);
    
    handle = CTFontCreateWithName( name_str, (CGFloat)height, NULL );
    
    CFRelease(name_str);

    // FIXME : 真面目に計算する
    char_width = height / 2;
    char_height = height;

    zenkaku_table.clear();
    for( int i=0 ; i<=0xffff ; i++ )
    {
        zenkaku_table.push_back(false);
    }
}

FontMac::~FontMac()
{
	FUNC_TRACE;
    
    CFRelease(handle);
}

//-----------------------------------------------------------------------------

ImagePlaneMac::ImagePlaneMac( WindowBase * _window, int _x, int _y, int _width, int _height, float _priority )
	:
	ImagePlaneBase(_window,_x,_y,_width,_height,_priority)
{
}

ImagePlaneMac::~ImagePlaneMac()
{
}

void ImagePlaneMac::Draw( const Rect & paint_rect )
{
    /*
	WindowMac * window = (WindowMac*)this->window;
	ImageMac * image = (ImageMac*)this->image;
    */

	if( !show )
	{
		return;
	}

    Rect plane_rect = { x, y, x+width, y+height };
   	if( plane_rect.left>=paint_rect.right || plane_rect.right<=paint_rect.left
   		|| plane_rect.top>=paint_rect.bottom || plane_rect.bottom<=paint_rect.top )
	{
		return;
	}
    
    /*

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
     
    */
}

//-----------------------------------------------------------------------------

TextPlaneMac::TextPlaneMac( WindowBase * _window, int _x, int _y, int _width, int _height, float _priority )
	:
	TextPlaneBase(_window,_x,_y,_width,_height,_priority),
	offscreen_handle(0),
    offscreen_context(0)
{
	FUNC_TRACE;

	offscreen_size.cx = 0;
	offscreen_size.cy = 0;
}

TextPlaneMac::~TextPlaneMac()
{
	FUNC_TRACE;

	if(offscreen_handle) { CGLayerRelease(offscreen_handle); }
}

void TextPlaneMac::Scroll( int x, int y, int width, int height, int delta_x, int delta_y )
{
	FUNC_TRACE;

	//bool ret;

	// ÉeÉLÉXÉgópÉIÉtÉXÉNÉäÅ[ÉìÇ…Ç‹Çæï`Ç¢ÇƒÇ»Ç¢Ç‡ÇÃÇ™Ç†ÇÍÇŒï`Ç≠
    DrawOffscreen();

	// ñÑÇ‹Ç¡ÇƒÇ¢Ç»Ç©Ç¡ÇΩÇÁãÛï∂éöÇ≈êiÇﬂÇÈ
	while( (int)char_buffer.size() <= y+height+delta_y )
	{
        Line * line = new Line();
        char_buffer.push_back(line);
	}

	// ÉLÉÉÉâÉNÉ^ÉoÉbÉtÉ@ÇÉRÉsÅ[
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
		// ñÑÇ‹Ç¡ÇƒÇ¢Ç»Ç©Ç¡ÇΩÇÁãÛï∂éöÇ≈êiÇﬂÇÈ
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

    /*
	Rect src_rect = {
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
    */

	Rect dirty_rect = {
		(x+delta_x) * font->char_width + this->x, 
		(y+delta_y) * font->char_height + this->y, 
		(x+width+delta_x) * font->char_width + this->x, 
		(y+height+delta_y) * font->char_height + this->y };
	
	window->appendDirtyRect( dirty_rect );
}

void TextPlaneMac::DrawOffscreen()
{
	FUNC_TRACE;

	WindowMac * window = (WindowMac*)this->window;
	FontMac * font = (FontMac*)this->font;

	if(!dirty){ return; }

	bool offscreen_rebuilt = false;

	// ÉIÉtÉXÉNÉäÅ[ÉìÉoÉbÉtÉ@ê∂ê¨
	if( offscreen_handle==NULL || offscreen_context==NULL || offscreen_size.cx!=width || offscreen_size.cy!=height )
	{
		if(offscreen_context){ CGContextRelease(offscreen_context); }
		if(offscreen_handle){ CGLayerRelease(offscreen_handle); }

		offscreen_size.cx = width;
		offscreen_size.cy = height;

		if(offscreen_size.cx<1){ offscreen_size.cx=1; }
		if(offscreen_size.cy<1){ offscreen_size.cy=1; }

        CGContextRef gctx = window->getCGContext();
        
        offscreen_handle = CGLayerCreateWithContext( gctx, CGSizeMake(offscreen_size.cx,offscreen_size.cy), NULL );
        offscreen_context = CGLayerGetContext(offscreen_handle);
        
		offscreen_rebuilt = true;
	}

	int text_width = width / font->char_width;
	int text_height = height / font->char_height;

    wchar_t * work_text = new wchar_t[ text_width ];
    int * work_width = new int [ text_width ];
    unsigned int work_len;
	bool work_dirty;

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
                    /*
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
					*/
                    
					window->perf_fillrect_count ++;
				}
				else if( chr.attr.bg & Attribute::BG_Gradation )
				{
                    /*
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
                    */

					window->perf_fillrect_count ++;
				}
				else
				{
                    /*
					HBRUSH brush = (HBRUSH)GetStockObject( BLACK_BRUSH );

					RECT rect = {
						x * font->char_width,
						y * font->char_height,
						x2 * font->char_width,
                		(y+1) * font->char_height
					};

					FillRect( offscreen_dc, &rect, brush );
                    */

					window->perf_fillrect_count ++;
				}

				if(chr.attr.bg)
				{
                    /*
					SetTextColor( offscreen_dc, chr.attr.fg_color );
                    */
				}
				else
				{
                    /*
					SetTextColor( offscreen_dc, RGB(0xff, 0xff, 0xff) );
                    */
				}

                /*
	            SetBkMode( offscreen_dc, TRANSPARENT );

	            ExtTextOut(
	                offscreen_dc,
	                x * font->char_width,
	                y * font->char_height,
	                0, NULL,
	                (LPCWSTR)work_text,
	                (UINT)(work_len),
	                work_width );
                */

				window->perf_drawtext_count ++;

				if(chr.attr.bg)
				{
                    /*
					RECT rect = {
						x * font->char_width,
						y * font->char_height,
						x2 * font->char_width,
                		(y+1) * font->char_height
					};

					// AlphaÇ 255 Ç≈ñÑÇﬂÇÈ
					for( int py=rect.top ; py<rect.bottom ; ++py )
					{
						for( int px=rect.left ; px<rect.right ; ++px )
						{
							offscreen_buf[ (offscreen_size.cy-py-1) * offscreen_size.cx * 4 + px * 4 + 3 ] = 0xff;
						}
					}
                    */
				}
				else
				{
                    /*
					RECT rect = {
						x * font->char_width,
						y * font->char_height,
						x2 * font->char_width,
                		(y+1) * font->char_height
					};

					// Premultiplied Alpha èàóù
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
                    */
				}

				for( int line=0 ; line<2 ; ++line )
				{
		            if( chr.attr.line[line] & (Attribute::Line_Left|Attribute::Line_Right|Attribute::Line_Top|Attribute::Line_Bottom) )
		            {
                        /*
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
                        */
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

void TextPlaneMac::DrawHorizontalLine( int x1, int y1, int x2, Color color, bool dotted )
{
    /*
	color = (0xff << 24) | (GetRValue(color) << 16) | (GetGValue(color) << 8) | GetBValue(color);
	int step;
	if(dotted) { step=2; } else { step=1; }

	for( int x=x1 ; x<x2 ; x+=step )
	{
		*(int*)(&offscreen_buf[ (offscreen_size.cy-y1-1) * offscreen_size.cx * 4 + x * 4 + 0 ]) = color;
	}
    */
}

void TextPlaneMac::DrawVerticalLine( int x1, int y1, int y2, Color color, bool dotted )
{
    /*
	color = (0xff << 24) | (GetRValue(color) << 16) | (GetGValue(color) << 8) | GetBValue(color);
	int step;
	if(dotted) { step=2; } else { step=1; }

	for( int y=y1 ; y<y2 ; y+=step )
	{
		*(int*)(&offscreen_buf[ (offscreen_size.cy-y-1) * offscreen_size.cx * 4 + x1 * 4 + 0 ]) = color;
	}
    */
}

void TextPlaneMac::Draw( const Rect & paint_rect )
{
	FUNC_TRACE;

	//WindowMac * window = (WindowMac*)this->window;

	if( !show )
	{
		return;
	}

    Rect plane_rect = { x, y, x+width, y+height };
   	if( plane_rect.left>=paint_rect.right || plane_rect.right<=paint_rect.left
   		|| plane_rect.top>=paint_rect.bottom || plane_rect.bottom<=paint_rect.top )
	{
		return;
	}

	// ÉeÉLÉXÉgópÉIÉtÉXÉNÉäÅ[ÉìÉoÉbÉtÉ@ÇÃï`âÊ
	DrawOffscreen();

	// ÉeÉLÉXÉgópÉIÉtÉXÉNÉäÅ[ÉìÉoÉbÉtÉ@Ç©ÇÁÇÃ AlphaBlend
	{
		Rect _paint_rect = paint_rect;
		if( _paint_rect.left < plane_rect.left ){ _paint_rect.left = plane_rect.left; }
		if( _paint_rect.right > plane_rect.right ){ _paint_rect.right = plane_rect.right; }
		if( _paint_rect.top < plane_rect.top ){ _paint_rect.top = plane_rect.top; }
		if( _paint_rect.bottom > plane_rect.bottom ){ _paint_rect.bottom = plane_rect.bottom; }

		//int paint_width = _paint_rect.right - _paint_rect.left;
		//int paint_height = _paint_rect.bottom - _paint_rect.top;

        /*
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
        */
	}
}

//-----------------------------------------------------------------------------

void WindowMac::initializeSystem( const wchar_t * prefix )
{
    ckit_Application_Create();
}

void WindowMac::terminateSystem()
{
}

static int _drawRect( void * owner, CGRect rect, CGContextRef gctx )
{
    TRACE;
    
    // 単色矩形描画テスト
    {
        CGContextSetRGBFillColor (gctx, 1, 0, 0, 1);
        CGContextFillRect (gctx, CGRectMake (0, 0, 200, 100 ));
        CGContextSetRGBFillColor (gctx, 0, 0, 1, 0.5);
        CGContextFillRect (gctx, CGRectMake (0, 0, 100, 200));
    }
    
    // 画像描画テスト
    {
        int width = 100;
        int height = 100;
        
        unsigned char * buffer = (unsigned char*)malloc(width * height * 4);
        for(int y=0 ; y<height ; ++y)
        {
            for(int x=0 ; x<width ; ++x)
            {
                int i = width * y + x;
                buffer[i*4+0] = (x * 4) % 255;    // R
                buffer[i*4+1] = (y * 4) % 255;    // G
                buffer[i*4+2] = 255;              // B
                buffer[i*4+3] = 255;              // A
            }
        }
        
        CGDataProviderRef dataProviderRef = CGDataProviderCreateWithData(NULL, buffer, width*height*4, NULL);
        CGImageRef image = CGImageCreate(width, height, 8, 32, width*4, CGColorSpaceCreateDeviceRGB(), kCGBitmapByteOrderDefault | kCGImageAlphaLast, dataProviderRef, NULL, 0, kCGRenderingIntentDefault);
        
        CGContextDrawImage(gctx, CGRectMake(210,0,100,100), image );
        
        CGImageRelease(image);
        CFRelease(dataProviderRef);
        free(buffer);
    }
    
    // オフスクリーン描画テスト
    {
        CGLayerRef layer = CGLayerCreateWithContext(gctx, CGSizeMake(200,200), NULL );
        
        CGContextRef layer_ctx = CGLayerGetContext(layer);
        
        // オフスクリーンに描画
        {
            CGContextSetRGBFillColor( layer_ctx, 0, 1, 0, 0.3);
            CGContextFillRect( layer_ctx, CGRectMake (0, 0, 200, 100 ));
            CGContextSetRGBFillColor( layer_ctx, 0, 0, 1, 0.3);
            CGContextFillRect( layer_ctx, CGRectMake (0, 0, 100, 200));
        }
        
        // 実画面にリピート描画
        CGContextDrawLayerInRect( gctx, CGRectMake(0, 210, 50, 50), layer);
        CGContextDrawLayerInRect( gctx, CGRectMake(60, 210, 50, 50), layer);
        CGContextDrawLayerInRect( gctx, CGRectMake(120, 210, 50, 50), layer);
        
        CGLayerRelease(layer);
    }
    
    // テキスト描画テスト
    {
        CFMutableAttributedStringRef attrString = CFAttributedStringCreateMutable(kCFAllocatorDefault, 0 );
        
        // 文字列
        CFStringRef str = CFSTR("Hello World! こんにちは");
        CFAttributedStringReplaceString(attrString, CFRangeMake(0,0), str );
        CFRelease(str);
        
        // フォント
        CTFontRef font = CTFontCreateWithName(CFSTR("HiraKakuProN-W6"), 20.f, NULL);
        CFAttributedStringSetAttribute(attrString, CFRangeMake(0, CFStringGetLength(str)), kCTFontAttributeName, font);
        CFRelease(font);
        
        // 色
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGFloat components[] = {1,0,0,0.8};
        CGColorRef color = CGColorCreate(colorSpace, components );
        CFAttributedStringSetAttribute(attrString, CFRangeMake(0,5), kCTForegroundColorAttributeName, color);
        CGColorRelease(color);
        CGColorSpaceRelease(colorSpace);
        
        // １行描画
        CTLineRef line = CTLineCreateWithAttributedString(attrString);
        
        CGContextSetTextMatrix(gctx, CGAffineTransformIdentity);
        CGContextSetTextPosition(gctx, 110.0, 110.0);
        CTLineDraw(line,gctx);
        
        CFRelease(line);
        CFRelease(attrString);
    }
    
    // フォント列挙
    {
        CFStringRef keys[] = { };
        CFTypeRef values[] = { };
        CFDictionaryRef options =
        CFDictionaryCreate(kCFAllocatorDefault, (const void**)&keys,
                           (const void**)&values, sizeof(keys) / sizeof(keys[0]),
                           &kCFTypeDictionaryKeyCallBacks,
                           &kCFTypeDictionaryValueCallBacks);
        
        CTFontCollectionRef fontCollection = CTFontCollectionCreateFromAvailableFonts(options);
        
        CFArrayRef array = CTFontCollectionCreateMatchingFontDescriptors(fontCollection);
        
        int numFound = 0;
        
        for( int i=0 ; i<CFArrayGetCount(array) ; ++i )
        {
            CTFontDescriptorRef fontDesc = (CTFontDescriptorRef)CFArrayGetValueAtIndex(array,i);
            
            CFStringRef familyName = (CFStringRef)CTFontDescriptorCopyAttribute(fontDesc, kCTFontFamilyNameAttribute);
            CFStringRef displayName = (CFStringRef)CTFontDescriptorCopyAttribute(fontDesc, kCTFontDisplayNameAttribute);
            //CFNumberRef fixedAdvance = CTFontDescriptorCopyAttribute(fontDesc, kCTFontFixedAdvanceAttribute);
            
            //if( fixedAdvance )
            {
                //float fixedAdvanceValue;
                //CFNumberGetValue(fixedAdvance, kCFNumberFloat32Type, &fixedAdvanceValue );
                
                printf( "family  name: %s\n", CFStringGetCStringPtr(familyName, kCFStringEncodingUTF8 ) );
                printf( "display name: %s\n", CFStringGetCStringPtr(displayName, kCFStringEncodingUTF8 ) );
                //printf( "fixed advance: %f\n", fixedAdvanceValue );
                
                numFound += 1;
            }
            
            CFRelease(displayName);
            CFRelease(familyName);
            //if(fixedAdvance){ CFRelease(fixedAdvance); }
        }
        
        printf( "font count = %d\n", numFound );
        
        CFRelease(array);
        CFRelease(fontCollection);
        CFRelease(options);
    }

    return 0;
}

ckit_Window_Callbacks callbacks = {
    _drawRect
};

WindowMac::WindowMac( Param & param )
	:
	WindowBase(param),
    handle(0)
{
	FUNC_TRACE;
    
    // initialize graphics system
    memset( &window_frame_size, 0, sizeof(window_frame_size) );
    
    // create window
    if( ckit_Window_Create( &callbacks, this, &handle )!=0 )
    {
        printf("ckit_Window_Create failed\n");
        return;
    }
}

WindowMac::~WindowMac()
{
	FUNC_TRACE;

	// ÉEÉCÉìÉhÉEîjä¸íÜÇ… activate_handler Ç»Ç«ÇåƒÇŒÇ»Ç¢ÇÊÇ§Ç…
	clearCallables();

    // terminate graphics system
    
    // destroy window
    if( ckit_Window_Destroy(handle)!=0 )
    {
        printf("ckit_Window_Destroy failed\n");
        return;
    }
}

WindowHandle WindowMac::getHandle() const
{
    // FIXME : implement
	return NULL;
}

void WindowMac::_drawBackground( const Rect & paint_rect )
{
    /*
	FillRect( offscreen_dc, &paint_rect, bg_brush );
    */
}

void WindowMac::_drawPlanes( const Rect & paint_rect )
{
	FUNC_TRACE;

	std::list<PlaneBase*>::const_iterator i;
    for( i=plane_list.begin() ; i!=plane_list.end() ; i++ )
    {
    	PlaneBase * plane = *i;
		plane->Draw( paint_rect );
    }
}

void WindowMac::_drawCaret( const Rect & paint_rect )
{
    /*
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
    */
}

void WindowMac::flushPaint()
{
	FUNC_TRACE;

    if(!dirty){ return; }

    /*
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

	// ÉIÉtÉXÉNÉäÅ[ÉìÉoÉbÉtÉ@ê∂ê¨
	if( offscreen_dc==NULL || offscreen_bmp==NULL || offscreen_size.cx!=client_rect.right-client_rect.left || offscreen_size.cy!=client_rect.bottom-client_rect.top )
	{
		if(offscreen_dc){ DeleteObject(offscreen_dc); }
		if(offscreen_bmp){ DeleteObject(offscreen_bmp); }
		
		int width = client_rect.right-client_rect.left;
		int height = client_rect.bottom-client_rect.top;
		if(width<1){ width=1; }
		if(height<1){ height=1; }

		// ÉIÉtÉXÉNÉäÅ[Éì
		offscreen_dc = CreateCompatibleDC(hDC);
		offscreen_bmp = CreateCompatibleBitmap(hDC, width, height);
		SelectObject(offscreen_dc, offscreen_bmp);

		offscreen_size.cx = client_rect.right-client_rect.left;
		offscreen_size.cy = client_rect.bottom-client_rect.top;
		
		// ÉIÉtÉXÉNÉäÅ[ÉìÇçÏÇ¡ÇΩíºå„ÇÕëSÇƒï`Ç≠
		dirty_rect = client_rect;
		FillRect( offscreen_dc, &dirty_rect, bg_brush );
	}

	// ÉIÉtÉXÉNÉäÅ[ÉìÇ÷ÇÃï`âÊ
	{
	    IntersectClipRect( offscreen_dc, dirty_rect.left, dirty_rect.top, dirty_rect.right, dirty_rect.bottom );

		_drawBackground( dirty_rect );
		_drawPlanes( dirty_rect );
		_drawCaret( dirty_rect );

		SelectClipRgn( offscreen_dc, NULL );
	}

	// ÉIÉtÉXÉNÉäÅ[ÉìÇ©ÇÁÉEÉCÉìÉhÉEÇ…ì]ëó
	if(bitblt)
	{
	    BOOL ret = BitBlt( hDC, dirty_rect.left, dirty_rect.top, dirty_rect.right-dirty_rect.left, dirty_rect.bottom-dirty_rect.top, offscreen_dc, dirty_rect.left, dirty_rect.top, SRCCOPY );
		assert(ret);
	}

	if(dc_need_release)
	{
		ReleaseDC( hwnd, hDC );
	}
    */

	clearDirtyRect();

	if(ime_on)
    {
        //_setImePosition();
    }
}

void WindowMac::enumFonts( std::vector<std::wstring> * font_list )
{
    /*
	HDC hdc = GetDC(NULL);

	EnumFontFamiliesEx(
		hdc,
		NULL,
		(FONTENUMPROC)cbEnumFont,
		(LPARAM)font_list,
		0
	);

	ReleaseDC( NULL, hdc );
    */
}

void WindowMac::setBGColor( Color color )
{
    /*
    if(bg_brush){ DeleteObject(bg_brush); }
    bg_brush = CreateSolidBrush(color);

	RECT dirty_rect;
    GetClientRect( hwnd, &dirty_rect );
	appendDirtyRect( dirty_rect );

	RedrawWindow( hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE );
    */
}

void WindowMac::setFrameColor( Color color )
{
    /*
    if(frame_pen){ DeleteObject(frame_pen); }
    frame_pen = CreatePen( PS_SOLID, 0, color );

	RedrawWindow( hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE );
    */
}

void WindowMac::setCaretColor( Color color0, Color color1 )
{
	FUNC_TRACE;

    /*
    if(caret0_brush){ DeleteObject(caret0_brush); }
    if(caret1_brush){ DeleteObject(caret1_brush); }

    caret0_brush = CreateSolidBrush(color0);
    caret1_brush = CreateSolidBrush(color1);
    */

	appendDirtyRect( caret_rect );
}

void WindowMac::setMenu( PyObject * _menu )
{
	if( menu == _menu ) return;
	
	Py_XDECREF(menu);
	menu = _menu;
	Py_XINCREF(menu);
	
	clearMenuCommands();

    /*
	HMENU old_menu_handle = GetMenu(hwnd);

	// MenuNode Ç…ÇµÇΩÇ™Ç¡ÇƒÉÅÉjÉÖÅ[Ççƒç\ízÇ∑ÇÈ
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

	// å√Ç¢ÉÅÉjÉÖÅ[ÇçÌèúÇ∑ÇÈ
	if(old_menu_handle)
	{
		DestroyMenu(old_menu_handle);
	}
    */

	//_updateWindowFrameRect();
}

/*
void WindowMac::_refreshMenu()
{
	clearMenuCommands();

	// MenuNode Ç…ÇµÇΩÇ™Ç¡ÇƒÉÅÉjÉÖÅ[Ççƒç\ízÇ∑ÇÈ
	if(menu)
	{
		HMENU menu_handle = GetMenu(hwnd);
		
		// ÉÅÉjÉÖÅ[ÇÉNÉäÉAÇ∑ÇÈ
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
*/

void WindowMac::setPositionAndSize( int x, int y, int width, int height, int origin )
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
    
    /*
	::SetWindowPos( hwnd, NULL, x, y, window_w, window_h, SWP_NOZORDER | SWP_NOACTIVATE );
     */

	Rect dirty_rect = { 0, 0, client_w, client_h };
	appendDirtyRect( dirty_rect );
}

void WindowMac::setCapture()
{
    /*
	SetCapture(hwnd);
     */
}

void WindowMac::releaseCapture()
{
    /*
    if(hwnd!=GetCapture())
    {
        return;
    }
    ReleaseCapture();
     */
}

void WindowMac::setMouseCursor( int id )
{
    /*
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
     */
}

void WindowMac::drag( int x, int y )
{
    /*
	PostMessage( hwnd, WM_NCLBUTTONDOWN, (WPARAM)HTCAPTION, ((y<<16)|x) );
     */
}

void WindowMac::show( bool show, bool activate )
{
	FUNC_TRACE;

    /*
    ShowWindow( hwnd, show ? (activate?SW_SHOW:SW_SHOWNOACTIVATE) : SW_HIDE );
     */
}

void WindowMac::enable( bool enable )
{
	FUNC_TRACE;

    /*
    EnableWindow( hwnd, enable );
     */
}

void WindowMac::activate()
{
	FUNC_TRACE;

    /*
    SetActiveWindow(hwnd);
     */
}

void WindowMac::inactivate()
{
	FUNC_TRACE;

    /*
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
     */
}

void WindowMac::foreground()
{
	FUNC_TRACE;

    /*
    HWND hwnd_last_active = GetLastActivePopup( hwnd );
    SetForegroundWindow( hwnd_last_active );
     */
}

void WindowMac::restore()
{
	FUNC_TRACE;

    /*
    ShowWindow( hwnd, SW_RESTORE );
     */
}

void WindowMac::maximize()
{
	FUNC_TRACE;

    /*
    ShowWindow( hwnd, SW_MAXIMIZE );
     */
}

void WindowMac::minimize()
{
	FUNC_TRACE;

    /*
    ShowWindow( hwnd, SW_MINIMIZE );
     */
}

void WindowMac::topmost( bool topmost )
{
	FUNC_TRACE;

    /*
	if(topmost)
	{
	    SetWindowPos( hwnd, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	}
	else
	{
	    SetWindowPos( hwnd, HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	}
     */
}

bool WindowMac::isEnabled()
{
	FUNC_TRACE;

    /*
	return ::IsWindowEnabled( hwnd )!=FALSE;
     */
    return false;
}

bool WindowMac::isVisible()
{
	FUNC_TRACE;

    /*
	return ::IsWindowVisible( hwnd )!=FALSE;
     */
    return true;
}

bool WindowMac::isMaximized()
{
	FUNC_TRACE;
	
    /*
	return ::IsZoomed( hwnd )!=FALSE;
     */
    return false;
}

bool WindowMac::isMinimized()
{
	FUNC_TRACE;

    /*
	return ::IsIconic( hwnd )!=FALSE;
     */
    return false;
}

bool WindowMac::isActive()
{
	FUNC_TRACE;
	
    /*
	return active;
     */
    return true;
}

bool WindowMac::isForeground()
{
	FUNC_TRACE;

	/*
	return hwnd==GetForegroundWindow();
     */
    return true;
}

void WindowMac::getWindowRect( Rect * rect )
{
	FUNC_TRACE;
	
    /*
	if(::IsIconic(hwnd))
	{
		*rect = last_valid_window_rect;
	}
	else
	{
		::GetWindowRect(hwnd,rect);
	}
     */
}

void WindowMac::getClientSize( Size * size )
{
	FUNC_TRACE;
	
    /*
	Rect rect;
    GetClientRect( hwnd, &rect );

	size->cx = rect.right - rect.left;
	size->cy = rect.bottom - rect.top;
     */
}

void WindowMac::getNormalWindowRect( Rect * rect )
{
	FUNC_TRACE;

    /*
	WINDOWPLACEMENT window_place;
	memset( &window_place, 0, sizeof(window_place) );
	window_place.length = sizeof(window_place);
	::GetWindowPlacement( hwnd, &window_place );

	*rect = window_place.rcNormalPosition;
    */
}

void WindowMac::getNormalClientSize( Size * size )
{
	FUNC_TRACE;

    /*
	WINDOWPLACEMENT window_place;
	memset( &window_place, 0, sizeof(window_place) );
	window_place.length = sizeof(window_place);
	::GetWindowPlacement( hwnd, &window_place );

	size->cx = window_place.rcNormalPosition.right - window_place.rcNormalPosition.left - window_frame_size.cx;
	size->cy = window_place.rcNormalPosition.bottom - window_place.rcNormalPosition.top - window_frame_size.cy;
    */
}

void WindowMac::clientToScreen(Point * point)
{
	FUNC_TRACE;

    /*
	ClientToScreen( hwnd, point );
    */
}

void WindowMac::screenToClient(Point * point)
{
	FUNC_TRACE;

    /*
	ScreenToClient( hwnd, point );
    */
}

void WindowMac::setTimer( PyObject * func, int interval )
{
    /*
	SetTimer( hwnd, (UINT_PTR)func, interval, NULL );
    */
}

void WindowMac::killTimer( PyObject * func )
{
    /*
	KillTimer( hwnd, (UINT_PTR)(func) );
    */
}

void WindowMac::setHotKey( int vk, int mod, PyObject * func )
{
    /*
	// 0x0000 - 0xBFFF ÇÃîÕàÕÇ≈ÅAégÇÌÇÍÇƒÇ¢Ç»Ç¢IDÇåüçıÇ∑ÇÈ
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
    */
}

void WindowMac::killHotKey( PyObject * func )
{
    /*
	for( std::list<HotKeyInfo>::iterator i=hotkey_list.begin(); i!=hotkey_list.end() ; i++ )
	{
		if( i->pyobj==NULL ) continue;

		if( PyObject_RichCompareBool( func, (i->pyobj), Py_EQ )==1 )
		{
			UnregisterHotKey( hwnd, i->id );

			Py_XDECREF( i->pyobj );
			i->pyobj = NULL;

			// Note : ÉäÉXÉgÇ©ÇÁÇÃçÌèúÇÕÅAÇ±Ç±Ç≈ÇÕÇ»Ç≠åƒÇ—èoÇµÇÃå„ÅAhotkey_list_ref_count Ç™ 0 Ç≈Ç†ÇÈÇ±Ç∆ÇämîFÇµÇ»Ç™ÇÁçsÇ§

			break;
		}
	}
    */
}

void WindowMac::setText( const wchar_t * text )
{
    /*
	SetWindowText( hwnd, text );
    */
}

bool WindowMac::popupMenu( int x, int y, PyObject * items )
{
    /*
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
		SetForegroundWindow(hwnd); //ÉEÉBÉìÉhÉEÇÉtÉHÉAÉOÉâÉEÉìÉhÇ…éùÇ¡ÇƒÇ´Ç‹Ç∑ÅB
		SetFocus(hwnd);	//Ç±ÇÍÇÇµÇ»Ç¢Ç∆ÅAÉÅÉjÉÖÅ[Ç™è¡Ç¶Ç»Ç≠Ç»ÇËÇ‹Ç∑ÅB

		TrackPopupMenu( menu, TPM_TOPALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON, x, y, 0, hwnd, 0 );

		PostMessage(hwnd,WM_NULL,0,0); //Ç±ÇÍÇÇµÇ»Ç¢Ç∆ÅAÇQìxñ⁄ÇÃÉÅÉjÉÖÅ[Ç™Ç∑ÇÆè¡Ç¶ÇøÇ·Ç¢Ç‹Ç∑ÅB
	}

	DestroyMenu(menu);
    */

	return true;
}

void WindowMac::enableIme( bool enable )
{
	FUNC_TRACE;

    /*
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
    */
}

void WindowMac::setImeFont( FontBase * _font )
{
	FUNC_TRACE;

    /*
	FontMac * font = (FontMac*)_font;
	ime_logfont = font->logfont;
    */
}

CGContextRef WindowMac::getCGContext()
{
    // FIXME : implement
    return NULL;
}

void WindowMac::messageLoop()
{
    ckit_Window_MessageLoop(handle);
}

