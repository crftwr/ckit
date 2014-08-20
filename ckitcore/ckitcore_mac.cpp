#include <wchar.h>
#include <vector>
#include <algorithm>

#include "pythonutil.h"
#include "ckitcore_mac.h"
#include "ckitcore_cocoa_export.h"

using namespace ckit;

//-----------------------------------------------------------------------------

#define MODULE_NAME "ckitcore"

const double TIMER_PAINT_INTERVAL = 0.01666;
const double TIMER_CHECK_QUIT_INTERVAL = 0.01666;

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

#define WARN_NOT_IMPLEMENTED printf("Warning: %s is not implemented.\n", __FUNCTION__)

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
	handle(0),
    ascent(0.0),
    descent(0.0)
{
	FUNC_TRACE;

    size_t name_len = wcslen(name);

    CFStringRef name_str = CFStringCreateWithBytes(
        NULL,
        (const UInt8*)name,
        name_len * sizeof(wchar_t),
        kCFStringEncodingUTF32LE,
        false);

    // heightはピクセル単位なので、ちょうどいいポイントを探す
    {
        CTFontRef _handle;
        
        // まずはピクセル＝ポイントと見なして作ってみる
        _handle = CTFontCreateWithName( name_str, (CGFloat)height, NULL );
        ascent = CTFontGetAscent(_handle);
        descent = CTFontGetDescent(_handle);
        CFRelease(_handle);
        
        PRINTF("font size adjust : step1 : %f,%f,%d\n", ascent, descent, height );

        // 比率で当たりを付けて作ってみる
        int size_in_point = int(height * height / (ascent + descent));
        _handle = CTFontCreateWithName( name_str, size_in_point, NULL );
        ascent = CTFontGetAscent(_handle);
        descent = CTFontGetDescent(_handle);

        PRINTF("font size adjust : step2 : %f,%f,%d\n", ascent, descent, size_in_point );
        
        int retry_count = 0;

        // 微調整する
        while(true)
        {
            UniChar unichars[1];
            unichars[0] = 'W';
            CGGlyph glyphs[1];
            CTFontGetGlyphsForCharacters( _handle, unichars, glyphs, 1 );
            CGSize advances[1];
            double advance = CTFontGetAdvancesForGlyphs( _handle, kCTFontDefaultOrientation, glyphs, advances, 1 );
            
            if( ascent + descent <= height && fabs(advance-int(advance))<0.0001f )
            {
                break;
            }
            
            if( retry_count++>=10 )
            {
                printf("font size adjust : retry count > 10\n");
                break;
            }
            
            CFRelease(_handle);

            size_in_point -= 1;
            _handle = CTFontCreateWithName( name_str, size_in_point, NULL );
            ascent = CTFontGetAscent(_handle);
            descent = CTFontGetDescent(_handle);

            PRINTF("font size adjust : step3 : %f,%f,%d\n", ascent, descent, size_in_point );
        }
        
        handle = _handle;
        
        PRINTF("font size adjust : done\n" );
    }

    CFRelease(name_str);

    // 文字の幅を取得する
    {
        UniChar unichars[0x10000];
        for( int i=0 ; i<0x10000 ; ++i )
        {
            unichars[i] = i;
        }
        
        CGGlyph glyphs[0x10000];
        CTFontGetGlyphsForCharacters( handle, unichars, glyphs, 0x10000 );
        
        CGSize advances[0x10000];
        CTFontGetAdvancesForGlyphs( handle, kCTFontDefaultOrientation, glyphs, advances, 0x10000 );
        
        char_width = advances['a'].width;
        char_height = height;
        
        PRINTF("char size = %d,%d\n", char_width, char_height );
        PRINTF("advance[W] = %f,%f\n", advances['W'].width, advances['W'].height );
        PRINTF("advance[i] = %f,%f\n", advances['i'].width, advances['i'].height );
        
        zenkaku_table.clear();
        for( int i=0 ; i<=0x10000 ; i++ )
        {
            zenkaku_table.push_back(advances[i].width >= char_width * 2);
        }
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
    //TRACE;
    
	WindowMac * window = (WindowMac*)this->window;
	ImageMac * image = (ImageMac*)this->image;

	if( !show )
	{
		return;
	}

    // TODO : paint_rectの範囲外だったら再描画しない
    
   	if( image )
   	{
        // FIXME : halftone, transparent をちゃんと使う
        //CGContextSetBlendMode( window->paint_gctx, kCGBlendModeNormal );
        
        CGContextDrawImage( window->paint_gctx, CGRectMake( x, window->paint_client_size.cy - y - height, width, height ), image->handle );

        window->perf_fillrect_count++;
    }
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

    DrawOffscreen();

	while( (int)char_buffer.size() <= y+height+delta_y )
	{
        Line * line = new Line();
        char_buffer.push_back(line);
	}

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
    
    WARN_NOT_IMPLEMENTED;

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

    // 必要だったらオフスクリーンバッファを生成
	if( offscreen_handle==NULL || offscreen_context==NULL || offscreen_size.cx!=width || offscreen_size.cy!=height )
	{
		if(offscreen_context){ CGContextRelease(offscreen_context); }
		if(offscreen_handle){ CGLayerRelease(offscreen_handle); }

		offscreen_size.cx = width;
		offscreen_size.cy = height;

		if(offscreen_size.cx<1){ offscreen_size.cx=1; }
		if(offscreen_size.cy<1){ offscreen_size.cy=1; }

        offscreen_handle = CGLayerCreateWithContext( window->paint_gctx, CGSizeMake(offscreen_size.cx,offscreen_size.cy), NULL );
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
                    CGContextSetRGBFillColor( offscreen_context, chr.attr.bg_color[0].r/255.0f, chr.attr.bg_color[0].g/255.0f, chr.attr.bg_color[0].b/255.0f, 1 );
                    CGContextFillRect( offscreen_context, CGRectMake( x * font->char_width, offscreen_size.cy - (y+1) * font->char_height + (offscreen_size.cy % font->char_height), (x2-x) * font->char_width, font->char_height) );
                    
					window->perf_fillrect_count ++;
				}
				else if( chr.attr.bg & Attribute::BG_Gradation )
				{
                    // FIXME : グラデーションになってない
                    
                    CGContextSetRGBFillColor( offscreen_context, chr.attr.bg_color[0].r/255.0f, chr.attr.bg_color[0].g/255.0f, chr.attr.bg_color[0].b/255.0f, 1 );
                    CGContextFillRect( offscreen_context, CGRectMake( x * font->char_width, offscreen_size.cy - (y+1) * font->char_height + (offscreen_size.cy % font->char_height), (x2-x) * font->char_width, font->char_height) );
                    
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
                    CGContextClearRect( offscreen_context, CGRectMake( x * font->char_width, offscreen_size.cy - (y+1) * font->char_height + (offscreen_size.cy % font->char_height), (x2-x) * font->char_width, font->char_height) );

					window->perf_fillrect_count ++;
				}

                // テキスト描画
                {
                    CFMutableAttributedStringRef attrString = CFAttributedStringCreateMutable(kCFAllocatorDefault, 0 );
                    
                    // 文字列
                    CFStringRef str = CFStringCreateWithBytes(
                                                           NULL,
                                                           (const UInt8*)work_text,
                                                           work_len * sizeof(wchar_t),
                                                           kCFStringEncodingUTF32LE,
                                                           false);
                    CFAttributedStringReplaceString(attrString, CFRangeMake(0,0), str );
                    CFRelease(str);
                    
                    // フォント
                    CFAttributedStringSetAttribute(attrString, CFRangeMake(0,work_len), kCTFontAttributeName, font->handle);

                    // 色
                    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
                    CGFloat components[] = { chr.attr.fg_color.r/255.0f, chr.attr.fg_color.g/255.0f, chr.attr.fg_color.b/255.0f, 1 };
                    CGColorRef color = CGColorCreate(colorSpace, components );
                    CFAttributedStringSetAttribute(attrString, CFRangeMake(0,work_len), kCTForegroundColorAttributeName, color);
                    CGColorRelease(color);
                    CGColorSpaceRelease(colorSpace);
                    
                    // 行の描画
                    CTLineRef line = CTLineCreateWithAttributedString(attrString);
                    CGContextSetTextMatrix( offscreen_context, CGAffineTransformIdentity);
                    CGContextSetTextPosition( offscreen_context, x * font->char_width, offscreen_size.cy - y * font->char_height - font->ascent + (offscreen_size.cy % font->char_height) );
                    CTLineDraw(line,offscreen_context);
                    
                    CFRelease(line);
                    CFRelease(attrString);
                }
                
				window->perf_drawtext_count ++;

				for( int line=0 ; line<2 ; ++line )
				{
		            if( chr.attr.line[line] & (Attribute::Line_Left|Attribute::Line_Right|Attribute::Line_Top|Attribute::Line_Bottom) )
		            {
                        CGContextSetRGBStrokeColor( offscreen_context, chr.attr.line_color[line].r/255.0, chr.attr.line_color[line].g/255.0, chr.attr.line_color[line].b/255.0, 1 );
                        CGContextSetLineWidth(offscreen_context, 1);
                        CGContextBeginPath(offscreen_context);

		            	if(chr.attr.line[line] & Attribute::Line_Dot)
		            	{
                            /*
							LOGBRUSH log_brush;
							log_brush.lbColor = chr.attr.line_color[line];
							log_brush.lbHatch = 0;
							log_brush.lbStyle = BS_SOLID;
							DWORD pattern[] = { 1, 1 };
					    
						    hPen = ExtCreatePen( PS_GEOMETRIC | PS_ENDCAP_FLAT | PS_USERSTYLE, 1, &log_brush, 2, pattern );
                            */
		            	}
		            	else
		            	{
                            /*
						    hPen = CreatePen( PS_SOLID, 1, chr.attr.line_color[line] );
                            */
		            	}

						if(chr.attr.line[line] & Attribute::Line_Left)
						{
                            CGContextMoveToPoint(offscreen_context, x * font->char_width + 0.5, offscreen_size.cy - (y+1) * font->char_height + (offscreen_size.cy % font->char_height) );
                            CGContextAddLineToPoint(offscreen_context, x * font->char_width + 0.5, offscreen_size.cy - (y) * font->char_height + (offscreen_size.cy % font->char_height) );
                            
                            /*
							DrawVerticalLine(
								x * font->char_width, 
								y * font->char_height, 
								(y+1) * font->char_height, 
								chr.attr.line_color[line], 
								(chr.attr.line[line] & Attribute::Line_Dot)!=0 );
                            */
						}

						if(chr.attr.line[line] & Attribute::Line_Bottom)
						{
                            CGContextMoveToPoint(offscreen_context, x * font->char_width, offscreen_size.cy - (y+1) * font->char_height + (offscreen_size.cy % font->char_height) + 0.5 );
                            CGContextAddLineToPoint(offscreen_context, x2 * font->char_width, offscreen_size.cy - (y+1) * font->char_height + (offscreen_size.cy % font->char_height) + 0.5 );

                            /*
							DrawHorizontalLine( 
								x * font->char_width, 
								(y+1) * font->char_height - 1, 
								x2 * font->char_width, 
								chr.attr.line_color[line], 
								(chr.attr.line[line] & Attribute::Line_Dot)!=0 );
                            */
						}

						if(chr.attr.line[line] & Attribute::Line_Right)
						{
                            CGContextMoveToPoint(offscreen_context, x2 * font->char_width - 0.5, offscreen_size.cy - (y+1) * font->char_height + (offscreen_size.cy % font->char_height) );
                            CGContextAddLineToPoint(offscreen_context, x2 * font->char_width - 0.5, offscreen_size.cy - (y) * font->char_height + (offscreen_size.cy % font->char_height) );

                            /*
							DrawVerticalLine( 
								x2 * font->char_width - 1, 
			                	y * font->char_height,
								(y+1) * font->char_height, 
								chr.attr.line_color[line], 
								(chr.attr.line[line] & Attribute::Line_Dot)!=0 );
                            */
						}

						if(chr.attr.line[line] & Attribute::Line_Top)
						{
                            CGContextMoveToPoint(offscreen_context, x * font->char_width, offscreen_size.cy - (y) * font->char_height + (offscreen_size.cy % font->char_height) - 0.5 );
                            CGContextAddLineToPoint(offscreen_context, x2 * font->char_width, offscreen_size.cy - (y) * font->char_height + (offscreen_size.cy % font->char_height) - 0.5 );

                            /*
							DrawHorizontalLine( 
								x * font->char_width, 
			                	y * font->char_height,
				            	x2 * font->char_width,
								chr.attr.line_color[line], 
								(chr.attr.line[line] & Attribute::Line_Dot)!=0 );
                            */
						}

                        CGContextStrokePath(offscreen_context);
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

void TextPlaneMac::Draw( const Rect & paint_rect )
{
	FUNC_TRACE;

	WindowMac * window = (WindowMac*)this->window;

	if( !show )
	{
		return;
	}
    
    // TODO : paint_rectの範囲外だったら再描画しない

	// Textのオフスクリーンレイヤを描画
	DrawOffscreen();

	// テキストレイヤを実スクリーンに描画
	{
        CGContextDrawLayerAtPoint( window->paint_gctx, CGPointMake(x, window->paint_client_size.cy - y - height), offscreen_handle);
        
        window->perf_fillrect_count++;
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
    WindowMac * window = (WindowMac*)owner;
    return window->drawRect( rect, gctx );
}

int WindowMac::drawRect( CGRect rect, CGContextRef gctx )
{
    TRACE;
    
    // なぜかウインドウ作成直後のsetFrameでは位置設定が効かないので、ここで遅延して設定する
    // (runModalForWindow を呼び出したときに、位置がリセットされているような振る舞い。)
    if(initial_rect_set)
    {
        ckit_Window_SetWindowRect(handle, initial_rect);
        initial_rect_set=false;
    }

    // ウインドウシステムの要請なのでdirtyにマーク
    CGSize client_size;
    ckit_Window_GetClientSize( handle, &client_size );
	appendDirtyRect( Rect(0,0,client_size.width,client_size.height) );
    
    // 描画する
    beginPaint( gctx );
    flushPaint();
    endPaint();
    
    return 0;
}

static int _windowShouldClose( void * owner )
{
    WindowMac * window = (WindowMac*)owner;
    return window->windowShouldClose();
}

int WindowMac::windowShouldClose()
{
    TRACE;
    
	PythonUtil::GIL_Ensure gil_ensure;
    
    if( close_handler )
    {
        PyObject * pyarglist = Py_BuildValue("()");
        PyObject * pyresult = PyEval_CallObject( close_handler, pyarglist );
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
        quit_requested = true;
    }

    return 0;
}

static int _windowDidResize( void * owner, CGSize size )
{
    WindowMac * window = (WindowMac*)owner;
    return window->windowDidResize(size);
}

int WindowMac::windowDidResize( CGSize size )
{
    callSizeHandler(size);
    return 0;
}

int _windowWillResize( void * owner, CGSize * size )
{
    TRACE;
    
    WindowMac * window = (WindowMac*)owner;
    return window->windowWillResize(size);
}

int WindowMac::windowWillResize( CGSize * size )
{
    TRACE;
    
	PythonUtil::GIL_Ensure gil_ensure;

    calculateFrameSize();
    
    int width = size->width - window_frame_size.cx;
    int height = size->height - window_frame_size.cy;
    
	if( sizing_handler )
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
    
    size->width = width + window_frame_size.cx;
    size->height= height + window_frame_size.cy;
    
    return 0;
}

int _windowDidBecomeKey( void * owner )
{
    WindowMac * window = (WindowMac*)owner;
    return window->windowDidBecomeKey();
}

int WindowMac::windowDidBecomeKey()
{
    TRACE;
    
	PythonUtil::GIL_Ensure gil_ensure;
    
    if( activate_handler )
    {
        PyObject * pyarglist = Py_BuildValue( "(i)", 1 );
        PyObject * pyresult = PyEval_CallObject( activate_handler, pyarglist );
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

int _windowDidResignKey( void * owner )
{
    WindowMac * window = (WindowMac*)owner;
    return window->windowDidResignKey();
}

int WindowMac::windowDidResignKey()
{
    TRACE;
    
	PythonUtil::GIL_Ensure gil_ensure;
    
    if( activate_handler )
    {
        PyObject * pyarglist = Py_BuildValue( "(i)", 0 );
        PyObject * pyresult = PyEval_CallObject( activate_handler, pyarglist );
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

void WindowMac::callSizeHandler( Size size )
{
    TRACE;
    
	PythonUtil::GIL_Ensure gil_ensure;
    
    calculateFrameSize();
    
    if( initialized && size_handler )
    {
        PyObject * pyarglist = Py_BuildValue("(ii)", size.cx, size.cy );
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
}

void WindowMac::calculateFrameSize()
{
    CGRect window_rect;
    ckit_Window_GetWindowRect( handle, &window_rect );

    CGSize client_size;
    ckit_Window_GetClientSize( handle, &client_size );
    
    window_frame_size.cx = window_rect.size.width - client_size.width;
    window_frame_size.cy = window_rect.size.height - client_size.height;
}

static int _timerHandler( void * owner, CocoaObject * timer )
{
    //TRACE;
    
    WindowMac * window = (WindowMac*)owner;
    return window->timerHandler(timer);
}

int WindowMac::timerHandler( CocoaObject * timer )
{
	PythonUtil::GIL_Ensure gil_ensure;

    if( timer == timer_paint )
    {
        if(dirty)
        {
            ckit_Window_SetNeedsRedraw(handle);
        }

        if( delayed_call_list.size() )
        {
            delayed_call_list_ref_count ++;
            
            for( std::list<DelayedCallInfo>::iterator i=delayed_call_list.begin(); i!=delayed_call_list.end() ; i++ )
            {
                if(i->pyobj)
                {
                    i->timeout -= TIMER_PAINT_INTERVAL * 1000;
                    if(i->timeout<=0)
                    {
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
            
            delayed_call_list_ref_count --;
            
            if(delayed_call_list_ref_count==0)
            {
                for( std::list<DelayedCallInfo>::iterator i=delayed_call_list.begin(); i!=delayed_call_list.end() ; )
                {
                    if( (i->pyobj)==0 )
                    {
                        i = delayed_call_list.erase(i);
                    }
                    else
                    {
                        i++;
                    }
                }
            }
        }
    }
    else if( timer == timer_check_quit )
    {
        // FIXME : 一番深い messageLoop だけを Quit 判定するようにすべき
        
        if(quit_requested)
        {
            quit_requested = false;
            ckit_Window_Quit(handle);
        }
        
        if(messageloop_continue_cond_func_stack.size()>0)
        {
            PyObject * continue_cond_func = messageloop_continue_cond_func_stack.back();
            if(continue_cond_func)
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
                        ckit_Window_Quit(handle);
                    }
                }
                else
                {
                    PyErr_Print();
                }
            }
        }
    }
    else
    {
        timer_list_ref_count ++;

        for( std::list<TimerInfo>::iterator i=timer_list.begin(); i!=timer_list.end() ; i++ )
        {
            if( timer == i->handle )
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

        timer_list_ref_count --;

        if(timer_list_ref_count==0)
        {
            for( std::list<TimerInfo>::iterator i=timer_list.begin(); i!=timer_list.end() ; )
            {
                if( (i->pyobj)==0 )
                {
                    i = timer_list.erase(i);
                }
                else
                {
                    i++;
                }
            }
        }
    }
    
    return 0;
}

static int _keyDown( void * owner, int vk, int mod )
{
    //TRACE;
    
    WindowMac * window = (WindowMac*)owner;
    return window->keyDown(vk,mod);
}

int WindowMac::keyDown( int vk, int mod )
{
	PythonUtil::GIL_Ensure gil_ensure;

    if(keydown_handler)
    {
        PyObject * pyarglist = Py_BuildValue("(ii)", vk, mod );
        PyObject * pyresult = PyEval_CallObject( keydown_handler, pyarglist );
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

static int _keyUp( void * owner, int vk, int mod )
{
    //TRACE;
    
    WindowMac * window = (WindowMac*)owner;
    return window->keyUp(vk,mod);
}

int WindowMac::keyUp( int vk, int mod )
{
	PythonUtil::GIL_Ensure gil_ensure;
    
    if(keyup_handler)
    {
        PyObject * pyarglist = Py_BuildValue("(ii)", vk, mod );
        PyObject * pyresult = PyEval_CallObject( keyup_handler, pyarglist );
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

static int _insertText( void * owner, const wchar_t * text, int mod )
{
    //TRACE;
    
    WindowMac * window = (WindowMac*)owner;
    return window->insertText(text,mod);
}

int WindowMac::insertText( const wchar_t * text, int mod )
{
	PythonUtil::GIL_Ensure gil_ensure;
    
    if(char_handler)
    {
        for( const wchar_t * t=text ; *t ; ++t )
        {
            PyObject * pyarglist = Py_BuildValue("(ii)", *t, mod );
            PyObject * pyresult = PyEval_CallObject( char_handler, pyarglist );
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
}

int _mouse( void * owner, const ckit_MouseEvent * event )
{
    WindowMac * window = (WindowMac*)owner;
    return window->mouse(event);
}

int WindowMac::mouse( const ckit_MouseEvent * event )
{
    TRACE;
    
	PythonUtil::GIL_Ensure gil_ensure;
    
    switch(event->type)
    {
    case ckit_MouseEventType_LeftDown:
        if(lbuttondown_handler)
        {
			//int mod = getModKey();
            int mod = 0; // FIXME : モディファイアキー
            
            CGSize client_size;
            ckit_Window_GetClientSize( handle, &client_size );
            Point location( event->location.x, client_size.height - event->location.y );
			
			PyObject * pyarglist = Py_BuildValue("(iii)", location.x, location.y, mod );
			PyObject * pyresult = PyEval_CallObject( lbuttondown_handler, pyarglist );
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

    case ckit_MouseEventType_LeftUp:
        if(lbuttonup_handler)
        {
			//int mod = getModKey();
            int mod = 0; // FIXME : モディファイアキー
			
            CGSize client_size;
            ckit_Window_GetClientSize( handle, &client_size );
            Point location( event->location.x, client_size.height - event->location.y );
			
			PyObject * pyarglist = Py_BuildValue("(iii)", location.x, location.y, mod );
			PyObject * pyresult = PyEval_CallObject( lbuttonup_handler, pyarglist );
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

    case ckit_MouseEventType_RightDown:
        if(rbuttondown_handler)
        {
			//int mod = getModKey();
            int mod = 0; // FIXME : モディファイアキー
            
            CGSize client_size;
            ckit_Window_GetClientSize( handle, &client_size );
            Point location( event->location.x, client_size.height - event->location.y );
			
			PyObject * pyarglist = Py_BuildValue("(iii)", location.x, location.y, mod );
			PyObject * pyresult = PyEval_CallObject( rbuttondown_handler, pyarglist );
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
            
    case ckit_MouseEventType_RightUp:
        if(rbuttonup_handler)
        {
			//int mod = getModKey();
            int mod = 0; // FIXME : モディファイアキー
			
            CGSize client_size;
            ckit_Window_GetClientSize( handle, &client_size );
            Point location( event->location.x, client_size.height - event->location.y );
			
			PyObject * pyarglist = Py_BuildValue("(iii)", location.x, location.y, mod );
			PyObject * pyresult = PyEval_CallObject( rbuttonup_handler, pyarglist );
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
            
    case ckit_MouseEventType_MiddleDown:
        if(mbuttondown_handler)
        {
			//int mod = getModKey();
            int mod = 0; // FIXME : モディファイアキー
            
            CGSize client_size;
            ckit_Window_GetClientSize( handle, &client_size );
            Point location( event->location.x, client_size.height - event->location.y );
			
			PyObject * pyarglist = Py_BuildValue("(iii)", location.x, location.y, mod );
			PyObject * pyresult = PyEval_CallObject( mbuttondown_handler, pyarglist );
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
            
    case ckit_MouseEventType_MiddleUp:
        if(mbuttonup_handler)
        {
			//int mod = getModKey();
            int mod = 0; // FIXME : モディファイアキー
			
            CGSize client_size;
            ckit_Window_GetClientSize( handle, &client_size );
            Point location( event->location.x, client_size.height - event->location.y );
			
			PyObject * pyarglist = Py_BuildValue("(iii)", location.x, location.y, mod );
			PyObject * pyresult = PyEval_CallObject( mbuttonup_handler, pyarglist );
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
            
    case ckit_MouseEventType_LeftDoubleClick:
        if(lbuttondoubleclick_handler)
        {
			//int mod = getModKey();
            int mod = 0; // FIXME : モディファイアキー
            
            CGSize client_size;
            ckit_Window_GetClientSize( handle, &client_size );
            Point location( event->location.x, client_size.height - event->location.y );
			
			PyObject * pyarglist = Py_BuildValue("(iii)", location.x, location.y, mod );
			PyObject * pyresult = PyEval_CallObject( lbuttondoubleclick_handler, pyarglist );
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
            
    case ckit_MouseEventType_RightDoubleClick:
        if(rbuttondoubleclick_handler)
        {
			//int mod = getModKey();
            int mod = 0; // FIXME : モディファイアキー
            
            CGSize client_size;
            ckit_Window_GetClientSize( handle, &client_size );
            Point location( event->location.x, client_size.height - event->location.y );
			
			PyObject * pyarglist = Py_BuildValue("(iii)", location.x, location.y, mod );
			PyObject * pyresult = PyEval_CallObject( rbuttondoubleclick_handler, pyarglist );
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
            
    case ckit_MouseEventType_MiddleDoubleClick:
        if(mbuttondoubleclick_handler)
        {
			//int mod = getModKey();
            int mod = 0; // FIXME : モディファイアキー
            
            CGSize client_size;
            ckit_Window_GetClientSize( handle, &client_size );
            Point location( event->location.x, client_size.height - event->location.y );
			
			PyObject * pyarglist = Py_BuildValue("(iii)", location.x, location.y, mod );
			PyObject * pyresult = PyEval_CallObject( mbuttondoubleclick_handler, pyarglist );
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
            
    case ckit_MouseEventType_Move:
        if(mousemove_handler)
        {
			//int mod = getModKey();
            int mod = 0; // FIXME : モディファイアキー
			
            CGSize client_size;
            ckit_Window_GetClientSize( handle, &client_size );
            Point location( event->location.x, client_size.height - event->location.y );
			
			PyObject * pyarglist = Py_BuildValue("(iii)", location.x, location.y, mod );
			PyObject * pyresult = PyEval_CallObject( mousemove_handler, pyarglist );
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

    case ckit_MouseEventType_Wheel:
        if(mousewheel_handler)
        {
			//int mod = getModKey();
            int mod = 0; // FIXME : モディファイアキー
			
            CGSize client_size;
            ckit_Window_GetClientSize( handle, &client_size );
            Point location( event->location.x, client_size.height - event->location.y );
            
            printf( "ckit_MouseEventType_Wheel : %f, %f\n", event->delta_x, event->delta_y );
			
			PyObject * pyarglist = Py_BuildValue("(iiii)", location.x, location.y, (int)event->delta_y, mod );
			PyObject * pyresult = PyEval_CallObject( mousewheel_handler, pyarglist );
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
    
    return 0;
}

int _imePosition( void * owner, CGRect * caret_rect )
{
    WindowMac * window = (WindowMac*)owner;
    return window->imePosition(caret_rect);
}

int WindowMac::imePosition( CGRect * _caret_rect )
{
    CGSize client_size;
    ckit_Window_GetClientSize( handle, &client_size );
    
    *_caret_rect = CGRectMake( caret_rect.left, client_size.height - caret_rect.bottom, caret_rect.right - caret_rect.left, caret_rect.bottom - caret_rect.top );
    
    return 0;
}

ckit_Window_Callbacks callbacks = {
    _drawRect,
    _windowShouldClose,
    _windowDidResize,
    _windowWillResize,
    _windowDidBecomeKey,
    _windowDidResignKey,
    _timerHandler,
    _keyDown,
    _keyUp,
    _insertText,
    _mouse,
    _imePosition,
};

WindowMac::WindowMac( Param & _params )
	:
	WindowBase(_params),
    initialized(false),
    handle(0),
    initial_rect_set(false),
    timer_paint(0),
    timer_check_quit(0),
    bg_color(_params.bg_color),
    caret_color0(_params.caret0_color),
    caret_color1(_params.caret1_color),
    ime_font(0),
    paint_gctx(0)
{
	FUNC_TRACE;
    
    // initialize graphics system
    memset( &window_frame_size, 0, sizeof(window_frame_size) );
    memset( &initial_rect, 0, sizeof(initial_rect) );
    memset( &paint_client_size, 0, sizeof(paint_client_size) );
    
    ckit_Window_Create_Parameters params;
    memset(&params, 0, sizeof(params));
    params.callbacks = &callbacks;
    params.owner = this;
    if(_params.parent_window)
    {
        params.parent_window = ((WindowMac*)_params.parent_window)->handle;
    }
    params.title = _params.title.c_str();
    params.titlebar = _params.title_bar;
    params.minimizable = _params.minimizebox;
    params.resizable = _params.resizable;
    
    // create window
    if( ckit_Window_Create( &params, &handle )!=0 )
    {
        printf("ckit_Window_Create failed\n");
        return;
    }

    calculateFrameSize();
    
    // ウインドウ初期位置
    initial_rect = calculateWindowRectFromPositionSizeOrigin(_params.winpos_x, _params.winpos_y, _params.winsize_w, _params.winsize_h, _params.origin);
    ckit_Window_SetWindowRect(handle, initial_rect);
    initial_rect_set = true;
    
    ckit_Window_SetTimer( handle, TIMER_PAINT_INTERVAL, &timer_paint );
    ckit_Window_SetTimer( handle, TIMER_CHECK_QUIT_INTERVAL, &timer_check_quit );
    
    if( _params.show )
    {
        ckit_Window_Show( handle, true, false );
    }
    
    initialized = true;
}

WindowMac::~WindowMac()
{
	FUNC_TRACE;

    // 終了処理中のリソースにアクセスしないように、呼び出し可能オブジェクトを破棄
	clearCallables();

    // タイマー破棄
    ckit_Window_KillTimer( handle, timer_paint );
    ckit_Window_KillTimer( handle, timer_check_quit );
    
    // terminate graphics system
    if(ime_font){ ime_font->Release(); }
    
    // destroy window
    if( ckit_Window_Destroy(handle)!=0 )
    {
        printf("ckit_Window_Destroy failed\n");
        return;
    }
}

WindowHandle WindowMac::getHandle() const
{
    WARN_NOT_IMPLEMENTED;

	return NULL;
}

void WindowMac::beginPaint(CGContextRef _gctx )
{
    TRACE;

    paint_gctx = _gctx;
    
    CGSize size;
    ckit_Window_GetClientSize(handle, &size);
    paint_client_size = size;
}

void WindowMac::endPaint()
{
    TRACE;
    
    paint_gctx = NULL;
}

void WindowMac::paintBackground()
{
    CGContextSetRGBFillColor( paint_gctx, bg_color.r/255.0, bg_color.g/255.0, bg_color.b/255.0, 1 );
    CGContextFillRect( paint_gctx, CGRectMake(0,0,paint_client_size.cx, paint_client_size.cy) );
}

void WindowMac::paintPlanes()
{
	FUNC_TRACE;
    
    Rect paint_rect(0,0,paint_client_size.cx,paint_client_size.cy);

	std::list<PlaneBase*>::const_iterator i;
    for( i=plane_list.begin() ; i!=plane_list.end() ; i++ )
    {
    	PlaneBase * plane = *i;
		plane->Draw( paint_rect );
    }
}

void WindowMac::paintMarkedText()
{
	FUNC_TRACE;

    CFMutableAttributedStringRef attrString;
    ckit_Window_GetMarkedText( handle, &attrString );
    
    if(!attrString)
    {
        return;
    }
    
    if(!ime_font)
    {
        return;
    }

    CFStringRef str = CFAttributedStringGetString(attrString);
    size_t len = CFStringGetLength(str);
    
    // 幅を取得
    int width = 0;
    for( int i=0 ; i<len ; ++i )
    {
        UniChar c = CFStringGetCharacterAtIndex(str, i);
        
        if( ime_font->zenkaku_table[c] )
        {
            width += 2;
        }
        else
        {
            width += 1;
        }
    }

    // 背景を塗る
    {
        CGContextSetRGBFillColor( paint_gctx, 1, 1, 1, 1 );
        CGContextFillRect( paint_gctx, CGRectMake( caret_rect.left, paint_client_size.cy - caret_rect.bottom, width * ime_font->char_width, ime_font->char_height) );
        
        perf_fillrect_count ++;
    }
    
    // フォント
    CFAttributedStringSetAttribute(attrString, CFRangeMake(0,len), kCTFontAttributeName, ime_font->handle);

    // １行描画
    CTLineRef line = CTLineCreateWithAttributedString(attrString);
    CGContextSetTextMatrix( paint_gctx, CGAffineTransformIdentity);
    CGContextSetTextPosition( paint_gctx, caret_rect.left, paint_client_size.cy - caret_rect.top - ime_font->ascent );
    CTLineDraw(line,paint_gctx);
    
    perf_drawtext_count ++;
}

void WindowMac::paintCaret()
{
    if( caret && caret_blink )
    {
		if(caret_rect.right>0 || caret_rect.bottom>0)
		{
            int ime_opened = 0;
            ckit_Window_IsImeOpened( handle, &ime_opened );
            
			if(ime_opened)
			{
                CGContextSetRGBFillColor( paint_gctx, caret_color1.r/255.0, caret_color1.g/255.0, caret_color1.b/255.0, 1 );
			}
			else
			{
                CGContextSetRGBFillColor( paint_gctx, caret_color0.r/255.0, caret_color0.g/255.0, caret_color0.b/255.0, 1 );
			}

            // 文字と被って見えなくならないように、XOR的な処理
            CGContextSetBlendMode( paint_gctx, kCGBlendModeDifference );
            //CGContextSetBlendMode( paint_gctx, kCGBlendModeExclusion );

            CGContextFillRect( paint_gctx, CGRectMake( caret_rect.left, paint_client_size.cy - caret_rect.bottom, caret_rect.right - caret_rect.left, caret_rect.bottom - caret_rect.top ) );

            // BlendModeを戻す
            CGContextSetBlendMode( paint_gctx, kCGBlendModeNormal );
		}
    }
}

void WindowMac::flushPaint()
{
	FUNC_TRACE;

    if(!dirty){ return; }
    
    paintBackground();
    paintPlanes();
    paintCaret();
    paintMarkedText();

	clearDirtyRect();
}

void WindowMac::enumFonts( std::vector<std::wstring> * font_list )
{
    WARN_NOT_IMPLEMENTED;

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
	FUNC_TRACE;
    
    bg_color = color;
    
    CGSize client_size;
    ckit_Window_GetClientSize( handle, &client_size );
	appendDirtyRect( Rect(0,0,client_size.width,client_size.height) );
}

void WindowMac::setFrameColor( Color color )
{
    WARN_NOT_IMPLEMENTED;

    /*
    if(frame_pen){ DeleteObject(frame_pen); }
    frame_pen = CreatePen( PS_SOLID, 0, color );

	RedrawWindow( hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE );
    */
}

void WindowMac::setCaretColor( Color color0, Color color1 )
{
	FUNC_TRACE;

    caret_color0 = color0;
    caret_color1 = color1;

	appendDirtyRect( caret_rect );
}

void WindowMac::setMenu( PyObject * _menu )
{
	if( menu == _menu ) return;
	
	Py_XDECREF(menu);
	menu = _menu;
	Py_XINCREF(menu);
	
	clearMenuCommands();

    WARN_NOT_IMPLEMENTED;

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

CGRect WindowMac::calculateWindowRectFromPositionSizeOrigin( int x, int y, int width, int height, int origin )
{
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
    
    // 左下原点に変換
    CGSize screen_size;
    ckit_Window_GetScreenSize(handle, &screen_size);
    y = screen_size.height-(y+window_h);
    
    return CGRectMake( x, y, window_w, window_h );
}

void WindowMac::setPositionAndSize( int x, int y, int width, int height, int origin )
{
	FUNC_TRACE;
    
    CGRect rect = calculateWindowRectFromPositionSizeOrigin(x,y,width,height,origin);
    
    if(initial_rect_set)
    {
        // 最初のdrawRectが呼ばれる前にsetPositionAndSizeが呼ばれたら遅延設定のために保存する
        initial_rect = rect;
    }

    ckit_Window_SetWindowRect( handle, rect );
    
    // size_handler を明示的に呼び出し
    callSizeHandler( Size(width, height) );
    
	Rect dirty_rect = { 0, 0, width, height };
	appendDirtyRect( dirty_rect );
}

void WindowMac::setCapture()
{
    WARN_NOT_IMPLEMENTED;

    /*
	SetCapture(hwnd);
     */
}

void WindowMac::releaseCapture()
{
    WARN_NOT_IMPLEMENTED;

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
    WARN_NOT_IMPLEMENTED;

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
    WARN_NOT_IMPLEMENTED;

    /*
	PostMessage( hwnd, WM_NCLBUTTONDOWN, (WPARAM)HTCAPTION, ((y<<16)|x) );
     */
}

void WindowMac::show( bool show, bool activate )
{
	FUNC_TRACE;

    ckit_Window_Show( handle, show, activate );
}

void WindowMac::enable( bool enable )
{
	FUNC_TRACE;

    WARN_NOT_IMPLEMENTED;

    /*
    EnableWindow( hwnd, enable );
     */
}

void WindowMac::activate()
{
	FUNC_TRACE;

    ckit_Window_Activate(handle);
}

void WindowMac::inactivate()
{
	FUNC_TRACE;

    WARN_NOT_IMPLEMENTED;

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
    
    ckit_Window_SetForeground(handle);
}

void WindowMac::restore()
{
	FUNC_TRACE;

    WARN_NOT_IMPLEMENTED;

    /*
    ShowWindow( hwnd, SW_RESTORE );
     */
}

void WindowMac::maximize()
{
	FUNC_TRACE;

    WARN_NOT_IMPLEMENTED;

    /*
    ShowWindow( hwnd, SW_MAXIMIZE );
     */
}

void WindowMac::minimize()
{
	FUNC_TRACE;

    WARN_NOT_IMPLEMENTED;

    /*
    ShowWindow( hwnd, SW_MINIMIZE );
     */
}

void WindowMac::topmost( bool topmost )
{
	FUNC_TRACE;

    WARN_NOT_IMPLEMENTED;

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

    WARN_NOT_IMPLEMENTED;

    /*
	return ::IsWindowEnabled( hwnd )!=FALSE;
     */
    return false;
}

bool WindowMac::isVisible()
{
	FUNC_TRACE;

    int visible = 0;
    ckit_Window_IsVisible(handle, &visible);
    
    return visible!=0;
}

bool WindowMac::isMaximized()
{
	FUNC_TRACE;
	
    int maximized = 0;
    ckit_Window_IsMaximized(handle, &maximized);

    return maximized!=0;
}

bool WindowMac::isMinimized()
{
	FUNC_TRACE;

    int minimized = 0;
    ckit_Window_IsMinimized(handle, &minimized);
    
    return minimized!=0;
}

bool WindowMac::isActive()
{
	FUNC_TRACE;
	
    int active = 0;
    ckit_Window_IsActive(handle, &active);
    
    return active!=0;
}

bool WindowMac::isForeground()
{
	FUNC_TRACE;

    WARN_NOT_IMPLEMENTED;

	/*
	return hwnd==GetForegroundWindow();
     */
    return true;
}

void WindowMac::getWindowRect( Rect * rect )
{
	FUNC_TRACE;
    
    CGRect _rect;
    ckit_Window_GetWindowRect( handle, &_rect );
	
    // 左上原点に変換
    CGSize screen_size;
    ckit_Window_GetScreenSize( handle, &screen_size );
    _rect.origin.y = screen_size.height - _rect.origin.y - _rect.size.height;
    
    *rect = Rect(_rect);
}

void WindowMac::getClientSize( Size * size )
{
	FUNC_TRACE;
	
    CGSize _size;
    ckit_Window_GetClientSize( handle, &_size );
	
    *size = Size(_size);
}

void WindowMac::getNormalWindowRect( Rect * rect )
{
	FUNC_TRACE;

    // FIXME : 最大化されていない状態の矩形を返すべきか
    
    CGRect _rect;
    ckit_Window_GetWindowRect( handle, &_rect );

    // 左上原点に変換
    CGSize screen_size;
    ckit_Window_GetScreenSize( handle, &screen_size );
    _rect.origin.y = screen_size.height - _rect.origin.y - _rect.size.height;
	
    *rect = Rect(_rect);
}

void WindowMac::getNormalClientSize( Size * size )
{
	FUNC_TRACE;

    // FIXME : 最大化されていない状態のサイズを返すべきか
    
    CGSize _size;
    ckit_Window_GetClientSize( handle, &_size );
	
    *size = Size(_size);
}

void WindowMac::clientToScreen(Point * _point)
{
	FUNC_TRACE;

    CGSize client_size;
    ckit_Window_GetClientSize( handle, &client_size );

    CGSize screen_size;
    ckit_Window_GetScreenSize( handle, &screen_size );
    
    // 左下原点に変換
    CGPoint point = CGPointMake(_point->x, client_size.height - _point->y);
    
    // スクリーン座標系に変換
    ckit_Window_ClientToScreen(handle, &point);

    // 左上原点に変換
    _point->x = point.x;
    _point->y = screen_size.height - point.y;
}

void WindowMac::screenToClient(Point * _point)
{
	FUNC_TRACE;

    CGSize client_size;
    ckit_Window_GetClientSize( handle, &client_size );
    
    CGSize screen_size;
    ckit_Window_GetScreenSize( handle, &screen_size );
    
    // 左下原点に変換
    CGPoint point = CGPointMake(_point->x, screen_size.height - _point->y);

    // クライアント座標系に変換
    ckit_Window_ScreenToClient(handle, &point);
    
    // 左上原点に変換
    _point->x = point.x;
    _point->y = client_size.height - point.y;
}

void WindowMac::setTimer( TimerInfo * timer_info )
{
    ckit_Window_SetTimer( handle, timer_info->interval/1000.0f, (CocoaObject**)&timer_info->handle );
}

void WindowMac::killTimer( TimerInfo * timer_info )
{
    ckit_Window_KillTimer( handle, (CocoaObject*)timer_info->handle );
}

void WindowMac::setHotKey( int vk, int mod, PyObject * func )
{
    WARN_NOT_IMPLEMENTED;

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
    WARN_NOT_IMPLEMENTED;

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
    ckit_Window_SetTitle( handle, text );
}

bool WindowMac::popupMenu( int x, int y, PyObject * items )
{
    WARN_NOT_IMPLEMENTED;

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

    ckit_Window_EnableIme( handle, enable );
}

void WindowMac::setImeFont( FontBase * _font )
{
	FUNC_TRACE;

    if( ime_font == _font ){ return; }

    if(ime_font){ ime_font->Release(); }
    ime_font = (FontMac*)_font;
    if(ime_font){ ime_font->AddRef(); }
}

void WindowMac::messageLoop( PyObject * continue_cond_func )
{
    Py_BEGIN_ALLOW_THREADS
    
    messageloop_continue_cond_func_stack.push_back(continue_cond_func);
    
    ckit_Window_MessageLoop(handle);
    
    messageloop_continue_cond_func_stack.pop_back();

    Py_END_ALLOW_THREADS
}

void WindowMac::removeKeyMessage()
{
	FUNC_TRACE;

    ckit_Window_RemoveKeyMessage(handle);
}

void GlobalMac::setClipboardText( const wchar_t * text )
{
	FUNC_TRACE;
    
    ckit_Global_SetClipboardText(text);
}

std::wstring GlobalMac::getClipboardText()
{
	FUNC_TRACE;
    
    wchar_t * _text;

    // mallocで確保された文字列が格納される
    ckit_Global_GetClipboardText(&_text);
    
    std::wstring text = _text;
    
    free(_text);
    
    return text;
}

int GlobalMac::getClipboardChangeCount()
{
	FUNC_TRACE;

    int change_count;
    
    ckit_Global_GetClipboardChangeCount(&change_count);
    
    return change_count;
}

void GlobalMac::beep()
{
	FUNC_TRACE;
    
    ckit_Global_Beep();
}

