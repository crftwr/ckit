//
//  ckitcore_cocoa.m
//  ckitcore
//
//  Created by Shimomura Tomonori on 2014/07/05.
//  Copyright (c) 2014年 craftware. All rights reserved.
//

#import <wchar.h>

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <Events.h>

#import "ckitcore_cocoa.h"
#import "ckitcore_cocoa_export.h"

//-----------------------------------------------------------------------------

//#define PRINTF printf
#define PRINTF(...)

//#define TRACE printf("%s(%d) : %s\n",__FILE__,__LINE__,__FUNCTION__)
#define TRACE

//-----------------------------------------------------------------------------

typedef struct Globals_t
{
    int keyevent_removal_tag;
    int hotkey_next_id;
    ckit_Application_Callbacks * application_callbacks;

} Globals;

static Globals g;

//-----------------------------------------------------------------------------

static OSStatus hotKeyHandler(EventHandlerCallRef nextHandler, EventRef theEvent, void *userData);

//-----------------------------------------------------------------------------

@implementation CkitView

- (id)initWithFrame:(NSRect)frame callbacks:(ckit_Window_Callbacks*)_callbacks owner:(void*)_owner parent_window:(CocoaObject*)parent_window
{
    TRACE;

    self = [super initWithFrame:frame];
    
    if(self)
    {
        self->callbacks = _callbacks;
        self->owner = _owner;
        self->mouse_tracking_tag = 0;
        self->ime_enabled = NO;
        self->marked_text = [[NSMutableAttributedString alloc] init];
        
        self.parent_window = parent_window;
    }

    // IME状態変化の通知を受ける
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(keyboardInputSourceChanged:)
                                                 name:NSTextInputContextKeyboardSelectionDidChangeNotification
                                               object:nil];
    
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
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

    [self removeTrackingRect:mouse_tracking_tag];
    mouse_tracking_tag = [self addTrackingRect:rect owner:self userData:NULL assumeInside:NO];
    
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
    MODKEY_CMD   = 0x00000010,
    MODKEY_FN    = 0x00000020,
};

