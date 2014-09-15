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

    pid_t focused_pid;
    AXObserverRef ax_observer;
    bool need_reset_ax_observer;
    int focus_change_count;

} Globals;

static Globals g;

//-----------------------------------------------------------------------------

static OSStatus hotKeyHandler(EventHandlerCallRef nextHandler, EventRef theEvent, void *userData);

//-----------------------------------------------------------------------------

static wchar_t * NSStringToMallocedWchar( NSString * src )
{
    NSData * data = [src dataUsingEncoding:NSUTF32LittleEndianStringEncoding allowLossyConversion:YES];
    
    unsigned long char_len = [src length];
    unsigned long byte_len = [data length];
    
    wchar_t * dst = (wchar_t*)malloc( byte_len + sizeof(wchar_t) );
    
    memcpy( dst, [data bytes], byte_len );
    dst[char_len] = 0;
    
    return dst;
}

static NSString * WcharToNSString( const wchar_t * src )
{
    NSString * dst = [[NSString alloc] initWithBytes:src length:wcslen(src)*sizeof(wchar_t) encoding:NSUTF32LittleEndianStringEncoding];

    return dst;
}

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

    if( callbacks && owner )
    {
        callbacks->windowShouldClose( owner );
    }
    
    return FALSE;
}

- (void)drawRect:(NSRect)dirtyRect
{
    TRACE;
    
    [super drawRect:dirtyRect];
    
    // Drawing code here.
    
    CGContextRef gctx = [[NSGraphicsContext currentContext] graphicsPort];
    
    if( callbacks && owner )
    {
        callbacks->drawRect( owner, dirtyRect, gctx );
    }
}

- (void)windowDidResize:(NSNotification *)notification
{
    TRACE;
    
    CGRect rect = [self frame];

    [self removeTrackingRect:mouse_tracking_tag];
    mouse_tracking_tag = [self addTrackingRect:rect owner:self userData:NULL assumeInside:NO];
    
    if( callbacks && owner )
    {
        callbacks->windowDidResize( owner, rect.size );
    }
}

- (void)windowDidEndLiveResize:(NSNotification *)notification
{
    TRACE;

    CGRect rect = [self frame];
    
    if( callbacks && owner )
    {
        callbacks->windowDidResize( owner, rect.size );
    }
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize
{
    TRACE;

    if( callbacks && owner )
    {
        callbacks->windowWillResize( owner, &frameSize );
    }
    
    return frameSize;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    if( callbacks && owner )
    {
        callbacks->windowDidBecomeKey(owner);
    }
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    if( callbacks && owner )
    {
        callbacks->windowDidResignKey(owner);
    }
}

-(void)timerHandler:(NSTimer*)timer
{
    if( callbacks && owner )
    {
        callbacks->timerHandler( owner, (__bridge CocoaObject *)(timer) );
    }
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
};

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
        
        if( callbacks && owner )
        {
            callbacks->keyDown( owner, keyCode, mod );
        }
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
        
        wchar_t * p = NSStringToMallocedWchar(s);
        
        if( callbacks && owner )
        {
            callbacks->insertText( owner, p, 0 );
        }
        
        free(p);
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
    
    if( callbacks && owner )
    {
        callbacks->keyUp( owner, keyCode, mod );
    }
}

- (void)insertText:(id)insertString
{
    TRACE;

    // FIXME : TABやEnterはここで処理されない。

    // FIXME : insertString が　AttributedStringである可能性もある
    NSString * s = insertString;
    
    wchar_t * p = NSStringToMallocedWchar(s);

    if( callbacks && owner )
    {
        // FIXME : modifierをちゃんとする
        callbacks->insertText( owner, p, 0 );
    }
    
    free(p);
}

- (void)deleteBackward:(id)sender
{
    TRACE;

    if( callbacks && owner )
    {
        // FIXME : modifierをちゃんとする
        callbacks->insertText( owner, L"\b", 0 );
    }
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
    
    if( callbacks && owner )
    {
        callbacks->mouse( owner, &mouse_event );
    }
}

