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

#define TRACE printf("%s(%d) : %s\n",__FILE__,__LINE__,__FUNCTION__)
//#define TRACE

//-----------------------------------------------------------------------------

@implementation ckitcore_cocoa

@end

int ckit_Application_Create()
{
    TRACE;
    
    [NSApplication sharedApplication];

    return 0;
}

int ckit_Window_Create( CocoaObject ** _window )
{
    TRACE;

    NSWindow * window = [[NSWindow alloc]
                  initWithContentRect:NSMakeRect(0,0,200,200)
                  styleMask:NSTitledWindowMask
                  backing:NSBackingStoreBuffered
                  defer:NO];
    
    *_window = (__bridge_retained CocoaObject *)window;
    
    return 0;
}

int ckit_Window_Destroy( CocoaObject * _window )
{
    TRACE;
    
    NSWindow * window = (__bridge_transfer NSWindow*)_window;

    return 0;
}

int ckit_Window_MessageLoop( CocoaObject * _window )
{
    TRACE;
    
    NSWindow * window = (__bridge NSWindow*)_window;
    
    [NSApp runModalForWindow:window];
    
    return 0;
}

