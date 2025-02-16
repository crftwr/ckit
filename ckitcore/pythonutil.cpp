#include "strutil.h"
#include "pythonutil.h"

bool PythonUtil::PyStringToString( PyObject * pystr, std::string * str )
{
	if( PyUnicode_Check(pystr) )
	{
		Py_ssize_t len;
		const wchar_t* s = PyUnicode_AsWideCharString(pystr, &len);

		*str = StringUtil::WideCharToMultiByte(s, (int)len);
		return true;
	}
	else
	{
		PyErr_SetString( PyExc_TypeError, "must be unicode." );
		*str = "";
		return false;
	}
}

bool PythonUtil::PyStringToWideString( PyObject * pystr, std::wstring * str )
{
	if( PyUnicode_Check(pystr) )
	{
		Py_ssize_t len;
		const wchar_t* s = PyUnicode_AsWideCharString(pystr, &len);

		*str = s;
		return true;
	}
	else
	{
		PyErr_SetString( PyExc_TypeError, "must be unicode." );
		*str = L"";
		return false;
	}
}
