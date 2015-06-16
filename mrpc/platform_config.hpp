#ifndef ISU_PLATFORM_CONFIG_HPP
#define ISU_PLATFORM_CONFIG_HPP

#ifdef _MSC_VER
#define _WINDOWS
#endif

typedef void* sysptr_t;

#ifdef _WINDOWS

#define SYSTEM_THREAD_CALLBACK __stdcall
#define SYSTEM_THREAD_RET DWORD

#else
#endif

#define SYSTEM_THREAD_RET_CODE(val) \
	return static_cast<SYSTEM_THREAD_RET>(val);
#endif