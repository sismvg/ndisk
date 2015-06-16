
#include <WinSock2.h>

#include <rpc_utility.hpp>

void startup_network_api()
{
	WSAData data;
	WSAStartup(MAKEWORD(2, 2), &data);
}

void close_network_api()
{
	WSACleanup();
}

spaddr_type get_addrtype(const sockaddr_in& addr)
{
	//WARNING:太丑了，似乎能该好看一点
	auto val = addr.sin_addr.S_un.S_addr;
	spaddr_type ret = type_A;
	if (val&type_A)
		ret = type_A;
	else if (val&type_B)
		ret = type_B;
	else if (val&type_C)
		ret = type_C;
	else if (val&type_D)
		ret = type_D;
	else
		ret = type_E;
	if (val >= multicast_first&&val <= multicast_last)
		ret = static_cast<spaddr_type>(
			static_cast<size_t>(ret) | type_multicast);
	return ret;
}

bool operator==(const sockaddr_in& lhs, const sockaddr_in& rhs)
{
	return lhs.sin_port == rhs.sin_port&&
		lhs.sin_addr.S_un.S_addr == rhs.sin_addr.S_un.S_addr;
}

bool operator<(const sockaddr_in& lhs, const sockaddr_in& rhs)
{
	size_t lv = lhs.sin_port + lhs.sin_addr.S_un.S_addr;
	size_t rv = rhs.sin_port + rhs.sin_addr.S_un.S_addr;
	return lv < rv;
}

ulong64 make_ulong64(size_t high, size_t low)
{
	ulong64 ret = high;
	return (ret <<= sizeof(size_t) * 8) |= low;
}

thread_id_type get_current_threadid()
{
	return GetCurrentThreadId();
}

socket_error_type get_last_socket_error()
{
	return WSAGetLastError();
}