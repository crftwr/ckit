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
    /*
	char buf[64];
	buf[0] = 0;
    
	switch( ((Input_Object*)self)->num )
	{
        case 0:
            break;
            
        case 1:
		{
			INPUT & input = ((Input_Object*)self)->input[0];
			
			switch(input.type)
			{
                case INPUT_KEYBOARD:
				{
					if( input.ki.dwFlags & KEYEVENTF_UNICODE )
					{
						_snprintf_s( buf, sizeof(buf), _TRUNCATE, "Char(%d)", input.ki.wScan );
					}
					else if( input.ki.dwFlags & KEYEVENTF_KEYUP )
					{
						_snprintf_s( buf, sizeof(buf), _TRUNCATE, "KeyUp(%d)", input.ki.wVk );
					}
					else
					{
						_snprintf_s( buf, sizeof(buf), _TRUNCATE, "KeyDown(%d)", input.ki.wVk );
					}
				}
                    break;
                    
                case INPUT_MOUSE:
				{
					if( input.mi.dwFlags & MOUSEEVENTF_LEFTDOWN )
					{
						_snprintf_s( buf, sizeof(buf), _TRUNCATE, "MouseLeftDown(%d,%d)", input.mi.dx, input.mi.dy );
					}
					else if( input.mi.dwFlags & MOUSEEVENTF_LEFTUP )
					{
						_snprintf_s( buf, sizeof(buf), _TRUNCATE, "MouseLeftUp(%d,%d)", input.mi.dx, input.mi.dy );
					}
					else if( input.mi.dwFlags & MOUSEEVENTF_RIGHTDOWN )
					{
						_snprintf_s( buf, sizeof(buf), _TRUNCATE, "MouseRightDown(%d,%d)", input.mi.dx, input.mi.dy );
					}
					else if( input.mi.dwFlags & MOUSEEVENTF_RIGHTUP )
					{
						_snprintf_s( buf, sizeof(buf), _TRUNCATE, "MouseRightUp(%d,%d)", input.mi.dx, input.mi.dy );
					}
					else if( input.mi.dwFlags & MOUSEEVENTF_MIDDLEDOWN )
					{
						_snprintf_s( buf, sizeof(buf), _TRUNCATE, "MouseMiddleDown(%d,%d)", input.mi.dx, input.mi.dy );
					}
					else if( input.mi.dwFlags & MOUSEEVENTF_MIDDLEUP )
					{
						_snprintf_s( buf, sizeof(buf), _TRUNCATE, "MouseMiddleUp(%d,%d)", input.mi.dx, input.mi.dy );
					}
					else if( input.mi.dwFlags & MOUSEEVENTF_WHEEL )
					{
						_snprintf_s( buf, sizeof(buf), _TRUNCATE, "MouseWheel(%d,%d,%d)", input.mi.dx, input.mi.dy, input.mi.mouseData );
					}
					else if( input.mi.dwFlags & MOUSEEVENTF_HWHEEL )
					{
						_snprintf_s( buf, sizeof(buf), _TRUNCATE, "MouseHorizontalWheel(%d,%d,%d)", input.mi.dx, input.mi.dy, input.mi.mouseData );
					}
					else
					{
						_snprintf_s( buf, sizeof(buf), _TRUNCATE, "MouseMove(%d,%d)", input.mi.dx, input.mi.dy );
					}
				}
                    break;
			}
		}
            break;
            
        case 2:
		{
			INPUT & input = ((Input_Object*)self)->input[0];
            
			switch(input.type)
			{
                case INPUT_KEYBOARD:
				{
					_snprintf_s( buf, sizeof(buf), _TRUNCATE, "Key(%d)", input.ki.wVk );
				}
                    break;
                    
                case INPUT_MOUSE:
				{
					if( input.mi.dwFlags & MOUSEEVENTF_LEFTDOWN )
					{
						_snprintf_s( buf, sizeof(buf), _TRUNCATE, "MouseLeftClick(%d,%d)", input.mi.dx, input.mi.dy );
					}
					else if( input.mi.dwFlags & MOUSEEVENTF_RIGHTDOWN )
					{
						_snprintf_s( buf, sizeof(buf), _TRUNCATE, "MouseRightClick(%d,%d)", input.mi.dx, input.mi.dy );
					}
					else if( input.mi.dwFlags & MOUSEEVENTF_MIDDLEDOWN )
					{
						_snprintf_s( buf, sizeof(buf), _TRUNCATE, "MouseMiddleClick(%d,%d)", input.mi.dx, input.mi.dy );
					}
				}
                    break;
			}
		}
            break;
	}
	
	if(buf[0]==0)
	{
		_snprintf_s( buf, sizeof(buf), _TRUNCATE, "<unknown Input object>" );
	}
    return buf;
    */
    
    return L"<Input>";
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
                
                // FIXME : CGEventSetFlags を使って、モディファイアの状態も設定する
                //CGEventSetFlags(spcd, kCGEventFlagMaskCommand);

                CGEventPost(kCGHIDEventTap, event);

                CFRelease(event);
            }
            break;

        case InputType_KeyUp:
            {
                CGEventRef event = CGEventCreateKeyboardEvent( eventSource, input->vk, false );
                
                // FIXME : CGEventSetFlags を使って、モディファイアの状態も設定する
                //CGEventSetFlags(spcd, kCGEventFlagMaskCommand);
                
                CGEventPost(kCGHIDEventTap, event);
                
                CFRelease(event);
            }
            break;

        case InputType_Key:
            {
                CGEventRef event_down = CGEventCreateKeyboardEvent( eventSource, input->vk, true );
                CGEventRef event_up = CGEventCreateKeyboardEvent( eventSource, input->vk, false );
                
                // FIXME : CGEventSetFlags を使って、モディファイアの状態も設定する
                //CGEventSetFlags(spcd, kCGEventFlagMaskCommand);
                
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

