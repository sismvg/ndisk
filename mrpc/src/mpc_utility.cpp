
#include <mrpc_utility.hpp>

#include <windows.h>

unsigned int cpu_core_count()
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
}