/*
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

static int translateVk_MacToWin(int src)
{
    switch(src)
    {
        case kVK_Escape:
            return VK_ESCAPE;
        case kVK_Delete:
            return VK_BACK;
        case kVK_Return:
            return VK_RETURN;
        case kVK_Tab:
            return VK_TAB;
        case kVK_Space:
            return VK_SPACE;
        case kVK_LeftArrow:
            return VK_LEFT;
        case kVK_RightArrow:
            return VK_RIGHT;
        case kVK_DownArrow:
            return VK_DOWN;
        case kVK_UpArrow:
            return VK_UP;

        case kVK_ANSI_A:
            return VK_A;
        case kVK_ANSI_S:
            return VK_S;
        case kVK_ANSI_D:
            return VK_D;
        case kVK_ANSI_F:
            return VK_F;
        case kVK_ANSI_G:
            return VK_G;
        case kVK_ANSI_H:
            return VK_H;
        case kVK_ANSI_J:
            return VK_J;
        case kVK_ANSI_K:
            return VK_K;
        case kVK_ANSI_L:
            return VK_L;
        case kVK_ANSI_Q:
            return VK_Q;
        case kVK_ANSI_W:
            return VK_W;
        case kVK_ANSI_E:
            return VK_E;
        case kVK_ANSI_R:
            return VK_R;
        case kVK_ANSI_T:
            return VK_T;
        case kVK_ANSI_Y:
            return VK_Y;
        case kVK_ANSI_U:
            return VK_U;
        case kVK_ANSI_I:
            return VK_I;
        case kVK_ANSI_O:
            return VK_O;
        case kVK_ANSI_P:
            return VK_P;
        case kVK_ANSI_Z:
            return VK_Z;
        case kVK_ANSI_X:
            return VK_X;
        case kVK_ANSI_C:
            return VK_C;
        case kVK_ANSI_V:
            return VK_V;
        case kVK_ANSI_B:
            return VK_B;
        case kVK_ANSI_N:
            return VK_N;
        case kVK_ANSI_M:
            return VK_M;
        case kVK_ANSI_1:
            return VK_1;
        case kVK_ANSI_2:
            return VK_2;
        case kVK_ANSI_3:
            return VK_3;
        case kVK_ANSI_4:
            return VK_4;
        case kVK_ANSI_5:
            return VK_5;
        case kVK_ANSI_6:
            return VK_6;
        case kVK_ANSI_7:
            return VK_7;
        case kVK_ANSI_8:
            return VK_8;
        case kVK_ANSI_9:
            return VK_9;
        case kVK_ANSI_0:
            return VK_0;

        case kVK_ANSI_Minus:
            return VK_OEM_MINUS;
        case kVK_ANSI_Equal:
            return VK_OEM_PLUS;
        case kVK_ANSI_Comma:
            return VK_OEM_COMMA;
        case kVK_ANSI_Period:
            return VK_OEM_PERIOD;
        
        // FIXME : 日本語キーボードサポート
        case kVK_ANSI_Slash:
            return VK_OEM_2; // slash
        case kVK_ANSI_Semicolon:
            return VK_OEM_1; // ;
        case kVK_ANSI_Quote:
            return VK_OEM_7; // '
        case kVK_ANSI_LeftBracket:
            return VK_OEM_4; // [
        case kVK_ANSI_RightBracket:
            return VK_OEM_6; // ]
        case kVK_ANSI_Backslash:
            return VK_OEM_5; // back-slash
        case kVK_ANSI_Grave:
            return VK_OEM_3; // `
    }
    
    return -1;
}

static int translateVk_WinToMac(int src)
{
    switch(src)
    {
        case VK_ESCAPE:
            return kVK_Escape;
        case VK_BACK:
            return kVK_Delete;
        case VK_RETURN:
            return kVK_Return;
        case VK_TAB:
            return kVK_Tab;
        case VK_SPACE:
            return kVK_Space;
        case VK_LEFT:
            return kVK_LeftArrow;
        case VK_RIGHT:
            return kVK_RightArrow;
        case VK_DOWN:
            return kVK_DownArrow;
        case VK_UP:
            return kVK_UpArrow;
            
        case VK_A:
            return kVK_ANSI_A;
        case VK_S:
            return kVK_ANSI_S;
        case VK_D:
            return kVK_ANSI_D;
        case VK_F:
            return kVK_ANSI_F;
        case VK_G:
            return kVK_ANSI_G;
        case VK_H:
            return kVK_ANSI_H;
        case VK_J:
            return kVK_ANSI_J;
        case VK_K:
            return kVK_ANSI_K;
        case VK_L:
            return kVK_ANSI_L;
        case VK_Q:
            return kVK_ANSI_Q;
        case VK_W:
            return kVK_ANSI_W;
        case VK_E:
            return kVK_ANSI_E;
        case VK_R:
            return kVK_ANSI_R;
        case VK_T:
            return kVK_ANSI_T;
        case VK_Y:
            return kVK_ANSI_Y;
        case VK_U:
            return kVK_ANSI_U;
        case VK_I:
            return kVK_ANSI_I;
        case VK_O:
            return kVK_ANSI_O;
        case VK_P:
            return kVK_ANSI_P;
        case VK_Z:
            return kVK_ANSI_Z;
        case VK_X:
            return kVK_ANSI_X;
        case VK_C:
            return kVK_ANSI_C;
        case VK_V:
            return kVK_ANSI_V;
        case VK_B:
            return kVK_ANSI_B;
        case VK_N:
            return kVK_ANSI_N;
        case VK_M:
            return kVK_ANSI_M;
        case VK_1:
            return kVK_ANSI_1;
        case VK_2:
            return kVK_ANSI_2;
        case VK_3:
            return kVK_ANSI_3;
        case VK_4:
            return kVK_ANSI_4;
        case VK_5:
            return kVK_ANSI_5;
        case VK_6:
            return kVK_ANSI_6;
        case VK_7:
            return kVK_ANSI_7;
        case VK_8:
            return kVK_ANSI_8;
        case VK_9:
            return kVK_ANSI_9;
        case VK_0:
            return kVK_ANSI_0;
            
        case VK_OEM_MINUS:
            return kVK_ANSI_Minus;
        case VK_OEM_PLUS:
            return kVK_ANSI_Equal;
        case VK_OEM_COMMA:
            return kVK_ANSI_Comma;
        case VK_OEM_PERIOD:
            return kVK_ANSI_Period;
            
            // FIXME : 日本語キーボードサポート
        case VK_OEM_2:
            return kVK_ANSI_Slash; // slash
        case VK_OEM_1:
            return kVK_ANSI_Semicolon; // ;
        case VK_OEM_7:
            return kVK_ANSI_Quote; // '
        case VK_OEM_4:
            return kVK_ANSI_LeftBracket; // [
        case VK_OEM_6:
            return kVK_ANSI_RightBracket; // ]
        case VK_OEM_5:
            return kVK_ANSI_Backslash; // back-slash
        case VK_OEM_3:
            return kVK_ANSI_Grave; // `
    }
    
    return -1;
}
*/

