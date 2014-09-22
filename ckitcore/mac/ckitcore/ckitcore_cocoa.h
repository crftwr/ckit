//
//  ckitcore_cocoa.h
//  ckitcore
//
//  Created by Shimomura Tomonori on 2014/07/05.
//  Copyright (c) 2014年 craftware. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "ckitcore_cocoa_export.h"

//-----------------------------------------------------------------------------

@interface CkitView : NSView <NSWindowDelegate,NSTextInputClient>
{
    @public ckit_Window_Callbacks * callbacks;
    @public void * owner;
    
    @public NSTrackingRectTag mouse_tracking_tag;

    @public BOOL ime_enabled;
    @public NSMutableAttributedString * marked_text;
}

- (id)initWithFrame:(NSRect)frame callbacks:(ckit_Window_Callbacks*)_callbacks owner:(void*)_owner parent_window:(CocoaObject*)_parent_window;

- (void)dealloc;

@property CocoaObject * parent_window;

@end

//-----------------------------------------------------------------------------

@interface CkitMenu : NSMenu <NSMenuDelegate>
{
    @public ckit_Menu_Callbacks * callbacks;
    @public void * owner;
}

- (void)menuClicked:(NSMenuItem*)menuItem;

@end

//-----------------------------------------------------------------------------
