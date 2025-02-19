﻿#ifndef _PYTHONUTIL_H_
#define _PYTHONUTIL_H_

#include <string>

#if defined(_DEBUG)
#undef _DEBUG
#include "python.h"
#define _DEBUG
#else
#include "python.h"
#endif

namespace PythonUtil
{
    bool PyStringToString( PyObject * pystr, std::string * str );
	bool PyStringToWideString( PyObject * pystr, std::wstring * str );
	
	//#define GIL_Ensure_TRACE printf("%s(%d) : %s\n",__FILE__,__LINE__,__FUNCTION__)
	#define GIL_Ensure_TRACE

	class GIL_Ensure
	{
	public:
		GIL_Ensure()
		{
			GIL_Ensure_TRACE;
			state = PyGILState_Ensure();
			GIL_Ensure_TRACE;
		};

		~GIL_Ensure()
		{
			GIL_Ensure_TRACE;
			PyGILState_Release(state);
			GIL_Ensure_TRACE;
		};
		
	private:
		PyGILState_STATE state;
	};
};

#endif // _PYTHONUTIL_H_