- (void)keyDown:(NSEvent *)theEvent
{
    TRACE;

    int tag = g.keyevent_removal_tag;
    
    if( ! [self hasMarkedText] )
    {
        int keyCode = [theEvent keyCode];
        
        int mod = 0;
        if( theEvent.modifierFlags & NSAlternateKeyMask ){ mod |= MODKEY_ALT; }
        if( theEvent.modifierFlags & NSControlKeyMask ){ mod |= MODKEY_CTRL; }
        if( theEvent.modifierFlags & NSShiftKeyMask ){ mod |= MODKEY_SHIFT; }
        if( theEvent.modifierFlags & NSCommandKeyMask ){ mod |= MODKEY_CMD; }
        if( theEvent.modifierFlags & NSFunctionKeyMask ){ mod |= MODKEY_FN; }
        
        callbacks->keyDown( owner, keyCode, mod );
    }

    // messageLoopやremoveKeyMessageが呼ばれたら、それ以前の keyDown については interpretKeyEvents を呼ばない
    if( tag != g.keyevent_removal_tag )
    {
        return;
    }
    
    if( self->ime_enabled )
    {
        [[self inputContext] handleEvent:theEvent];
    }
    else
    {
        NSString * s = nil;
        
        if(theEvent.keyCode==51)
        {
            s = @"\b";
        }
        else
        {
            s = theEvent.characters;
        }

        PRINTF( "keyDown: [%s]\n", [theEvent.characters cStringUsingEncoding:NSUTF8StringEncoding] );
        
        callbacks->insertText( owner, (const wchar_t*)[s cStringUsingEncoding:NSUTF32LittleEndianStringEncoding], 0 );
    }
}

