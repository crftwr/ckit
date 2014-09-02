#include <string.h>
#include <wchar.h>

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

CGEventFlags HookMac::modifier = 0 ;
CGEventSourceRef InputMac::eventSource = NULL;

//-----------------------------------------------------------------------------

HookMac::HookMac()
    :
    HookBase(),
    eventTap(NULL),
    runLoopSource(NULL)
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
    if( CGEventGetIntegerValueField(event, kCGEventSourceStateID)==CGEventSourceGetSourceStateID(Input::eventSource) )
    {
        if(type==kCGEventFlagsChanged)
        {
            modifier = CGEventGetFlags(event);
        }
        
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
            modifier = CGEventGetFlags(event);
            
            bool down;
            switch(vk)
            {
            case kVK_Control:
                down = (modifier & NX_DEVICELCTLKEYMASK) != 0;
                break;
            case kVK_RightControl:
                down = (modifier & NX_DEVICERCTLKEYMASK) != 0;
                break;
            case kVK_Shift:
                down = (modifier & NX_DEVICELSHIFTKEYMASK) != 0;
                break;
            case kVK_RightShift:
                down = (modifier & NX_DEVICERSHIFTKEYMASK) != 0;
                break;
            case kVK_Command:
                down = (modifier & NX_DEVICELCMDKEYMASK) != 0;
                break;
            case 0x36: // RightCommand
                down = (modifier & NX_DEVICERCMDKEYMASK) != 0;
                break;
            case kVK_Option:
                down = (modifier & NX_DEVICELALTKEYMASK) != 0;
                break;
            case kVK_RightOption:
                down = (modifier & NX_DEVICERALTKEYMASK) != 0;
                break;
            case kVK_Function:
                down = (modifier & NX_SECONDARYFNMASK) != 0;
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
    
pass_through:

    return event;
}

int HookMac::InstallKeyHook()
{
    FUNC_TRACE;

    CGEventMask eventMask = (1 << kCGEventKeyDown) | (1 << kCGEventKeyUp) | (1 << kCGEventFlagsChanged);
    
    eventTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, eventMask, _KeyHookCallback, this );
    runLoopSource = CFMachPortCreateRunLoopSource( kCFAllocatorDefault, eventTap, 0);
    CFRunLoopAddSource( CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes );
    CGEventTapEnable(eventTap, true);
 
    return 0;
}

int HookMac::UninstallKeyHook()
{
    FUNC_TRACE;

    if(runLoopSource){ CFRelease(runLoopSource); runLoopSource=NULL; }
    if(eventTap){ CFRelease(eventTap); eventTap=NULL; }
    
    return 0;
}

//-----------------------------------------------------------------------------

int InputMac::Initialize()
{
    eventSource = CGEventSourceCreate(kCGEventSourceStatePrivate);
    return 0;
}

int InputMac::Terminate()
{
    if(eventSource){ CFRelease(eventSource); eventSource=NULL; }
    return 0;
}

InputMac::InputMac()
    :
    InputBase(),
    type(InputType_None),
    vk(0)
{
    
}

InputMac::~InputMac()
{
}

int InputMac::setKeyDown(int _vk)
{
    type = InputType_KeyDown;
    vk = _vk;
    
    return 0;
}

int InputMac::setKeyUp(int _vk)
{
    type = InputType_KeyUp;
    vk = _vk;
    
    return 0;
}

int InputMac::setKey(int _vk)
{
    type = InputType_Key;
    vk = _vk;
    
    return 0;
}


std::wstring InputMac::ToString()
{
	wchar_t buf[64];
	buf[0] = 0;
    
	switch(type)
	{
    case InputType_None:
        break;
            
    case InputType_KeyDown:
		{
            swprintf( buf, sizeof(buf)/sizeof(buf[0]), L"KeyDown(%d)", vk );
		}
        break;
            
    case InputType_KeyUp:
		{
            swprintf( buf, sizeof(buf)/sizeof(buf[0]), L"KeyUp(%d)", vk );
		}
        break;
            
    case InputType_Key:
		{
            swprintf( buf, sizeof(buf)/sizeof(buf[0]), L"Key(%d)", vk );
		}
        break;
	}
	
	if(buf[0]==0)
	{
		swprintf( buf, sizeof(buf)/sizeof(buf[0]), L"<unknown Input object>" );
	}

    return buf;
}

