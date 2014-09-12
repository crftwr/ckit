#include "basictypes.h"
#include "ckitcore_hook.h"

#if defined(PLATFORM_WIN32)
# include "ckitcore_hook_win.h"
#elif defined(PLATFORM_MAC)
# include "ckitcore_hook_mac.h"
#endif // PLATFORM_XXX

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

/*
// フックのフラグを、擬似入力のフラグに変換する
static int flags_conv( DWORD flags )
{
	int ret = 0;
	if( flags & LLKHF_EXTENDED ) ret |= KEYEVENTF_EXTENDEDKEY;
	if( flags & LLKHF_UP ) ret |= KEYEVENTF_KEYUP;
	return ret;
}

static LRESULT CALLBACK Hook_wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PythonUtil::GIL_Ensure gil_ensure;
    
	switch(message)
	{
        case WM_CREATE:
            break;
            
        case WM_DESTROY:
		{
			PostQuitMessage(0);
		}
            break;
            
        case WM_DRAWCLIPBOARD:
        case WM_CHANGECBCHAIN:
            return Hook_Clipboard_wndProc(hWnd, message, wParam, lParam );
            break;
            
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
*/

HookBase::HookBase()
    :
    keydown_handler(NULL),
    keyup_handler(NULL)
{
}

HookBase::~HookBase()
{
    Py_XDECREF(keydown_handler);
    Py_XDECREF(keyup_handler);
}

static int Hook_init( PyObject * self, PyObject * args, PyObject * kwds)
{
	if( ! PyArg_ParseTuple( args, "" ) )
    {
        return -1;
    }
    
	// FIXME : Hookオブジェクトはシステム上に1つだけ。チェックするべき
    
	Hook * hook = new Hook();
    
	((Hook_Object*)self)->p = hook;
    
	return 0;
}

static void Hook_dealloc( PyObject * self )
{
    delete ((Hook_Object*)self)->p;

	self->ob_type->tp_free((PyObject*)self);
}

