#include "pythonutil.h"

bool PythonUtil::PyStringToWideString( const PyObject * pystr, std::wstring * str )
{
	if( PyUnicode_Check(pystr) )
	{
		*str = (wchar_t*)PyUnicode_AS_UNICODE(pystr);
		return true;
	}
	else
	{
		PyErr_SetString( PyExc_TypeError, "must be unicode." );
		*str = L"";
		return false;
	}
}