- (void)keyUp:(NSEvent *)theEvent
{
    TRACE;

    int keyCode = [theEvent keyCode];

    int mod = 0;
    if( theEvent.modifierFlags & NSAlternateKeyMask ){ mod |= MODKEY_ALT; }
    if( theEvent.modifierFlags & NSControlKeyMask ){ mod |= MODKEY_CTRL; }
    if( theEvent.modifierFlags & NSShiftKeyMask ){ mod |= MODKEY_SHIFT; }
    if( theEvent.modifierFlags & NSCommandKeyMask ){ mod |= MODKEY_CMD; }
    if( theEvent.modifierFlags & NSFunctionKeyMask ){ mod |= MODKEY_FN; }
    
    callbacks->keyUp( owner, keyCode, mod );
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

- (void)viewDidMoveToWindow
{
    TRACE;
    
    NSRect rect = [self frame];
    mouse_tracking_tag = [self addTrackingRect:rect owner:self userData:NULL assumeInside:NO];
}

- (void)mouseEntered:(NSEvent *)theEvent
{
    [[self window] setAcceptsMouseMovedEvents:YES];
    [[self window] makeFirstResponder:self];
}

- (void)mouseExited:(NSEvent *)theEvent
{
    [[self window] setAcceptsMouseMovedEvents:NO];
}

- (void)mouseDown:(NSEvent *)theEvent
{
    TRACE;
    
    ckit_MouseEvent mouse_event;
    memset( &mouse_event, 0, sizeof(mouse_event) );

    switch( theEvent.clickCount )
    {
    case 1:
        mouse_event.type = ckit_MouseEventType_LeftDown;
        break;
    case 2:
        mouse_event.type = ckit_MouseEventType_LeftDoubleClick;
        break;
    default:
        return;
    }
    
    mouse_event.location = theEvent.locationInWindow;
    
    callbacks->mouse( owner, &mouse_event );
}

- (void)mouseUp:(NSEvent *)theEvent
{
    TRACE;
    
    ckit_MouseEvent mouse_event;
    memset( &mouse_event, 0, sizeof(mouse_event) );
    
    mouse_event.type = ckit_MouseEventType_LeftUp;
    mouse_event.location = theEvent.locationInWindow;
    
    callbacks->mouse( owner, &mouse_event );
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    TRACE;
    
    ckit_MouseEvent mouse_event;
    memset( &mouse_event, 0, sizeof(mouse_event) );
    
    switch( theEvent.clickCount )
    {
    case 1:
        mouse_event.type = ckit_MouseEventType_RightDown;
        break;
    case 2:
        mouse_event.type = ckit_MouseEventType_RightDoubleClick;
        break;
    default:
        return;
    }
    
    mouse_event.location = theEvent.locationInWindow;
    
    callbacks->mouse( owner, &mouse_event );
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
    TRACE;
    
    ckit_MouseEvent mouse_event;
    memset( &mouse_event, 0, sizeof(mouse_event) );
    
    mouse_event.type = ckit_MouseEventType_RightUp;
    mouse_event.location = theEvent.locationInWindow;
    
    callbacks->mouse( owner, &mouse_event );
}

- (void)otherMouseDown:(NSEvent *)theEvent
{
    TRACE;
    
    if(theEvent.buttonNumber!=2){ return; }
    
    ckit_MouseEvent mouse_event;
    memset( &mouse_event, 0, sizeof(mouse_event) );
    
    switch( theEvent.clickCount )
    {
    case 1:
        mouse_event.type = ckit_MouseEventType_MiddleDown;
        break;
    case 2:
        mouse_event.type = ckit_MouseEventType_MiddleDoubleClick;
        break;
    default:
        return;
    }
    
    mouse_event.location = theEvent.locationInWindow;
    
    callbacks->mouse( owner, &mouse_event );
}

- (void)otherMouseUp:(NSEvent *)theEvent
{
    TRACE;
    
    if(theEvent.buttonNumber!=2){ return; }
    
    ckit_MouseEvent mouse_event;
    memset( &mouse_event, 0, sizeof(mouse_event) );
    
    mouse_event.type = ckit_MouseEventType_MiddleUp;
    mouse_event.location = theEvent.locationInWindow;
    
    callbacks->mouse( owner, &mouse_event );
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    TRACE;
    
    ckit_MouseEvent mouse_event;
    memset( &mouse_event, 0, sizeof(mouse_event) );
    
    mouse_event.type = ckit_MouseEventType_Move;
    mouse_event.location = theEvent.locationInWindow;
    
    callbacks->mouse( owner, &mouse_event );
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    TRACE;
    
    ckit_MouseEvent mouse_event;
    memset( &mouse_event, 0, sizeof(mouse_event) );
    
    mouse_event.type = ckit_MouseEventType_Move;
    mouse_event.location = theEvent.locationInWindow;
    
    callbacks->mouse( owner, &mouse_event );
}

- (void)rightMouseDragged:(NSEvent *)theEvent
{
    TRACE;
    
    ckit_MouseEvent mouse_event;
    memset( &mouse_event, 0, sizeof(mouse_event) );
    
    mouse_event.type = ckit_MouseEventType_Move;
    mouse_event.location = theEvent.locationInWindow;
    
    callbacks->mouse( owner, &mouse_event );
}

- (void)otherMouseDragged:(NSEvent *)theEvent
{
    TRACE;
    
    ckit_MouseEvent mouse_event;
    memset( &mouse_event, 0, sizeof(mouse_event) );
    
    mouse_event.type = ckit_MouseEventType_Move;
    mouse_event.location = theEvent.locationInWindow;
    
    callbacks->mouse( owner, &mouse_event );
}

- (void)scrollWheel:(NSEvent *)theEvent
{
    TRACE;
    
    ckit_MouseEvent mouse_event;
    memset( &mouse_event, 0, sizeof(mouse_event) );
    
    mouse_event.type = ckit_MouseEventType_Wheel;
    mouse_event.location = theEvent.locationInWindow;
    mouse_event.delta_x = theEvent.deltaX;
    mouse_event.delta_y = theEvent.deltaY;
    
    callbacks->mouse( owner, &mouse_event );
}

- (void)touchesBeganWithEvent:(NSEvent *)event
{
    TRACE;
}

- (void)touchesMovedWithEvent:(NSEvent *)event
{
    TRACE;
}

- (void)touchesEndedWithEvent:(NSEvent *)event
{
    TRACE;
}

- (void)touchesCancelledWithEvent:(NSEvent *)event
{
    TRACE;
}

- (void)insertText:(id)aString replacementRange:(NSRange)replacementRange
{
    TRACE;
    
    if( replacementRange.location == NSNotFound )
    {
        replacementRange = NSMakeRange( 0, [self->marked_text length] );
    }
    
    if( [aString isKindOfClass:[NSAttributedString class]] )
    {
        printf( "insertText: NSAttributedString\n" );
    }

    // FIXME : insertString が　AttributedStringである可能性もある
    NSString * s = aString;

    PRINTF( "insertText: [%s], replace=(%d,%d)\n", [s cStringUsingEncoding:NSUTF8StringEncoding], (int)replacementRange.location, (int)replacementRange.length );
    
    [self->marked_text replaceCharactersInRange:replacementRange withString:@""];

    // FIXME : modifierをちゃんとする
    callbacks->insertText( owner, (const wchar_t*)[s cStringUsingEncoding:NSUTF32LittleEndianStringEncoding], 0 );
}

- (void)setMarkedText:(id)aString selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange
{
    TRACE;
    
    if( replacementRange.location == NSNotFound )
    {
        replacementRange = NSMakeRange( 0, [self->marked_text length] );
    }

    if( [aString isKindOfClass:[NSAttributedString class]] )
    {
        /*
        NSAttributedString * attributed_string = aString;
        NSString * s = attributed_string.string;
        
        PRINTF( "setMarkedText: [%s], selected=(%d,%d) replace=(%d,%d)\n", [s cStringUsingEncoding:NSUTF8StringEncoding], (int)selectedRange.location, (int)selectedRange.length, (int)replacementRange.location, (int)replacementRange.length );
        */
        
        [self->marked_text replaceCharactersInRange:replacementRange withAttributedString:aString];
    }
    else
    {
        /*
        NSString * s = aString;
        
        PRINTF( "setMarkedText: [%s], selected=(%d,%d) replace=(%d,%d)\n", [s cStringUsingEncoding:NSUTF8StringEncoding], (int)selectedRange.location, (int)selectedRange.length, (int)replacementRange.location, (int)replacementRange.length );
        */
        
        [self->marked_text replaceCharactersInRange:replacementRange withString:aString];
    }

    [self setNeedsDisplay:TRUE];
}

- (void)unmarkText
{
    TRACE;
}

- (NSRange)selectedRange
{
    TRACE;
    
    return NSMakeRange(0,0);
}

- (NSRange)markedRange
{
    TRACE;

    return NSMakeRange( 0, [self->marked_text length] );
}

- (BOOL)hasMarkedText
{
    TRACE;
    
    return [self->marked_text length] > 0;
}

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)aRange actualRange:(NSRangePointer)actualRange
{
    TRACE;

    // We choose not to adjust the range, though we have the option
    if (actualRange)
    {
        *actualRange = aRange;
    }

    return [self->marked_text attributedSubstringFromRange:aRange];
}