- (void)mouseUp:(NSEvent *)theEvent
{
    TRACE;
    
    ckit_MouseEvent mouse_event;
    memset( &mouse_event, 0, sizeof(mouse_event) );
    
    mouse_event.type = ckit_MouseEventType_LeftUp;
    mouse_event.location = theEvent.locationInWindow;
    
    if( callbacks && owner )
    {
        callbacks->mouse( owner, &mouse_event );
    }
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
    
    if( callbacks && owner )
    {
        callbacks->mouse( owner, &mouse_event );
    }
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
    TRACE;
    
    ckit_MouseEvent mouse_event;
    memset( &mouse_event, 0, sizeof(mouse_event) );
    
    mouse_event.type = ckit_MouseEventType_RightUp;
    mouse_event.location = theEvent.locationInWindow;
    
    if( callbacks && owner )
    {
        callbacks->mouse( owner, &mouse_event );
    }
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
    
    if( callbacks && owner )
    {
        callbacks->mouse( owner, &mouse_event );
    }
}

- (void)otherMouseUp:(NSEvent *)theEvent
{
    TRACE;
    
    if(theEvent.buttonNumber!=2){ return; }
    
    ckit_MouseEvent mouse_event;
    memset( &mouse_event, 0, sizeof(mouse_event) );
    
    mouse_event.type = ckit_MouseEventType_MiddleUp;
    mouse_event.location = theEvent.locationInWindow;
    
    if( callbacks && owner )
    {
        callbacks->mouse( owner, &mouse_event );
    }
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    TRACE;
    
    ckit_MouseEvent mouse_event;
    memset( &mouse_event, 0, sizeof(mouse_event) );
    
    mouse_event.type = ckit_MouseEventType_Move;
    mouse_event.location = theEvent.locationInWindow;
    
    if( callbacks && owner )
    {
        callbacks->mouse( owner, &mouse_event );
    }
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    TRACE;
    
    ckit_MouseEvent mouse_event;
    memset( &mouse_event, 0, sizeof(mouse_event) );
    
    mouse_event.type = ckit_MouseEventType_Move;
    mouse_event.location = theEvent.locationInWindow;
    
    if( callbacks && owner )
    {
        callbacks->mouse( owner, &mouse_event );
    }
}

- (void)rightMouseDragged:(NSEvent *)theEvent
{
    TRACE;
    
    ckit_MouseEvent mouse_event;
    memset( &mouse_event, 0, sizeof(mouse_event) );
    
    mouse_event.type = ckit_MouseEventType_Move;
    mouse_event.location = theEvent.locationInWindow;
    
    if( callbacks && owner )
    {
        callbacks->mouse( owner, &mouse_event );
    }
}

- (void)otherMouseDragged:(NSEvent *)theEvent
{
    TRACE;
    
    ckit_MouseEvent mouse_event;
    memset( &mouse_event, 0, sizeof(mouse_event) );
    
    mouse_event.type = ckit_MouseEventType_Move;
    mouse_event.location = theEvent.locationInWindow;
    
    if( callbacks && owner )
    {
        callbacks->mouse( owner, &mouse_event );
    }
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
    
    if( callbacks && owner )
    {
        callbacks->mouse( owner, &mouse_event );
    }
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

    wchar_t * p = NSStringToMallocedWchar(s);

    if( callbacks && owner )
    {
        // FIXME : modifierをちゃんとする
        callbacks->insertText( owner, p, 0 );
    }
    
    free(p);
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

    if( callbacks && owner )
    {
        callbacks->imePosition( owner, &caret_rect );
    }
    
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
    CkitView * view = window.contentView;
    
    // C++側のオブジェクトとの関連をきる
    view->owner = NULL;
    view->callbacks = NULL;

    // ウインドウ削除前に親子関係を切る
    NSWindow * parent_window = (__bridge NSWindow*)view.parent_window;
    if(parent_window)
    {
        [parent_window removeChildWindow:window];
    }
    
    [window close];
    
    return 0;
}

