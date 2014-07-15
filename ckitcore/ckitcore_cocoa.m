//
//  ckitcore_cocoa.m
//  ckitcore
//
//  Created by Shimomura Tomonori on 2014/07/05.
//  Copyright (c) 2014年 craftware. All rights reserved.
//

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
    self = [super initWithFrame:frame];
    if(self)
    {
        self->callbacks = _callbacks;
        self->owner = _owner;
    }
    return self;
}

- (void)windowWillClose:(NSNotification *)notification
{
    [NSApp stopModal];
}

- (void)drawRect:(NSRect)dirtyRect
{
    TRACE;
    
    [super drawRect:dirtyRect];
    
    // Drawing code here.
    
    CGContextRef gctx = [[NSGraphicsContext currentContext] graphicsPort];
    
    callbacks->drawRect( owner, dirtyRect, gctx );
}

- (void)viewDidEndLiveResize
{
    CGRect rect = [self frame];
    
    callbacks->viewDidEndLiveResize( owner, rect.size );
}

-(void)timerHandler:(NSTimer*)timer
{
    //TRACE;
    callbacks->timerHandler( owner, (__bridge CocoaObject *)(timer) );
}

@end

int ckit_Application_Create()
{
    TRACE;
    
    [NSApplication sharedApplication];

    return 0;
}

int ckit_Window_Create( ckit_Window_Callbacks * _callbacks, void * _owner, CocoaObject ** _window )
{
    TRACE;

    NSWindow * window = [[NSWindow alloc]
                         initWithContentRect:NSMakeRect(0,0,242,242)
                         styleMask: (NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask)
                         backing:NSBackingStoreBuffered
                         defer:NO];
    
    CkitView * view = [[CkitView alloc] initWithFrame:NSMakeRect(0,0,1,1) callbacks:_callbacks owner:_owner ];
    
    window.delegate = view;
    
    [window setContentView:view];
    
    *_window = (__bridge_retained CocoaObject *)window;
    
    return 0;
}

int ckit_Window_Destroy( CocoaObject * _window )
{
    TRACE;
    
    NSWindow * window = (__bridge_transfer NSWindow*)_window;
    (void)window;

    return 0;
}

int ckit_Window_MessageLoop( CocoaObject * _window )
{
    TRACE;
    
    NSWindow * window = (__bridge NSWindow*)_window;
    
    [NSApp runModalForWindow:window];
    
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

int ckit_Window_GetWindowRect( CocoaObject * _window, CGRect * rect )
{
    NSWindow * window = (__bridge NSWindow*)_window;
    
    *rect = [window frame];
    
    return 0;
}

int ckit_Window_GetClientSize( CocoaObject * _window, CGSize * size )
{
    NSWindow * window = (__bridge NSWindow*)_window;
    
    NSView * view = window.contentView;
    CGRect rect = [view frame];
    
    *size = rect.size;
    
    return 0;
}

int ckit_Window_SetNeedsRedraw( CocoaObject * _window )
{
    NSWindow * window = (__bridge NSWindow*)_window;

    NSView * view = window.contentView;
    [view setNeedsDisplay:TRUE];
    
    return 0;
}

int ckit_Window_SetTimer( CocoaObject * _window, float interval, CocoaObject ** _timer )
{
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
    NSTimer * timer = (__bridge NSTimer*)_timer;
    
    [timer invalidate];
    
    return 0;
}
