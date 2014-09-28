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
        
        static bool IsAllowed( bool prompt );
        
        CFMachPortRef eventTap;
        CFRunLoopSourceRef runLoopSource;
        
        static CGEventFlags modifier;
    };

    enum InputType
    {
        InputType_None = 0,
        InputType_KeyDown,
        InputType_KeyUp,
        InputType_Key,
    };
    
    struct InputMac : public InputBase
    {
        static int Initialize();
        static int Terminate();
        
    	InputMac();
    	virtual ~InputMac();

        virtual int setKeyDown(int vk);
        virtual int setKeyUp(int vk);
        virtual int setKey(int vk);

        virtual std::wstring ToString();
        
        static int Send( PyObject * py_input_list );
        
        InputType type;
        
        union
        {
            int vk;
        };
        
        static CGEventSourceRef eventSource;
    };
    
	typedef HookMac Hook;
	typedef InputMac Input;
};

#endif //__CKITCORE_HOOK_MAC_H__
