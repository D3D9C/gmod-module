#pragma once

#include <Windows.h>
#include <cstdint>

namespace vmt {

// Get VMT
static inline void** get(void* obj) {
	return *reinterpret_cast<void***>(obj);
}

// Get method by index
template <typename T>
static inline T get(void* obj, std::uintptr_t index) {
	return reinterpret_cast<T>(get(obj)[index]);
}

// Call method by index
template <typename Ret, typename ...Args>
static inline Ret call(void* obj, std::uintptr_t index, Args ...args) {
	return reinterpret_cast<Ret(__thiscall*)(const void*, decltype(args)...)>(get(obj)[index])(obj, args...);
}

// Hook vmt method
template <typename Fn>
static bool hook_vmt(void** vmt, Fn* oldFunc, const void* newFunc, std::uintptr_t index) {
	Fn* funcAddr = reinterpret_cast<Fn*>(vmt + index);
	if (oldFunc)
		*oldFunc = *funcAddr;

	DWORD oldProt;
	if (!VirtualProtect(funcAddr, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt))
		return false;
	 
	memcpy(funcAddr, &newFunc, sizeof(void*));

	DWORD temp;
	return VirtualProtect(funcAddr, sizeof(void*), oldProt, &temp);
}

// Hook class method
template <typename Fn>
static bool hook(void* obj, Fn* oldFunc, const void* newFunc, std::uintptr_t index) {
	return hook_vmt(get(obj), oldFunc, newFunc, index);
}

}
