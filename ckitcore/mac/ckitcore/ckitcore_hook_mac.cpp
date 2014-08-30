#include <CoreGraphics/CGEventSource.h>

#include "pythonutil.h"
#include "ckitcore_hook_mac.h"

using namespace ckit;

//-----------------------------------------------------------------------------

//#define PRINTF printf
#define PRINTF(...)

//#define TRACE printf("%s(%d) : %s\n",__FILE__,__LINE__,__FUNCTION__)
#define TRACE

#if 0
struct FuncTrace
{
    FuncTrace( const char * _funcname, unsigned int _lineno )
    {
        funcname = _funcname;
        lineno   = _lineno;
        
        printf( "FuncTrace : Enter : %s(%d)\n", funcname, lineno );
    }
    
    ~FuncTrace()
    {
        printf( "FuncTrace : Leave : %s(%d)\n", funcname, lineno );
    }
    
    const char * funcname;
    unsigned int lineno;
};
#define FUNC_TRACE FuncTrace functrace(__FUNCTION__,__LINE__)
#else
#define FUNC_TRACE
#endif

#define WARN_NOT_IMPLEMENTED printf("Warning: %s is not implemented.\n", __FUNCTION__)

//-----------------------------------------------------------------------------

HookMac::HookMac()
    :
    HookBase(),
    eventTap(NULL),
    runLoopSource(NULL),
    eventSource(NULL)
{
    FUNC_TRACE;
}

HookMac::~HookMac()
{
    FUNC_TRACE;

    UninstallKeyHook();
}

int HookMac::reset()
{
    FUNC_TRACE;

    UninstallKeyHook();
    InstallKeyHook();

    return 0;
}

static CGEventRef _KeyHookCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void * userdata )
{
    HookMac * hook = (HookMac*)userdata;
    return hook->KeyHookCallback( proxy, type, event );
}

