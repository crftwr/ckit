#ifndef __CKITCORE_HOOK_MAC_H__
#define __CKITCORE_HOOK_MAC_H__

#import "Events.h"

#include "ckitcore_hook.h"

namespace ckit
{
    struct HookMac : public HookBase
    {
    	HookMac();
    	virtual ~HookMac();
        
        virtual int reset();

        int InstallKeyHook();
        int UninstallKeyHook();
        CGEventRef KeyHookCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event);
        
        CFMachPortRef eventTap;
        CFRunLoopSourceRef runLoopSource;
        CGEventSourceRef eventSource;
    };

	typedef HookMac Hook;
};

#endif //__CKITCORE_HOOK_MAC_H__
