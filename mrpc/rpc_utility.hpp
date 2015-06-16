#ifndef ISU_RPC_UTILITY_HPP
#define ISU_RPC_UTILITY_HPP


template<class T>
struct result_type
{
	typedef int type;
};

template<class... Arg>
struct result_type < void(Arg...) >
{
	typedef void type;
};

template<class T, class... Arg>
struct result_type < T(Arg...) >
{
	typedef T type;
};

enum spaddr_type{
	type_A=0x7FFFFFFF, 
	type_B=0x80000000, 
	type_C=0xC0000000,
	type_D=0xE0000000, 
	type_E=0xF0000000,
	type_multicast=0x100000
};

const size_t multicast_first = inet_addr("224.0.0.1");
const size_t multicast_last = inet_addr("239.255.255.255");

struct sockaddr_in;
spaddr_type get_addrtype(const sockaddr_in&);
bool operator==(const sockaddr_in& lhs, const sockaddr_in& rhs);
bool operator<(const sockaddr_in& lhs, const sockaddr_in& rhs);

typedef unsigned long long ulong64;
ulong64 make_ulong64(size_t high, size_t low);

typedef unsigned long thread_id_type;
thread_id_type get_current_threadid();

typedef int socket_error_type;
socket_error_type get_last_socket_error();

void startup_network_api();
void close_network_api();

#endif