CGEventRef HookMac::KeyHookCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event)
{
    FUNC_TRACE;

    PRINTF("KeyHookCallback : type = %d\n", type);
    
    //PRINTF("KeyHookCallback : src = %lld\n", CGEventGetIntegerValueField(event, kCGEventSourceStateID) );
    //PRINTF("KeyHookCallback : mysrc = %u\n", CGEventSourceGetSourceStateID(eventSource) );
    
    // 自分で挿入したイベントは処理しない
    if( CGEventGetIntegerValueField(event, kCGEventSourceStateID)==CGEventSourceGetSourceStateID(eventSource) )
    {
        return event;
    }
    
    PythonUtil::GIL_Ensure gil_ensure;
    
    int vk = (int)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
    
    PyObject * handler = NULL;
    
    switch(type)
    {
    case kCGEventKeyDown:
        handler = keydown_handler;
        break;

    case kCGEventKeyUp:
        handler = keyup_handler;
        break;
    
    case kCGEventFlagsChanged:
        {
            CGEventFlags mod = CGEventGetFlags(event);
            
            bool down;
            switch(vk)
            {
            case kVK_Control:
                down = (mod & NX_DEVICELCTLKEYMASK) != 0;
                break;
            case kVK_RightControl:
                down = (mod & NX_DEVICERCTLKEYMASK) != 0;
                break;
            case kVK_Shift:
                down = (mod & NX_DEVICELSHIFTKEYMASK) != 0;
                break;
            case kVK_RightShift:
                down = (mod & NX_DEVICERSHIFTKEYMASK) != 0;
                break;
            case kVK_Command:
                down = (mod & NX_DEVICELCMDKEYMASK) != 0;
                break;
            case 0x36: // RightCommand
                down = (mod & NX_DEVICERCMDKEYMASK) != 0;
                break;
            case kVK_Option:
                down = (mod & NX_DEVICELALTKEYMASK) != 0;
                break;
            case kVK_RightOption:
                down = (mod & NX_DEVICERALTKEYMASK) != 0;
                break;
            case kVK_Function:
                down = (mod & 0x800000) != 0;
                break;
            default:
                goto pass_through;
            }
            
            if(down)
            {
                handler = keydown_handler;
            }
            else
            {
                handler = keyup_handler;
            }
        }
        break;
    }
    
    if(handler)
    {
        PyObject * pyarglist = Py_BuildValue("(ii)", vk, 0 );
        PyObject * pyresult = PyEval_CallObject( handler, pyarglist );
        Py_DECREF(pyarglist);
        if(pyresult)
        {
            int result;
            if( pyresult==Py_None )
            {
                result = 0;
            }
            else
            {
                PyArg_Parse(pyresult,"i", &result );
            }
            Py_DECREF(pyresult);
            
            if(result)
            {
                // Python側で処理済みなのでイベントを捨てる
                CGEventSetType(event, kCGEventNull);
            }
        }
        else
        {
            PyErr_Print();
        }
    }
    
    TRACE;

    /*
    if( type==kCGEventKeyDown || type==kCGEventKeyUp )
    {
        // The incoming keycode.
        CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        
        PRINTF("KeyHookCallback : keycode = %d\n", keycode);
        
        // Swap 'a' (keycode=0) and 'z' (keycode=6).
        if (keycode == (CGKeyCode)0)
        {
            keycode = (CGKeyCode)6;
            
            // Set the modified keycode field in the event.
            CGEventSetIntegerValueField(event, kCGKeyboardEventKeycode, (int64_t)keycode);
        }
        else if (keycode == (CGKeyCode)6)
        {
            keycode = (CGKeyCode)0;
            
            // Set the modified keycode field in the event.
            CGEventSetIntegerValueField(event, kCGKeyboardEventKeycode, (int64_t)keycode);
        }
        else if( keycode==kVK_ANSI_1 )
        {
            // ignore the event
            CGEventSetType(event, kCGEventNull);
        }
        else if( keycode==kVK_ANSI_2 && type==kCGEventKeyDown )
        {
            CGEventRef cmdd = CGEventCreateKeyboardEvent(eventSource, kVK_Command, true);
            CGEventRef cmdu = CGEventCreateKeyboardEvent(eventSource, kVK_Command, false);
            CGEventRef spcd = CGEventCreateKeyboardEvent(eventSource, kVK_Space, true);
            CGEventRef spcu = CGEventCreateKeyboardEvent(eventSource, kVK_Space, false);
            
            CGEventSetFlags(spcd, kCGEventFlagMaskCommand);
            CGEventSetFlags(spcu, kCGEventFlagMaskCommand);
            
            CGEventTapLocation loc = kCGHIDEventTap;
            CGEventPost(loc, cmdd);
            CGEventPost(loc, spcd);
            CGEventPost(loc, spcu);
            CGEventPost(loc, cmdu);
            
            CFRelease(cmdd);
            CFRelease(cmdu);
            CFRelease(spcd);
            CFRelease(spcu);
            
            // ignore the event
            CGEventSetType(event, kCGEventNull);
        }
    }
    else if( type==kCGEventFlagsChanged )
    {
        CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        
        PRINTF("KeyHookCallback : keycode = %d\n", keycode);
    }
    */
    
pass_through:
    return event;
}

int HookMac::InstallKeyHook()
{
    FUNC_TRACE;

    CGEventMask eventMask = (1 << kCGEventKeyDown) | (1 << kCGEventKeyUp) | (1 << kCGEventFlagsChanged);
    
    eventTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, eventMask, _KeyHookCallback, this );
    runLoopSource = CFMachPortCreateRunLoopSource( kCFAllocatorDefault, eventTap, 0);
    eventSource = CGEventSourceCreate(kCGEventSourceStatePrivate);
    CFRunLoopAddSource( CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes );
    CGEventTapEnable(eventTap, true);
 
    return 0;
}

int HookMac::UninstallKeyHook()
{
    FUNC_TRACE;

    if(eventSource){ CFRelease(eventSource); eventSource=NULL; }
    if(runLoopSource){ CFRelease(runLoopSource); runLoopSource=NULL; }
    if(eventTap){ CFRelease(eventTap); eventTap=NULL; }
    
    return 0;
}

