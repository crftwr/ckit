//
//  ckitcore_cocoa.m
//  ckitcore
//
//  Created by Shimomura Tomonori on 2014/07/05.
//  Copyright (c) 2014年 craftware. All rights reserved.
//

#import <wchar.h>

#import <Cocoa/Cocoa.h>

#import "ckitcore_cocoa.h"
#import "ckitcore_cocoa_export.h"

//-----------------------------------------------------------------------------

//#define PRINTF printf
#define PRINTF(...)

//#define TRACE printf("%s(%d) : %s\n",__FILE__,__LINE__,__FUNCTION__)
#define TRACE

//-----------------------------------------------------------------------------

@implementation CkitView

- (id)initWithFrame:(NSRect)frame callbacks:(ckit_Window_Callbacks*)_callbacks owner:(void*)_owner
{
    TRACE;

    self = [super initWithFrame:frame];
    if(self)
    {
        self->callbacks = _callbacks;
        self->owner = _owner;
    }
    return self;
}

- (BOOL)windowShouldClose:(id)sender
{
    TRACE;

    callbacks->windowShouldClose( owner );
    
    return FALSE;
}

- (void)drawRect:(NSRect)dirtyRect
{
    TRACE;
    
    [super drawRect:dirtyRect];
    
    // Drawing code here.
    
    CGContextRef gctx = [[NSGraphicsContext currentContext] graphicsPort];
    
    callbacks->drawRect( owner, dirtyRect, gctx );
}

- (void)windowDidResize:(NSNotification *)notification
{
    TRACE;
    
    CGRect rect = [self frame];
    
    callbacks->windowDidResize( owner, rect.size );
}

