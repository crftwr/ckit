//
//  ckitcore_cocoa_export.h
//  ckitcore
//
//  Created by Shimomura Tomonori on 2014/07/05.
//  Copyright (c) 2014年 craftware. All rights reserved.
//

#if defined(__cplusplus)
# define EXTERN extern "C"
#else
# define EXTERN extern
#endif

typedef struct CocoaObject_t CocoaObject;

EXTERN int ckit_Application_Create();

EXTERN int ckit_Window_Create( CocoaObject ** window );
EXTERN int ckit_Window_Destroy( CocoaObject * window );
EXTERN int ckit_Window_MessageLoop( CocoaObject * window );

