
#pragma once

#define GMOD_USE_SOURCESDK
#include "GarrysMod/Lua/LuaBase.h"
using namespace GarrysMod::Lua;

#include <string>

enum class LuaInterface {
	CLIENT,
	SERVER,
	MENU
};

class CLuaShared {
public:
	virtual ~CLuaShared() = 0;
	virtual int Init(char*, char*, void*, void*) = 0;
	virtual int Shutdown(void) = 0;
	virtual int DumpStats(void) = 0;
	virtual ILuaBase* CreateLuaInterface(int, bool) = 0;
	virtual int CloseLuaInterface(ILuaBase*) = 0;
	virtual ILuaBase* GetLuaInterface(int) = 0;
	virtual int LoadFile(std::string, std::string, std::string, bool) = 0;
	virtual int GetCache(std::string) = 0;
	virtual int MountLua(const char*) = 0;
	virtual int MountLuaAdd(const char*, const char*) = 0;
	virtual int UnMountLua(const char*) = 0;
	virtual void SetFileContents() = 0; // does legit nothing
	virtual int SetLuaFindHook(void*) = 0;
	virtual int FindScripts() = 0;
	virtual void* GetStackTraces() = 0;
};

class iLuaObject;