- (void)windowDidEndLiveResize:(NSNotification *)notification
{
    TRACE;

    CGRect rect = [self frame];
    
    callbacks->windowDidResize( owner, rect.size );
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize
{
    TRACE;

    callbacks->windowWillResize( owner, &frameSize );
    return frameSize;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    callbacks->windowDidBecomeKey(owner);
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    callbacks->windowDidResignKey(owner);
}

-(void)timerHandler:(NSTimer*)timer
{
    //TRACE;
    callbacks->timerHandler( owner, (__bridge CocoaObject *)(timer) );
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

enum
{
    MODKEY_ALT   = 0x00000001,
    MODKEY_CTRL  = 0x00000002,
    MODKEY_SHIFT = 0x00000004,
    MODKEY_WIN   = 0x00000008,
};

enum
{
    VK_LBUTTON        = 0x01,
    VK_RBUTTON        = 0x02,
    VK_CANCEL         = 0x03,
    VK_MBUTTON        = 0x04,
    VK_XBUTTON1       = 0x05,
    VK_XBUTTON2       = 0x06,
    VK_BACK           = 0x08,
    VK_TAB            = 0x09,
    VK_CLEAR          = 0x0C,
    VK_RETURN         = 0x0D,
    VK_SHIFT          = 0x10,
    VK_CONTROL        = 0x11,
    VK_MENU           = 0x12,
    VK_PAUSE          = 0x13,
    VK_CAPITAL        = 0x14,
    VK_KANA           = 0x15,
    VK_HANGUL         = 0x15,
    VK_JUNJA          = 0x17,
    VK_FINAL          = 0x18,
    VK_HANJA          = 0x19,
    VK_KANJI          = 0x19,
    VK_ESCAPE         = 0x1B,
    VK_CONVERT        = 0x1C,
    VK_NONCONVERT     = 0x1D,
    VK_ACCEPT         = 0x1E,
    VK_MODECHANGE     = 0x1F,
    VK_SPACE          = 0x20,
    VK_PRIOR          = 0x21,
    VK_NEXT           = 0x22,
    VK_END            = 0x23,
    VK_HOME           = 0x24,
    VK_LEFT           = 0x25,
    VK_UP             = 0x26,
    VK_RIGHT          = 0x27,
    VK_DOWN           = 0x28,
    VK_SELECT         = 0x29,
    VK_PRINT          = 0x2A,
    VK_EXECUTE        = 0x2B,
    VK_SNAPSHOT       = 0x2C,
    VK_INSERT         = 0x2D,
    VK_DELETE         = 0x2E,
    VK_HELP           = 0x2F,
    VK_0              = 0x30,
    VK_1              = 0x31,
    VK_2              = 0x32,
    VK_3              = 0x33,
    VK_4              = 0x34,
    VK_5              = 0x35,
    VK_6              = 0x36,
    VK_7              = 0x37,
    VK_8              = 0x38,
    VK_9              = 0x39,
    VK_A              = 0x41,
    VK_B              = 0x42,
    VK_C              = 0x43,
    VK_D              = 0x44,
    VK_E              = 0x45,
    VK_F              = 0x46,
    VK_G              = 0x47,
    VK_H              = 0x48,
    VK_I              = 0x49,
    VK_J              = 0x4A,
    VK_K              = 0x4B,
    VK_L              = 0x4C,
    VK_M              = 0x4D,
    VK_N              = 0x4E,
    VK_O              = 0x4F,
    VK_P              = 0x50,
    VK_Q              = 0x51,
    VK_R              = 0x52,
    VK_S              = 0x53,
    VK_T              = 0x54,
    VK_U              = 0x55,
    VK_V              = 0x56,
    VK_W              = 0x57,
    VK_X              = 0x58,
    VK_Y              = 0x59,
    VK_Z              = 0x5A,
    VK_LWIN           = 0x5B,
    VK_RWIN           = 0x5C,
    VK_APPS           = 0x5D,
    VK_NUMPAD0        = 0x60,
    VK_NUMPAD1        = 0x61,
    VK_NUMPAD2        = 0x62,
    VK_NUMPAD3        = 0x63,
    VK_NUMPAD4        = 0x64,
    VK_NUMPAD5        = 0x65,
    VK_NUMPAD6        = 0x66,
    VK_NUMPAD7        = 0x67,
    VK_NUMPAD8        = 0x68,
    VK_NUMPAD9        = 0x69,
    VK_MULTIPLY       = 0x6A,
    VK_ADD            = 0x6B,
    VK_SEPARATOR      = 0x6C,
    VK_SUBTRACT       = 0x6D,
    VK_DECIMAL        = 0x6E,
    VK_DIVIDE         = 0x6F,
    VK_F1             = 0x70,
    VK_F2             = 0x71,
    VK_F3             = 0x72,
    VK_F4             = 0x73,
    VK_F5             = 0x74,
    VK_F6             = 0x75,
    VK_F7             = 0x76,
    VK_F8             = 0x77,
    VK_F9             = 0x78,
    VK_F10            = 0x79,
    VK_F11            = 0x7A,
    VK_F12            = 0x7B,
    VK_F13            = 0x7C,
    VK_F14            = 0x7D,
    VK_F15            = 0x7E,
    VK_F16            = 0x7F,
    VK_F17            = 0x80,
    VK_F18            = 0x81,
    VK_F19            = 0x82,
    VK_F20            = 0x83,
    VK_F21            = 0x84,
    VK_F22            = 0x85,
    VK_F23            = 0x86,
    VK_F24            = 0x87,
    VK_NUMLOCK        = 0x90,
    VK_SCROLL         = 0x91,
    VK_LSHIFT         = 0xA0,
    VK_RSHIFT         = 0xA1,
    VK_LCONTROL       = 0xA2,
    VK_RCONTROL       = 0xA3,
    VK_LMENU          = 0xA4,
    VK_RMENU          = 0xA5,
    VK_BROWSER_BACK     = 0xA6,
    VK_BROWSER_FORWARD  = 0xA7,
    VK_BROWSER_REFRESH  = 0xA8,
    VK_BROWSER_STOP     = 0xA9,
    VK_BROWSER_SEARCH   = 0xAA,
    VK_BROWSER_FAVORITES = 0xAB,
    VK_BROWSER_HOME     = 0xAC,
    VK_VOLUME_MUTE      = 0xAD,
    VK_VOLUME_DOWN      = 0xAE,
    VK_VOLUME_UP        = 0xAF,
    VK_MEDIA_NEXT_TRACK = 0xB0,
    VK_MEDIA_PREV_TRACK = 0xB1,
    VK_MEDIA_STOP       = 0xB2,
    VK_MEDIA_PLAY_PAUSE = 0xB3,
    VK_LAUNCH_MAIL      = 0xB4,
    VK_LAUNCH_MEDIA_SELECT  = 0xB5,
    VK_LAUNCH_APP1      = 0xB6,
    VK_LAUNCH_APP2      = 0xB7,
    VK_OEM_1            = 0xBA,
    VK_OEM_PLUS         = 0xBB,
    VK_OEM_COMMA        = 0xBC,
    VK_OEM_MINUS        = 0xBD,
    VK_OEM_PERIOD       = 0xBE,
    VK_OEM_2            = 0xBF,
    VK_OEM_3            = 0xC0,
    VK_OEM_4            = 0xDB,
    VK_OEM_5            = 0xDC,
    VK_OEM_6            = 0xDD,
    VK_OEM_7            = 0xDE,
    VK_OEM_8            = 0xDF,
    VK_OEM_102          = 0xE2,
    VK_PROCESSKEY     = 0xE5,
    VK_PACKET         = 0xE7,
    VK_ATTN           = 0xF6,
    VK_CRSEL          = 0xF7,
    VK_EXSEL          = 0xF8,
    VK_EREOF          = 0xF9,
    VK_PLAY           = 0xFA,
    VK_ZOOM           = 0xFB,
    VK_NONAME         = 0xFC,
    VK_PA1            = 0xFD,
    VK_OEM_CLEAR      = 0xFE,
};

static int translateVk(int src)
{
    switch(src)
    {
        case 53:
            return VK_ESCAPE;
        case 51:
            return VK_BACK;
        case 36:
            return VK_RETURN;
        case 48:
            return VK_TAB;
        case 49:
            return VK_SPACE;
        case 123:
            return VK_LEFT;
        case 124:
            return VK_RIGHT;
        case 125:
            return VK_DOWN;
        case 126:
            return VK_UP;

        case 0:
            return VK_A;
        case 1:
            return VK_S;
        case 2:
            return VK_D;
        case 3:
            return VK_F;
        case 5:
            return VK_G;
        case 4:
            return VK_H;
        case 38:
            return VK_J;
        case 40:
            return VK_K;
        case 37:
            return VK_L;
        case 12:
            return VK_Q;
        case 13:
            return VK_W;
        case 14:
            return VK_E;
        case 15:
            return VK_R;
        case 17:
            return VK_T;
        case 16:
            return VK_Y;
        case 32:
            return VK_U;
        case 34:
            return VK_I;
        case 31:
            return VK_O;
        case 35:
            return VK_P;
        case 6:
            return VK_Z;
        case 7:
            return VK_X;
        case 8:
            return VK_C;
        case 9:
            return VK_V;
        case 11:
            return VK_B;
        case 45:
            return VK_N;
        case 46:
            return VK_M;
        case 18:
            return VK_1;
        case 19:
            return VK_2;
        case 20:
            return VK_3;
        case 21:
            return VK_4;
        case 23:
            return VK_5;
        case 22:
            return VK_6;
        case 26:
            return VK_7;
        case 28:
            return VK_8;
        case 25:
            return VK_9;
        case 29:
            return VK_0;

        case 27:
            return VK_OEM_MINUS;
        case 24:
            return VK_OEM_PLUS;
        case 43:
            return VK_OEM_COMMA;
        case 47:
            return VK_OEM_PERIOD;
        
        // FIXME : 日本語キーボードサポート
        case 44:
            return VK_OEM_2; // slash
        case 41:
            return VK_OEM_1; // ;
        case 39:
            return VK_OEM_7; // '
        case 33:
            return VK_OEM_4; // [
        case 30:
            return VK_OEM_6; // ]
        case 42:
            return VK_OEM_5; // back-slash
        case 50:
            return VK_OEM_3; // `
    }
    
    return -1;
}

- (void)keyDown:(NSEvent *)theEvent
{
    TRACE;

    int keyCode = [theEvent keyCode];
    int vk = translateVk(keyCode);
    
    //printf("keyCode: %d\n", keyCode);
    //printf("vk: %d\n", vk);
    
    if(vk<0){return;}

    int mod = 0;
    if( theEvent.modifierFlags & NSShiftKeyMask ){ mod |= MODKEY_SHIFT; }
    if( theEvent.modifierFlags & NSCommandKeyMask ){ mod |= MODKEY_CTRL; }
    if( theEvent.modifierFlags & NSAlternateKeyMask ){ mod |= MODKEY_ALT; }
    if( theEvent.modifierFlags & NSControlKeyMask ){ mod |= MODKEY_WIN; }
    
    int consumed = callbacks->keyDown( owner, vk, mod );
    
    if(!consumed)
    {
        [self interpretKeyEvents: [NSArray arrayWithObject: theEvent]];
    }
}

- (void)keyUp:(NSEvent *)theEvent
{
    TRACE;

    int keyCode = [theEvent keyCode];
    int vk = translateVk(keyCode);
    
    if(vk<0){return;}

    int mod = 0;
    if( theEvent.modifierFlags & NSShiftKeyMask ){ mod |= MODKEY_SHIFT; }
    if( theEvent.modifierFlags & NSCommandKeyMask ){ mod |= MODKEY_CTRL; }
    if( theEvent.modifierFlags & NSAlternateKeyMask ){ mod |= MODKEY_ALT; }
    if( theEvent.modifierFlags & NSControlKeyMask ){ mod |= MODKEY_WIN; }
    
    callbacks->keyUp( owner, vk, mod );
}

- (void)insertText:(id)insertString
{
    TRACE;

    // FIXME : TABやEnterはここで処理されない。

    // FIXME : insertString が　AttributedStringである可能性もある
    NSString * s = insertString;
    
    // FIXME : modifierをちゃんとする
    callbacks->insertText( owner, (const wchar_t*)[s cStringUsingEncoding:NSUTF32LittleEndianStringEncoding], 0 );
}

- (void)deleteBackward:(id)sender
{
    TRACE;

    // FIXME : modifierをちゃんとする
    callbacks->insertText( owner, L"\b", 0 );
}

@end

int ckit_Application_Create()
{
    TRACE;
    
    [NSApplication sharedApplication];

    return 0;
}

int ckit_Window_Create( ckit_Window_Create_Parameters * params, CocoaObject ** _window )
{
    TRACE;

    int style = NSClosableWindowMask;
    if(params->titlebar){ style |= NSTitledWindowMask; }
    if(params->minimizable){ style |= NSMiniaturizableWindowMask; }
    if(params->resizable){ style |= NSResizableWindowMask; }
    
    NSWindow * window = [[NSWindow alloc]
                         initWithContentRect:NSMakeRect(0,0,100,100)
                         styleMask:style
                         backing:NSBackingStoreBuffered
                         defer:YES];
    
    CkitView * view = [[CkitView alloc] initWithFrame:NSMakeRect(0,0,1,1) callbacks:params->callbacks owner:params->owner ];
    
    window.delegate = view;
    
    [window setContentView:view];
    
    ckit_Window_SetTitle( (__bridge CocoaObject*)window, params->title );
    
    *_window = (__bridge_retained CocoaObject *)window;
    
    return 0;
}

int ckit_Window_Destroy( CocoaObject * _window )
{
    TRACE;
    
    // FIXME : ウインドウの破棄の方法として、これで合っているのか確認
    NSWindow * window = (__bridge_transfer NSWindow*)_window;
    (void)window;
    
    return 0;
}

int ckit_Window_MessageLoop( CocoaObject * _window )
{
    TRACE;
    
    NSWindow * window = (__bridge NSWindow*)_window;
    
    TRACE;

    [NSApp runModalForWindow:window];
    
    TRACE;

    return 0;
}

int ckit_Window_Quit( CocoaObject * _window )
{
    TRACE;
    
    NSWindow * window = (__bridge NSWindow*)_window;
    (void)window;
    
    [NSApp stopModal];
    
    return 0;
}

int ckit_Window_SetWindowRect( CocoaObject * _window, CGRect rect )
{
    TRACE;

    NSWindow * window = (__bridge NSWindow*)_window;

    [window setFrame:rect display:true];

    return 0;
}

int ckit_Window_GetWindowRect( CocoaObject * _window, CGRect * rect )
{
    TRACE;

    NSWindow * window = (__bridge NSWindow*)_window;
    
    *rect = [window frame];
    
    return 0;
}

int ckit_Window_GetClientSize( CocoaObject * _window, CGSize * size )
{
    TRACE;

    NSWindow * window = (__bridge NSWindow*)_window;
    
    NSView * view = window.contentView;
    CGRect rect = [view frame];
    
    *size = rect.size;
    
    return 0;
}

int ckit_Window_GetScreenSize( CocoaObject * _window, CGSize * size )
{
    TRACE;

    NSWindow * window = (__bridge NSWindow*)_window;
    
    NSScreen * screen = [window screen];
    
    CGRect rect = [screen frame];
    
    *size = rect.size;
    
    return 0;
}

int ckit_Window_IsVisible( CocoaObject * _window, int * visible )
{
    TRACE;
    
    NSWindow * window = (__bridge NSWindow*)_window;
    
    *visible = [window isVisible];
    
    return 0;
}

int ckit_Window_IsMaximized( CocoaObject * _window, int * maximized )
{
    TRACE;

    NSWindow * window = (__bridge NSWindow*)_window;
    
    *maximized = [window isZoomed];
    
    return 0;
}

int ckit_Window_IsMinimized( CocoaObject * _window, int * minimized )
{
    TRACE;

    NSWindow * window = (__bridge NSWindow*)_window;
    
    *minimized = [window isMiniaturized];
    
    return 0;
}

int ckit_Window_IsActive( CocoaObject * _window, int * active )
{
    TRACE;
    
    NSWindow * window = (__bridge NSWindow*)_window;
    
    //*active = [window isMainWindow];
    *active = [window isKeyWindow];
    
    return 0;
}

int ckit_Window_SetNeedsRedraw( CocoaObject * _window )
{
    //TRACE;

    NSWindow * window = (__bridge NSWindow*)_window;

    NSView * view = window.contentView;
    [view setNeedsDisplay:TRUE];
    
    return 0;
}

int ckit_Window_SetTimer( CocoaObject * _window, float interval, CocoaObject ** _timer )
{
    TRACE;

    NSWindow * window = (__bridge NSWindow*)_window;
    NSView * view = window.contentView;
    
    NSTimer * timer = [NSTimer scheduledTimerWithTimeInterval:interval
                                                       target:view
                                                     selector:@selector(timerHandler:)
                                                     userInfo:nil
                                                      repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSModalPanelRunLoopMode];

    *_timer = (__bridge CocoaObject *)timer;
    
    return 0;
}

int ckit_Window_KillTimer( CocoaObject * _window, CocoaObject * _timer )
{
    TRACE;

    NSTimer * timer = (__bridge NSTimer*)_timer;
    
    [timer invalidate];
    
    return 0;
}

int ckit_Window_ClientToScreen( CocoaObject * _window, CGPoint * point )
{
    TRACE;

    NSWindow * window = (__bridge NSWindow*)_window;
    NSView * view = window.contentView;

    CGPoint window_point = [view convertPoint:*point toView:nil];
    CGPoint screen_point = [window convertBaseToScreen:window_point];
    
    *point = screen_point;
    
    return 0;
}

int ckit_Window_ScreenToClient( CocoaObject * _window, CGPoint * point )
{
    TRACE;

    NSWindow * window = (__bridge NSWindow*)_window;
    NSView * view = window.contentView;
    
    CGPoint window_point = [window convertScreenToBase:*point];
    CGPoint client_point = [view convertPoint:window_point fromView:nil];
    
    *point = client_point;
    
    return 0;
}

int ckit_Window_SetTitle( CocoaObject * _window, const wchar_t * _title )
{
    TRACE;

    NSWindow * window = (__bridge NSWindow*)_window;
    
    NSString * title = [[NSString alloc] initWithBytes:_title length:wcslen(_title)*sizeof(wchar_t) encoding:NSUTF32LittleEndianStringEncoding];

    [window setTitle:title];
    
    return 0;
}

int ckit_Window_Show( CocoaObject * _window, bool show, bool activate )
{
    TRACE;
    
    NSWindow * window = (__bridge NSWindow*)_window;
    
    if(show)
    {
        [window makeKeyAndOrderFront:nil];
        
        if(activate)
        {
            [window makeKeyWindow];
            [window makeMainWindow];
        }
    }
    else
    {
        [window orderOut:nil];
    }
    
    
    return 0;
}

int ckit_Window_Activate( CocoaObject * _window )
{
    TRACE;
    
    NSWindow * window = (__bridge NSWindow*)_window;
    
    [window makeKeyWindow];
    [window makeMainWindow];
    
    return 0;
}

int ckit_Window_SetForeground( CocoaObject * _window )
{
    TRACE;
    
    [NSApp activateIgnoringOtherApps:YES];
    
    return 0;
}