static PyObject * Hook_destroy( PyObject* self, PyObject* args )
{
	if( ! PyArg_ParseTuple(args, "" ) )
    {
        return NULL;
    }
    
    delete ((Hook_Object*)self)->p;
    ((Hook_Object*)self)->p = NULL;
    
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Hook_getattr( Hook_Object * self, PyObject * pyattrname )
{
	const wchar_t * attrname = PyUnicode_AS_UNICODE(pyattrname);
    
	if( attrname[0]=='k' && wcscmp(attrname,L"keydown")==0 )
	{
		if(self->p->keydown_handler)
		{
			Py_INCREF(self->p->keydown_handler);
			return self->p->keydown_handler;
		}
		else
		{
			Py_INCREF(Py_None);
			return Py_None;
		}
	}
	else if( attrname[0]=='k' && wcscmp(attrname,L"keyup")==0 )
	{
		if(self->p->keyup_handler)
		{
			Py_INCREF(self->p->keyup_handler);
			return self->p->keyup_handler;
		}
		else
		{
			Py_INCREF(Py_None);
			return Py_None;
		}
	}
    /*
	else if( attrname[0]=='m' && wcscmp(attrname,L"mousedown")==0 )
	{
		if(self->mousedown)
		{
			Py_INCREF(self->mousedown);
			return self->mousedown;
		}
		else
		{
			Py_INCREF(Py_None);
			return Py_None;
		}
	}
	else if( attrname[0]=='m' && wcscmp(attrname,L"mouseup")==0 )
	{
		if(self->mouseup)
		{
			Py_INCREF(self->mouseup);
			return self->mouseup;
		}
		else
		{
			Py_INCREF(Py_None);
			return Py_None;
		}
	}
	else if( attrname[0]=='m' && wcscmp(attrname,L"mousedblclk")==0 )
	{
		if(self->mousedblclk)
		{
			Py_INCREF(self->mousedblclk);
			return self->mousedblclk;
		}
		else
		{
			Py_INCREF(Py_None);
			return Py_None;
		}
	}
	else if( attrname[0]=='m' && wcscmp(attrname,L"mousemove")==0 )
	{
		if(self->mousemove)
		{
			Py_INCREF(self->mousemove);
			return self->mousemove;
		}
		else
		{
			Py_INCREF(Py_None);
			return Py_None;
		}
	}
	else if( attrname[0]=='m' && wcscmp(attrname,L"mousewheel")==0 )
	{
		if(self->mousewheel)
		{
			Py_INCREF(self->mousewheel);
			return self->mousewheel;
		}
		else
		{
			Py_INCREF(Py_None);
			return Py_None;
		}
	}
	else if( attrname[0]=='m' && wcscmp(attrname,L"mousehorizontalwheel")==0 )
	{
		if(self->mousehorizontalwheel)
		{
			Py_INCREF(self->mousehorizontalwheel);
			return self->mousehorizontalwheel;
		}
		else
		{
			Py_INCREF(Py_None);
			return Py_None;
		}
	}
	else if( attrname[0]=='c' && wcscmp(attrname,L"clipboard")==0 )
	{
		if(self->clipboard)
		{
			Py_INCREF(self->clipboard);
			return self->clipboard;
		}
		else
		{
			Py_INCREF(Py_None);
			return Py_None;
		}
	}
    */
	else
	{
		return PyObject_GenericGetAttr((PyObject*)self,pyattrname);
	}
}

static int Hook_setattr( Hook_Object * self, PyObject * pyattrname, PyObject * pyvalue )
{
	const wchar_t * attrname = PyUnicode_AS_UNICODE(pyattrname);
    
	if( attrname[0]=='k' && wcscmp(attrname,L"keydown")==0 )
	{
		if(pyvalue!=Py_None)
		{
			Py_INCREF(pyvalue);
			Py_XDECREF(self->p->keydown_handler);
			self->p->keydown_handler = pyvalue;
		}
		else
		{
			Py_XDECREF(self->p->keydown_handler);
			self->p->keydown_handler = NULL;
		}
	}
	else if( attrname[0]=='k' && wcscmp(attrname,L"keyup")==0 )
	{
		if(pyvalue!=Py_None)
		{
			Py_INCREF(pyvalue);
			Py_XDECREF(self->p->keyup_handler);
			self->p->keyup_handler = pyvalue;
		}
		else
		{
			Py_XDECREF(self->p->keyup_handler);
			self->p->keyup_handler = NULL;
		}
	}
    /*
	else if( attrname[0]=='m' && wcscmp(attrname,L"mousedown")==0 )
	{
		if(pyvalue!=Py_None)
		{
			Py_INCREF(pyvalue);
			Py_XDECREF(self->mousedown);
			self->mousedown = pyvalue;
            
			HookStart_Mouse();
		}
		else
		{
			Py_XDECREF(self->mousedown);
			self->mousedown = NULL;
		}
	}
	else if( attrname[0]=='m' && wcscmp(attrname,L"mouseup")==0 )
	{
		if(pyvalue!=Py_None)
		{
			Py_INCREF(pyvalue);
			Py_XDECREF(self->mouseup);
			self->mouseup = pyvalue;
            
			HookStart_Mouse();
		}
		else
		{
			Py_XDECREF(self->mouseup);
			self->mouseup = NULL;
		}
	}
	else if( attrname[0]=='m' && wcscmp(attrname,L"mousedblclk")==0 )
	{
		if(pyvalue!=Py_None)
		{
			Py_INCREF(pyvalue);
			Py_XDECREF(self->mousedblclk);
			self->mousedblclk = pyvalue;
            
			HookStart_Mouse();
		}
		else
		{
			Py_XDECREF(self->mousedblclk);
			self->mousedblclk = NULL;
		}
	}
	else if( attrname[0]=='m' && wcscmp(attrname,L"mousemove")==0 )
	{
		if(pyvalue!=Py_None)
		{
			Py_INCREF(pyvalue);
			Py_XDECREF(self->mousemove);
			self->mousemove = pyvalue;
            
			HookStart_Mouse();
		}
		else
		{
			Py_XDECREF(self->mousemove);
			self->mousemove = NULL;
		}
	}
	else if( attrname[0]=='m' && wcscmp(attrname,L"mousewheel")==0 )
	{
		if(pyvalue!=Py_None)
		{
			Py_INCREF(pyvalue);
			Py_XDECREF(self->mousewheel);
			self->mousewheel = pyvalue;
            
			HookStart_Mouse();
		}
		else
		{
			Py_XDECREF(self->mousewheel);
			self->mousewheel = NULL;
		}
	}
	else if( attrname[0]=='m' && wcscmp(attrname,L"mousehorizontalwheel")==0 )
	{
		if(pyvalue!=Py_None)
		{
			Py_INCREF(pyvalue);
			Py_XDECREF(self->mousehorizontalwheel);
			self->mousehorizontalwheel = pyvalue;
            
			HookStart_Mouse();
		}
		else
		{
			Py_XDECREF(self->mousehorizontalwheel);
			self->mousehorizontalwheel = NULL;
		}
	}
	else if( attrname[0]=='c' && wcscmp(attrname,L"clipboard")==0 )
	{
		if(pyvalue!=Py_None)
		{
			Py_INCREF(pyvalue);
			Py_XDECREF(self->clipboard);
			self->clipboard = pyvalue;
            
			HookStart_Clipboard();
		}
		else
		{
			Py_XDECREF(self->clipboard);
			self->clipboard = NULL;
            
			HookEnd_Clipboard();
		}
	}
    */
	else
	{
		return PyObject_GenericSetAttr((PyObject*)self,pyattrname,pyvalue);
	}
    
	return 0;
}

static PyObject * Hook_reset( Hook_Object * self, PyObject* args )
{
	if( ! PyArg_ParseTuple(args, "" ) )
        return NULL;
    
	if( ! ((Hook_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}
    
    ((Hook_Object*)self)->p->reset();
        
	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef Hook_methods[] = {
	{ "destroy", Hook_destroy, METH_VARARGS, "" },
	{ "reset", (PyCFunction)Hook_reset, METH_VARARGS, "" },
	{NULL,NULL}
};

PyTypeObject Hook_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Hook",				/* tp_name */
	sizeof(Hook_Type), /* tp_basicsize */
	0,					/* tp_itemsize */
	Hook_dealloc,		/* tp_dealloc */
	0,					/* tp_print */
	0,					/* tp_getattr */
	0,					/* tp_setattr */
	0,					/* tp_compare */
	0, 					/* tp_repr */
	0,					/* tp_as_number */
	0,					/* tp_as_sequence */
	0,					/* tp_as_mapping */
	0,					/* tp_hash */
	0,					/* tp_call */
	0,					/* tp_str */
	(getattrofunc)Hook_getattr,	/* tp_getattro */
	(setattrofunc)Hook_setattr,	/* tp_setattro */
	0,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,/* tp_flags */
	"",					/* tp_doc */
	0,					/* tp_traverse */
	0,					/* tp_clear */
	0,					/* tp_richcompare */
	0,					/* tp_weaklistoffset */
	0,					/* tp_iter */
	0,					/* tp_iternext */
	Hook_methods,		/* tp_methods */
	0,					/* tp_members */
	0,					/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
	0,					/* tp_dictoffset */
	Hook_init,			/* tp_init */
	0,					/* tp_alloc */
	PyType_GenericNew,	/* tp_new */
	0,					/* tp_free */
};


//-----------------------------------------------------------------------------

InputBase::InputBase()
{
}

InputBase::~InputBase()
{
}

static int Input_init( PyObject * self, PyObject * args, PyObject * kwds)
{
	if( ! PyArg_ParseTuple( args, "" ) )
    {
        return -1;
    }
    
    Input * input = new Input();
    
	((Input_Object*)self)->p = input;
    
	return 0;
}

#if !defined(_MSC_VER)
# define _snprintf_s( a, b, c, ... ) snprintf( a, b, __VA_ARGS__ )
#endif

static PyObject * Input_repr( PyObject * self )
{
	if( ! ((Input_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}
    
    std::wstring s = ((Input_Object*)self)->p->ToString();
    
	PyObject * pyret = Py_BuildValue( "u", s.c_str() );
	return pyret;
}

static PyObject * Input_setKeyDown(PyObject* self, PyObject* args)
{
	int vk;
    
	if( ! PyArg_ParseTuple(args, "i", &vk ) )
    {
        return NULL;
    }

	if( ! ((Input_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}
    
    ((Input_Object*)self)->p->setKeyDown(vk);
    
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Input_setKeyUp(PyObject* self, PyObject* args)
{
	int vk;
    
	if( ! PyArg_ParseTuple(args, "i", &vk ) )
    {
        return NULL;
    }
    
	if( ! ((Input_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}
    
    ((Input_Object*)self)->p->setKeyUp(vk);
    
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Input_setKey(PyObject* self, PyObject* args)
{
	int vk;
    
	if( ! PyArg_ParseTuple(args, "i", &vk ) )
    {
        return NULL;
    }
    
	if( ! ((Input_Object*)self)->p )
	{
		PyErr_SetString( PyExc_ValueError, "already destroyed." );
		return NULL;
	}
    
    ((Input_Object*)self)->p->setKey(vk);
    
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * Input_send( PyObject * self, PyObject * args )
{
	PyObject * py_input_list;
    
	if( ! PyArg_ParseTuple(args,"O", &py_input_list ) )
		return NULL;
    
	if( !PyTuple_Check(py_input_list) && !PyList_Check(py_input_list) )
	{
		PyErr_SetString( PyExc_TypeError, "argument must be a tuple or list." );
		return NULL;
	}

    Input::Send( py_input_list );

	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef Input_methods[] = {
    { "setKeyDown", Input_setKeyDown, METH_VARARGS, "" },
    { "setKeyUp", Input_setKeyUp, METH_VARARGS, "" },
    { "setKey", Input_setKey, METH_VARARGS, "" },
    { "send", Input_send, METH_STATIC|METH_VARARGS, "" },
	{NULL,NULL}
};

PyTypeObject Input_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Input",			/* tp_name */
	sizeof(Input_Type), /* tp_basicsize */
	0,					/* tp_itemsize */
	0,					/* tp_dealloc */
	0,					/* tp_print */
	0,					/* tp_getattr */
	0,					/* tp_setattr */
	0,					/* tp_compare */
	Input_repr,			/* tp_repr */
	0,					/* tp_as_number */
	0,					/* tp_as_sequence */
	0,					/* tp_as_mapping */
	0,					/* tp_hash */
	0,					/* tp_call */
	0,					/* tp_str */
	PyObject_GenericGetAttr,/* tp_getattro */
	PyObject_GenericSetAttr,/* tp_setattro */
	0,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,/* tp_flags */
	"",					/* tp_doc */
	0,					/* tp_traverse */
	0,					/* tp_clear */
	0,					/* tp_richcompare */
	0,					/* tp_weaklistoffset */
	0,					/* tp_iter */
	0,					/* tp_iternext */
	Input_methods,		/* tp_methods */
	0,					/* tp_members */
	0,					/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
	0,					/* tp_dictoffset */
	Input_init,			/* tp_init */
	0,					/* tp_alloc */
	PyType_GenericNew,	/* tp_new */
	0,					/* tp_free */
};



