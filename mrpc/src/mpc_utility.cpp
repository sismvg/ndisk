
#include <mrpc_utility.hpp>

#include <WinSock2.h>
unsigned int cpu_core_count()
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
}