- (NSArray *)validAttributesForMarkedText
{
    TRACE;
    
    return [NSArray arrayWithObjects:NSMarkedClauseSegmentAttributeName, NSGlyphInfoAttributeName, nil];
}

- (NSRect)firstRectForCharacterRange:(NSRange)aRange actualRange:(NSRangePointer)actualRange
{
    TRACE;

    PRINTF( "firstRectForCharacterRange: range=(%d,%d)\n", (int)aRange.location, (int)aRange.length );

    CGRect caret_rect;
    callbacks->imePosition( owner, &caret_rect );
    
    NSWindow * window = [self window];
    
    caret_rect.origin = [self convertPoint:caret_rect.origin toView:nil];
    caret_rect.origin = [window convertBaseToScreen:caret_rect.origin];

    return caret_rect;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)aPoint
{
    TRACE;
    
    return 0;
}

- (void)doCommandBySelector:(SEL)aSelector
{
    // 親クラスの doCommandBySelector を呼ばない。
    // 呼ぶと Beep が鳴ってしまう。
}

- (void)keyboardInputSourceChanged:(NSNotification *)notification
{
    [self setNeedsDisplay:TRUE];
}

@end

int ckit_Application_Create( ckit_Application_Create_Parameters * params )
{
    TRACE;
    
    // シングルトンを初期化
    [NSApplication sharedApplication];

    // アプリを前面に
    [NSApp activateIgnoringOtherApps:YES];
    
    g.application_callbacks = params->callbacks;

    // ホットキー
    {
        EventTypeSpec eventTypeSpecList[] =
        {
            { kEventClassKeyboard, kEventHotKeyPressed }
        };
        
        InstallApplicationEventHandler(&hotKeyHandler, GetEventTypeCount(eventTypeSpecList), eventTypeSpecList, NULL, NULL);
    }
    
    return 0;
}

