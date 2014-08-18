//
//  ckitcore_cocoa.h
//  ckitcore
//
//  Created by Shimomura Tomonori on 2014/07/05.
//  Copyright (c) 2014年 craftware. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "ckitcore_cocoa_export.h"

@interface CkitView : NSView <NSWindowDelegate,NSTextInputClient>
{
    ckit_Window_Callbacks * callbacks;
    void * owner;
    
    NSTrackingRectTag mouse_tracking_tag;

    @public BOOL ime_enabled;
    @public NSMutableAttributedString * marked_text;
}

- (id)initWithFrame:(NSRect)frame callbacks:(ckit_Window_Callbacks*)_callbacks owner:(void*)_owner parent_window:(CocoaObject*)_parent_window;

- (void)dealloc;

@property CocoaObject * parent_window;

@end