static CGEventFlags TranslateModifier_DeviceToLogical( CGEventFlags src )
{
    CGEventFlags dst = 0;
    
    if( src & (NX_DEVICELCTLKEYMASK|NX_DEVICERCTLKEYMASK) ){ dst |= kCGEventFlagMaskControl; }
    if( src & (NX_DEVICELSHIFTKEYMASK|NX_DEVICERSHIFTKEYMASK) ){ dst |= kCGEventFlagMaskShift; }
    if( src & (NX_DEVICELALTKEYMASK|NX_DEVICERALTKEYMASK) ){ dst |= kCGEventFlagMaskAlternate; }
    if( src & (NX_DEVICELCMDKEYMASK|NX_DEVICERCMDKEYMASK) ){ dst |= kCGEventFlagMaskCommand; }
    if( src & NX_SECONDARYFNMASK ){ dst |= kCGEventFlagMaskSecondaryFn; }
    
    PRINTF("TranslateModifier_DeviceToLogical : %llx\n", dst );
    
    return dst;
}

int InputMac::Send( PyObject * py_input_list )
{
	long item_num = PySequence_Length(py_input_list);

    CGEventFlags mod = HookMac::modifier;
    
	for( int i=0 ; i<item_num ; i++ )
	{
		PyObject * pyitem = PySequence_GetItem( py_input_list, i );
        
		if( !Input_Check(pyitem) )
		{
			PyErr_SetString( PyExc_TypeError, "argument must be a sequence of Input object." );
			return -1;
		}
        
        InputMac * input = (InputMac*)((Input_Object*)pyitem)->p;
        
        switch(input->type)
        {
        case InputType_None:
            break;
                
        case InputType_KeyDown:
            {
                CGEventRef event = CGEventCreateKeyboardEvent( eventSource, input->vk, true );
                
                switch(input->vk)
                {
                case kVK_Control:
                    mod |= NX_DEVICELCTLKEYMASK;
                    break;
                case kVK_RightControl:
                    mod |= NX_DEVICERCTLKEYMASK;
                    break;
                case kVK_Shift:
                    mod |= NX_DEVICELSHIFTKEYMASK;
                    break;
                case kVK_RightShift:
                    mod |= NX_DEVICERSHIFTKEYMASK;
                    break;
                case kVK_Command:
                    mod |= NX_DEVICELCMDKEYMASK;
                    break;
                case 0x36: // RightCommand
                    mod |= NX_DEVICERCMDKEYMASK;
                    break;
                case kVK_Option:
                    mod |= NX_DEVICELALTKEYMASK;
                    break;
                case kVK_RightOption:
                    mod |= NX_DEVICERALTKEYMASK;
                    break;
                case kVK_Function:
                    mod |= NX_SECONDARYFNMASK;
                    break;
                default:
                    CGEventSetFlags( event, TranslateModifier_DeviceToLogical(mod) );
                    break;
                }
                
                CGEventPost(kCGHIDEventTap, event);

                CFRelease(event);
            }
            break;

        case InputType_KeyUp:
            {
                CGEventRef event = CGEventCreateKeyboardEvent( eventSource, input->vk, false );
                
                switch(input->vk)
                {
                case kVK_Control:
                    mod &= ~NX_DEVICELCTLKEYMASK;
                    break;
                case kVK_RightControl:
                    mod &= ~NX_DEVICERCTLKEYMASK;
                    break;
                case kVK_Shift:
                    mod &= ~NX_DEVICELSHIFTKEYMASK;
                    break;
                case kVK_RightShift:
                    mod &= ~NX_DEVICERSHIFTKEYMASK;
                    break;
                case kVK_Command:
                    mod &= ~NX_DEVICELCMDKEYMASK;
                    break;
                case 0x36: // RightCommand
                    mod &= ~NX_DEVICERCMDKEYMASK;
                    break;
                case kVK_Option:
                    mod &= ~NX_DEVICELALTKEYMASK;
                    break;
                case kVK_RightOption:
                    mod &= ~NX_DEVICERALTKEYMASK;
                    break;
                case kVK_Function:
                    mod &= ~NX_SECONDARYFNMASK;
                    break;
                default:
                    CGEventSetFlags( event, TranslateModifier_DeviceToLogical(mod) );
                    break;
                }
                
                CGEventPost(kCGHIDEventTap, event);
                
                CFRelease(event);
            }
            break;

        case InputType_Key:
            {
                CGEventRef event_down = CGEventCreateKeyboardEvent( eventSource, input->vk, true );
                CGEventRef event_up = CGEventCreateKeyboardEvent( eventSource, input->vk, false );
                
                CGEventSetFlags( event_down, TranslateModifier_DeviceToLogical(mod) );
                CGEventSetFlags( event_down, TranslateModifier_DeviceToLogical(mod) );
                
                CGEventPost(kCGHIDEventTap, event_down);
                CGEventPost(kCGHIDEventTap, event_up);
                
                CFRelease(event_down);
                CFRelease(event_up);
            }
            break;
        }
        
		Py_XDECREF(pyitem);
	}
    
    return 0;
}

