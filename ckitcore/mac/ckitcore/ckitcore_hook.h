#ifndef __CKITCORE_HOOK_H__
#define __CKITCORE_HOOK_H__

#if defined(_DEBUG)
#undef _DEBUG
#include "python.h"
#define _DEBUG
#else
#include "python.h"
#endif

namespace ckit
{
    struct HookBase
    {
        HookBase();
        virtual ~HookBase();
        
        virtual int reset() = 0;

		PyObject * keydown_handler;
		PyObject * keyup_handler;
    };
};

//-------------------------------------------------------------------

extern PyTypeObject Hook_Type;
#define Hook_Check(op) PyObject_TypeCheck(op, &Hook_Type)

struct Hook_Object
{
    PyObject_HEAD
    ckit::HookBase * p;
};


#endif // __CKITCORE_HOOK_H__