int ckit_Application_SetHotKey( int vk, int mod, CocoaObject ** _handle, int * _id )
{
    EventHotKeyRef hotKeyRef;
    
    EventHotKeyID hotKeyID;
    hotKeyID.id = g.hotkey_next_id;
    hotKeyID.signature = 'htky';
    
    UInt32 hotKeyModifier = 0;
    if(mod & MODKEY_ALT){ hotKeyModifier |= optionKey; }
    if(mod & MODKEY_CTRL){ hotKeyModifier |= controlKey; }
    if(mod & MODKEY_SHIFT){ hotKeyModifier |= shiftKey; }
    if(mod & MODKEY_CMD){ hotKeyModifier |= cmdKey; }

    OSStatus status = RegisterEventHotKey(vk, hotKeyModifier, hotKeyID, GetApplicationEventTarget(), 0, &hotKeyRef);
    (void)status;
    
    *_handle = (CocoaObject*)hotKeyRef;
    *_id = g.hotkey_next_id;
    
    g.hotkey_next_id++;
    
    return 0;
}

int ckit_Application_KillHotKey( CocoaObject * _handle )
{
    EventHotKeyRef hotKeyRef = (EventHotKeyRef)_handle;
    
    OSStatus status = UnregisterEventHotKey(hotKeyRef);
    (void)status;
    
    return 0;
}

int ckit_Window_Create( ckit_Window_Create_Parameters * params, CocoaObject ** _window )
{
    TRACE;

    int style = 0;
    
    if(params->titlebar) { style |= NSTitledWindowMask; }
    if(params->minimizable){ style |= NSMiniaturizableWindowMask; }
    if(params->resizable){ style |= NSResizableWindowMask; }
    if( params->minimizable || params->resizable ){ style |= NSClosableWindowMask; }
    
    NSWindow * window = [[NSWindow alloc]
                         initWithContentRect:NSMakeRect(0,0,100,100)
                         styleMask:style
                         backing:NSBackingStoreBuffered
                         defer:YES];

    // NSWindowオブジェクトの解放は Close 時ではなく ARC を使って制御する
    [window setReleasedWhenClosed:FALSE];
    
    CkitView * view = [[CkitView alloc] initWithFrame:NSMakeRect(0,0,1,1) callbacks:params->callbacks owner:params->owner parent_window:params->parent_window ];
    
    window.delegate = view;
    [window setContentView:view];
    
    ckit_Window_SetTitle( (__bridge CocoaObject*)window, params->title );
    
    *_window = (__bridge_retained CocoaObject *)window;
    
    return 0;
}

int ckit_Window_Destroy( CocoaObject * _window )
{
    TRACE;
    
    NSWindow * window = (__bridge_transfer NSWindow*)_window;

    // ウインドウ削除前に親子関係を切る
    {
        CkitView * view = window.contentView;
        NSWindow * parent_window = (__bridge NSWindow*)view.parent_window;
        if(parent_window)
        {
            [parent_window removeChildWindow:window];
        }
    }
    
    [window close];
    
    return 0;
}

