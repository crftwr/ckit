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

enum AdditionalVk
{
    kVK_RightCommand = 0x36,
};

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

static CGEventFlags FixupEventFlagMask( CGEventFlags src )
{
    CGEventFlags dst = src;
    
    if( src & (NX_DEVICELCTLKEYMASK|NX_DEVICERCTLKEYMASK) ){ dst |= kCGEventFlagMaskControl; }
    if( src & (NX_DEVICELSHIFTKEYMASK|NX_DEVICERSHIFTKEYMASK) ){ dst |= kCGEventFlagMaskShift; }
    if( src & (NX_DEVICELALTKEYMASK|NX_DEVICERALTKEYMASK) ){ dst |= kCGEventFlagMaskAlternate; }
    if( src & (NX_DEVICELCMDKEYMASK|NX_DEVICERCMDKEYMASK) ){ dst |= kCGEventFlagMaskCommand; }
    if( src & NX_SECONDARYFNMASK ){ dst |= kCGEventFlagMaskSecondaryFn; }
    
    PRINTF("AddEventFlagMask : %llx\n", dst );
    
    return dst;
}

static CGEventFlags VkToFlag( int vk )
{
    switch(vk)
    {
    case kVK_Control:
        return NX_DEVICELCTLKEYMASK;
    case kVK_RightControl:
        return NX_DEVICERCTLKEYMASK;
    case kVK_Shift:
        return NX_DEVICELSHIFTKEYMASK;
    case kVK_RightShift:
        return NX_DEVICERSHIFTKEYMASK;
    case kVK_Command:
        return NX_DEVICELCMDKEYMASK;
    case kVK_RightCommand:
        return NX_DEVICERCMDKEYMASK;
    case kVK_Option:
        return NX_DEVICELALTKEYMASK;
    case kVK_RightOption:
        return NX_DEVICERALTKEYMASK;
    case kVK_Function:
        return NX_SECONDARYFNMASK;
    }
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
    
    PythonUtil::GIL_Ensure gil_ensure;
    
    int vk = (int)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
    
    // down か up かを判定する
    bool down;
    switch(type)
    {
    case kCGEventKeyDown:
        {
            down = true;
        }
        break;
            
    case kCGEventKeyUp:
        {
            down = false;
        }
        break;
            
    case kCGEventFlagsChanged:
        {
            CGEventFlags flags = CGEventGetFlags(event);
            CGEventFlags changed_flag = VkToFlag(vk);
            
            if(changed_flag==0)
            {
                // FIXME : 普通のモディファイアキー以外の理由で kCGEventFlagsChanged
                // が来たときも、modifier を更新するべき
                goto end;
            }
            
            down = ( flags & changed_flag ) != 0;
        }
        break;
    
    case kCGEventTapDisabledByTimeout:
        printf("Warning : Key hook timed out. Re-enabling.\n");
        CGEventTapEnable(eventTap, true);
        goto end;

    default:
        printf("Unknown hook event type %d\n", type);
        goto end;
    }

    // 自分で挿入したイベントはスクリプト処理しない
    if( CGEventGetIntegerValueField(event, kCGEventSourceStateID)==CGEventSourceGetSourceStateID(Input::eventSource) )
    {
        goto treat_flags;
    }
    
    PyObject * handler;
    if(down)
    {
        handler = keydown_handler;
    }
    else
    {
        handler = keyup_handler;
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
                
                goto end;
            }
        }
        else
        {
            PyErr_Print();
            goto end;
        }
    }
    
treat_flags:

    // Pythonで処理されなかったキーに対してはモディファイアキーの処理を行う
    switch(type)
    {
    case kCGEventKeyDown:
    case kCGEventKeyUp:
        {
            // 仮想のモディファイアキーの状態をイベントに設定する
            CGEventSetFlags( event, FixupEventFlagMask( modifier ) );
        }
        break;
            
    case kCGEventFlagsChanged:
        {
            // 仮想のモディファイア状態を更新する
            if(down)
            {
                modifier |= VkToFlag(vk);
            }
            else
            {
                modifier &= ~VkToFlag(vk);
            }
            PRINTF( "updated modifier = %llx\n", modifier );
        }
        break;
    }
    
end:

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

int InputMac::Send( PyObject * py_input_list )
{
	long item_num = PySequence_Length(py_input_list);

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
                
                // Note : この時点では CGEventSetFlags をしない。フックの中でつける
                
                CGEventPost(kCGHIDEventTap, event);

                CFRelease(event);
            }
            break;

        case InputType_KeyUp:
            {
                CGEventRef event = CGEventCreateKeyboardEvent( eventSource, input->vk, false );
                
                // Note : この時点では CGEventSetFlags をしない。フックの中でつける

                CGEventPost(kCGHIDEventTap, event);
                
                CFRelease(event);
            }
            break;

        case InputType_Key:
            {
                CGEventRef event_down = CGEventCreateKeyboardEvent( eventSource, input->vk, true );
                CGEventRef event_up = CGEventCreateKeyboardEvent( eventSource, input->vk, false );
                
                // Note : この時点では CGEventSetFlags をしない。フックの中でつける
                
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

