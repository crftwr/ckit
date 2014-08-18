//
//  ckitcore_cocoa_export.h
//  ckitcore
//
//  Created by Shimomura Tomonori on 2014/07/05.
//  Copyright (c) 2014年 craftware. All rights reserved.
//

#ifndef __CKITCORE_COCOA_EXPORT_H__
#define __CKITCORE_COCOA_EXPORT_H__

#if defined(__cplusplus)
# define EXTERN extern "C"
#else
# define EXTERN extern
#endif


typedef struct CocoaObject_t CocoaObject;

typedef enum ckit_MouseEventType_t
{
    ckit_MouseEventType_LeftDown,
    ckit_MouseEventType_LeftUp,
    ckit_MouseEventType_RightDown,
    ckit_MouseEventType_RightUp,
    ckit_MouseEventType_MiddleDown,
    ckit_MouseEventType_MiddleUp,
    ckit_MouseEventType_LeftDoubleClick,
    ckit_MouseEventType_RightDoubleClick,
    ckit_MouseEventType_MiddleDoubleClick,
    ckit_MouseEventType_Move,
    ckit_MouseEventType_Wheel,
    
} ckit_MouseEventType;

typedef struct ckit_MouseEvent_t
{
    ckit_MouseEventType type;
    CGPoint location;
    CGFloat delta_x;
    CGFloat delta_y;
    
} ckit_MouseEvent;

typedef struct ckit_Window_Callbacks_t
{
    int (*drawRect)( void * owner, CGRect rect, CGContextRef gctx );
    int (*windowShouldClose)( void * owner );
    int (*windowDidResize)( void * owner, CGSize size );
    int (*windowWillResize)( void * owner, CGSize * size );
    int (*windowDidBecomeKey)( void * owner );
    int (*windowDidResignKey)( void * owner );
    int (*timerHandler)( void * owner, CocoaObject * timer );
    int (*keyDown)( void * owner, int vk, int mod );
    int (*keyUp)( void * owner, int vk, int mod );
    int (*insertText)( void * owner, const wchar_t * text, int mod );
    int (*mouse)( void * owner, const ckit_MouseEvent * event );
    int (*imePosition)( void * owner, CGRect * caret_rect );

} ckit_Window_Callbacks;

typedef struct ckit_Window_Create_Parameters_t
{
    ckit_Window_Callbacks * callbacks;
    void * owner;
    CocoaObject * parent_window;
    const wchar_t * title;
    bool titlebar;
    bool minimizable;
    bool resizable;
    
} ckit_Window_Create_Parameters;

EXTERN int ckit_Application_Create();

EXTERN int ckit_Window_Create( ckit_Window_Create_Parameters * params, CocoaObject ** window );
EXTERN int ckit_Window_Destroy( CocoaObject * window );
EXTERN int ckit_Window_MessageLoop( CocoaObject * window );
EXTERN int ckit_Window_Quit( CocoaObject * window );
EXTERN int ckit_Window_RemoveKeyMessage( CocoaObject * window );
EXTERN int ckit_Window_SetWindowRect( CocoaObject * window, CGRect rect );
EXTERN int ckit_Window_GetWindowRect( CocoaObject * window, CGRect * rect );
EXTERN int ckit_Window_GetClientSize( CocoaObject * window, CGSize * size );
EXTERN int ckit_Window_GetScreenSize( CocoaObject * _window, CGSize * size );
EXTERN int ckit_Window_IsVisible( CocoaObject * window, int * visible );
EXTERN int ckit_Window_IsMaximized( CocoaObject * window, int * maximized );
EXTERN int ckit_Window_IsMinimized( CocoaObject * window, int * minimized );
EXTERN int ckit_Window_IsActive( CocoaObject * window, int * active );
EXTERN int ckit_Window_SetNeedsRedraw( CocoaObject * window );
EXTERN int ckit_Window_SetTimer( CocoaObject * window, float interval, CocoaObject ** timer );
EXTERN int ckit_Window_KillTimer( CocoaObject * window, CocoaObject * timer );
EXTERN int ckit_Window_ClientToScreen( CocoaObject * window, CGPoint * point );
EXTERN int ckit_Window_ScreenToClient( CocoaObject * window, CGPoint * point );
EXTERN int ckit_Window_SetTitle( CocoaObject * window, const wchar_t * title );
EXTERN int ckit_Window_Show( CocoaObject * window, bool show, bool activate );
EXTERN int ckit_Window_Activate( CocoaObject * window );
EXTERN int ckit_Window_SetForeground( CocoaObject * window );
EXTERN int ckit_Window_GetMarkedText( CocoaObject * window, CFMutableAttributedStringRef * marked_text );
EXTERN int ckit_Window_IsImeOpened( CocoaObject * window, int * ime_opened );

EXTERN int ckit_Global_SetClipboard_Text( const wchar_t * text );
EXTERN int ckit_Global_GetClipboard_Text( wchar_t ** text );

#endif//__CKITCORE_COCOA_EXPORT_H__
