#include "stdafx.h"
#include "stdafx_luabind.h"
#include "CtrlrLuaObjectWrapper.h"

CtrlrLuaObjectWrapper::CtrlrLuaObjectWrapper()
{
	o = new luabind::object();
}

CtrlrLuaObjectWrapper::CtrlrLuaObjectWrapper(luabind::object const& other)
{
	o = new luabind::object(other);
}

CtrlrLuaObjectWrapper::CtrlrLuaObjectWrapper(const CtrlrLuaObjectWrapper& other)
{
	o = new luabind::object(*other.o);
}

CtrlrLuaObjectWrapper& CtrlrLuaObjectWrapper::operator= (const CtrlrLuaObjectWrapper& other)
{
	if (this != &other)
		*o = *other.o;

	return *this;
}

CtrlrLuaObjectWrapper::~CtrlrLuaObjectWrapper()
{
	delete o;
}

CtrlrLuaObjectWrapper::operator luabind::object &()
{
	return *o;
}

CtrlrLuaObjectWrapper::operator luabind::object()
{
	return *o;
}

const luabind::object &CtrlrLuaObjectWrapper::getLuabindObject() const
{
	return *o;
}

const luabind::object &CtrlrLuaObjectWrapper::getObject() const
{
	return *o;
}