int ckit_Window_MessageLoop( CocoaObject * _window, ckit_Window_MessageLoopCallback callback, void * py_func )
{
    TRACE;
    
    NSWindow * window = (__bridge NSWindow*)_window;
    CkitView * view = window.contentView;
    
    TRACE;

    // キーイベントのタグを更新し、文字入力イベントをキャンセル
    g.keyevent_removal_tag ++;
    
    if(0)
    {
        [NSApp runModalForWindow:window];
    }
    else
    {
        while(1)
        {
            int result = callback( view->owner, py_func );
            if(!result)
            {
                break;
            }
            
            NSEvent * event = [NSApp nextEventMatchingMask:NSAnyEventMask
                                                 untilDate:[NSDate distantPast]
                                                    inMode:NSDefaultRunLoopMode
                                                   dequeue:YES];
            if(event)
            {
                [NSApp sendEvent:event];
            }
            else
            {
                // FIXME : もっとエレガントなイベントの待ち方はないのか
                usleep(1000*10);
            }
        }
    }
    
    TRACE;

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
        if( g.application_callbacks )
        {
            g.application_callbacks->hotKey( hotKeyID.id );
        }
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
    
    NSString * title = WcharToNSString(_title);

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

int ckit_TaskTrayIcon_Create( ckit_TaskTrayIcon_Create_Parameters * params, CocoaObject ** _task_tray_icon )
{
    NSStatusItem * statusItem;

    NSStatusBar * systemStatusBar = [NSStatusBar systemStatusBar];

    statusItem = [systemStatusBar statusItemWithLength:NSVariableStatusItemLength];
    
    [statusItem setHighlightMode:YES];

    NSString * title = WcharToNSString(params->title);
    [statusItem setTitle:title];

    //[statusItem setImage:[NSImage imageNamed:@"AppIcon"]];
    //[statusItem setMenu:self.statusMenu];
    
    *_task_tray_icon = (__bridge_retained CocoaObject*)statusItem;

    return 0;
}

int ckit_TaskTrayIcon_Destroy( CocoaObject * _task_tray_icon )
{
    NSStatusItem * statusItem = (__bridge_transfer NSStatusItem*)_task_tray_icon;
    (void)statusItem;
    
    return 0;
}

int ckit_Global_GetMonitorInfo( ckit_MonitorInfo monitor_info[], int * num_info )
{
    NSArray * screenArray = [NSScreen screens];
    unsigned long screenCount = [screenArray count];
    
    for( int i=0 ; i<screenCount && i<*num_info ; ++i )
    {
        NSScreen * screen = [screenArray objectAtIndex: i];

        /*
        NSRect frame = [screen frame];
        NSRect visibleFrame = [screen visibleFrame];
        printf( "screen[%d] : (%f,%f,%f,%f)  (%f,%f,%f,%f)\n", i,
               frame.origin.x, frame.origin.y, frame.size.width, frame.size.height,
               visibleFrame.origin.x, visibleFrame.origin.y, visibleFrame.size.width, visibleFrame.size.height
               );
        */
        
        monitor_info[i].monitor_rect = [screen frame];
        monitor_info[i].workspace_rect = [screen visibleFrame];
    }
    
    *num_info = (int)screenCount;
    
    return 0;
}

int ckit_Global_SetClipboardText( const wchar_t * _text )
{
    NSPasteboard * pb = [NSPasteboard generalPasteboard];

    NSString * text = WcharToNSString(_text);
    
    [pb clearContents];
    [pb writeObjects:[NSArray arrayWithObject:text]];

    return 0;
}

int ckit_Global_GetClipboardText( wchar_t ** text )
{
    NSPasteboard * pb = [NSPasteboard generalPasteboard];
    
    NSArray * classes = [NSArray arrayWithObject:[NSString class]];
    NSArray * objects = [pb readObjectsForClasses:classes options:nil];
    
    PRINTF( "ckit_Global_GetClipboardText : objects count = %lu\n", (unsigned long)[objects count] );

    if([objects count]>0)
    {
        NSString * value = [objects objectAtIndex:0];
        *text = NSStringToMallocedWchar(value);
    }

    return 0;
}

int ckit_Global_GetClipboardChangeCount( int * change_count )
{
    NSPasteboard * pb = [NSPasteboard generalPasteboard];
    
    *change_count = (int)pb.changeCount;

    return 0;
}

static void _AccessibilityCallback( AXObserverRef observer, AXUIElementRef element, CFStringRef notificationName, void * contextData )
{
    PRINTF("_AccessibilityCallback : %s\n", CFStringGetCStringPtr(notificationName, kCFStringEncodingUTF8) );
    
    if( CFStringCompare( notificationName, kAXApplicationActivatedNotification, 0 )==kCFCompareEqualTo
     || CFStringCompare( notificationName, kAXApplicationDeactivatedNotification, 0 )==kCFCompareEqualTo
     || CFStringCompare( notificationName, kAXApplicationShownNotification, 0 )==kCFCompareEqualTo
     || CFStringCompare( notificationName, kAXApplicationHiddenNotification, 0 )==kCFCompareEqualTo
    )
    {
        g.need_reset_ax_observer = true;
    }
    
    g.focus_change_count ++;
}

int ckit_Global_GetFocusChangeCount( int * change_count )
{
    AXError ret;
    
    AXUIElementRef systemWide = AXUIElementCreateSystemWide();
    
    AXUIElementRef focusedApp = NULL;
    ret = AXUIElementCopyAttributeValue( systemWide, kAXFocusedApplicationAttribute, (CFTypeRef*)&focusedApp );
    
    if(ret!=kAXErrorSuccess)
    {
        g.focus_change_count ++;
        goto end;
    }
    
    pid_t pid;
    AXUIElementGetPid( focusedApp, &pid );
    
    if( g.need_reset_ax_observer || g.ax_observer==NULL || g.focused_pid!=pid )
    {
        if(g.ax_observer)
        {
            CFRelease(g.ax_observer);
            g.ax_observer = NULL;
        }
        
        AXObserverRef observer;
        ret = AXObserverCreate( pid, _AccessibilityCallback, &observer );
        
        if(ret!=kAXErrorSuccess)
        {
            printf("AXObserverCreate failed = %d\n", ret);
            goto end;
        }
        
        AXObserverAddNotification( observer, focusedApp, kAXApplicationActivatedNotification, NULL );
        AXObserverAddNotification( observer, focusedApp, kAXApplicationDeactivatedNotification, NULL );
        AXObserverAddNotification( observer, focusedApp, kAXApplicationHiddenNotification, NULL );
        AXObserverAddNotification( observer, focusedApp, kAXApplicationShownNotification, NULL );

        AXObserverAddNotification( observer, focusedApp, kAXFocusedWindowChangedNotification, NULL );
        AXObserverAddNotification( observer, focusedApp, kAXFocusedUIElementChangedNotification, NULL );
        //AXObserverAddNotification( observer, focusedApp, kAXUIElementDestroyedNotification, NULL );
        
        CFRunLoopAddSource([[NSRunLoop currentRunLoop] getCFRunLoop],
                           AXObserverGetRunLoopSource(observer),
                           kCFRunLoopDefaultMode);
        
        g.need_reset_ax_observer = false;
        g.ax_observer = observer;
        g.focused_pid = pid;
        g.focus_change_count ++;
    }

end:

    if(focusedApp){ CFRelease(focusedApp); }
    if(systemWide){ CFRelease(systemWide); }
    
    *change_count = g.focus_change_count;
    
    return 0;
}

int ckit_Global_GetRunningApplications( pid_t ** _applications, int * _num )
{
    NSArray * runningApplications = [[NSWorkspace sharedWorkspace] runningApplications];
    
    int num = (int)runningApplications.count;
    
    pid_t * applications = (pid_t*)malloc( num * sizeof(pid_t) );
    
    for( int i=0 ; i<num ; ++i )
    {
        NSRunningApplication * app = [runningApplications objectAtIndex:i];
        applications[i] = app.processIdentifier;
    }
    
    *_applications = applications;
    *_num = num;
    
    return 0;
}

int ckit_Global_GetApplicationNameByPid( pid_t pid, wchar_t ** name )
{
    NSRunningApplication * app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
    NSString * bundleId = [app bundleIdentifier];

    *name = NSStringToMallocedWchar(bundleId);
    
    return 0;
}

int ckit_Global_ActivateApplicationByPid( pid_t pid )
{
    NSRunningApplication * app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
    
    [app activateWithOptions:NSApplicationActivateAllWindows | NSApplicationActivateIgnoringOtherApps];
    
    return 0;
}

int ckit_Global_Beep()
{
    NSBeep();
    
    return 0;
}

int ckit_Global_Test()
{
    /*
    NSWorkspace * ws = [NSWorkspace sharedWorkspace];
    NSRunningApplication * app = [ws frontmostApplication];
    NSString * bundleId = [app bundleIdentifier];
    
    printf( "bundleId : %s\n", [bundleId cStringUsingEncoding:NSUTF8StringEncoding] );
    */
    
    return 0;
}










