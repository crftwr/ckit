#ifndef __CKITCORE_HOOK_H__
#define __CKITCORE_HOOK_H__

#include <string>

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
        virtual void fixWierdModifierState() = 0;

		PyObject * keydown_handler;
		PyObject * keyup_handler;
    };

    struct InputBase
    {
        InputBase();
        virtual ~InputBase();
        
        virtual int setKeyDown(int vk) = 0;
        virtual int setKeyUp(int vk) = 0;
        virtual int setKey(int vk) = 0;

        virtual std::wstring ToString() = 0;
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


extern PyTypeObject Input_Type;
#define Input_Check(op) PyObject_TypeCheck(op, &Input_Type)

struct Input_Object
{
    PyObject_HEAD
    ckit::InputBase * p;
};


#endif // __CKITCORE_HOOK_H__