int ckit_Window_MessageLoop( CocoaObject * _window )
{
    TRACE;
    
    NSWindow * window = (__bridge NSWindow*)_window;
    
    TRACE;

    // キーイベントのタグを更新し、文字入力イベントをキャンセル
    g.keyevent_removal_tag ++;
    
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

int ckit_Window_RemoveKeyMessage( CocoaObject * _window )
{
    TRACE;
    
    // キーイベントのタグを更新し、文字入力イベントをキャンセル
    g.keyevent_removal_tag ++;
    
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

static OSStatus hotKeyHandler(EventHandlerCallRef nextHandler, EventRef theEvent, void *userData)
{
    TRACE;
    
    EventHotKeyID hotKeyID;
    GetEventParameter(theEvent, kEventParamDirectObject, typeEventHotKeyID, NULL, sizeof(hotKeyID), NULL, &hotKeyID);
    
    if (hotKeyID.signature == 'htky')
    {
        g.application_callbacks->hotKey( hotKeyID.id );
    }
    
    return noErr;
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
            // FIXME : タイトルバーがないと、makeMainWindow がエラーになる

            [window makeKeyWindow];
            [window makeMainWindow];
        }

        // Show/Hideのときに親子関係を切らないと、なぜか親ウインドウが巻き添えになってしまう
        {
            CkitView * view = window.contentView;
            NSWindow * parent_window = (__bridge NSWindow*)view.parent_window;
            [parent_window addChildWindow:window ordered:NSWindowAbove];
        }
    }
    else
    {
        // Show/Hideのときに親子関係を切らないと、なぜか親ウインドウが巻き添えになってしまう
        {
            CkitView * view = window.contentView;
            NSWindow * parent_window = (__bridge NSWindow*)view.parent_window;
            [parent_window removeChildWindow:window];
        }

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

int ckit_Window_GetMarkedText( CocoaObject * _window, CFMutableAttributedStringRef * _marked_text )
{
    TRACE;

    NSWindow * window = (__bridge NSWindow*)_window;
    CkitView * view = window.contentView;
    
    *_marked_text = (__bridge CFMutableAttributedStringRef)view->marked_text;

    return 0;
}

int ckit_Window_EnableIme( CocoaObject * _window, int enable )
{
    NSWindow * window = (__bridge NSWindow*)_window;
    CkitView * view = window.contentView;

    if(enable)
    {
        view->ime_enabled = YES;
    }
    else
    {
        view->ime_enabled = NO;
    }
    
    return 0;
}

int ckit_Window_IsImeOpened( CocoaObject * _window, int * ime_opened )
{
    NSWindow * window = (__bridge NSWindow*)_window;
    CkitView * view = window.contentView;

    NSString * input_source = [view inputContext].selectedKeyboardInputSource;
    
    const char * s = [input_source cStringUsingEncoding:NSUTF8StringEncoding];
    size_t len = strlen(s);

    if( strcmp( s + len - 6, ".Roman" )==0 )
    {
        *ime_opened = 0;
    }
    else
    {
        *ime_opened = 1;
    }
    
    return 0;
}

int ckit_Global_SetClipboardText( const wchar_t * _text )
{
    NSPasteboard * pb = [NSPasteboard generalPasteboard];

    NSString * text = [[NSString alloc] initWithBytes:_text length:wcslen(_text)*sizeof(wchar_t) encoding:NSUTF32LittleEndianStringEncoding];
    
    [pb clearContents];
    [pb writeObjects:[NSArray arrayWithObject:text]];

    return 0;
}

int ckit_Global_GetClipboardText( wchar_t ** text )
{
    NSPasteboard * pb = [NSPasteboard generalPasteboard];
    
    NSArray * classes = [NSArray arrayWithObject:[NSString class]];
    NSArray * objects = [pb readObjectsForClasses:classes options:nil];
    
    if([objects count]>0)
    {
        NSString * value = [objects objectAtIndex:0];
        
        const wchar_t * _text = (const wchar_t*)[value cStringUsingEncoding:NSUTF32LittleEndianStringEncoding];
        
        *text = (wchar_t*)malloc( (wcslen(_text)+1) * sizeof(wchar_t) );
        
        wcscpy( *text, _text );
    }

    return 0;
}

int ckit_Global_GetClipboardChangeCount( int * change_count )
{
    NSPasteboard * pb = [NSPasteboard generalPasteboard];
    
    *change_count = (int)pb.changeCount;

    return 0;
}

int ckit_Global_GetFocusedApplicationId( wchar_t ** id )
{
    NSWorkspace * ws = [NSWorkspace sharedWorkspace];
    NSRunningApplication * app = [ws frontmostApplication];
    NSString * bundleId = [app bundleIdentifier];

    const wchar_t * _id = (const wchar_t*)[bundleId cStringUsingEncoding:NSUTF32LittleEndianStringEncoding];
    
    *id = (wchar_t*)malloc( (wcslen(_id)+1) * sizeof(wchar_t) );
    
    wcscpy( *id, _id );
    
    return 0;
}

int ckit_Global_Beep()
{
    NSBeep();
    
    return 0;
}

int ckit_Global_Test()
{
    NSWorkspace * ws = [NSWorkspace sharedWorkspace];
    NSRunningApplication * app = [ws frontmostApplication];
    NSString * bundleId = [app bundleIdentifier];
    
    printf( "bundleId : %s\n", [bundleId cStringUsingEncoding:NSUTF8StringEncoding] );
    
    return 0;
}










