#ifndef ISU_RPC_DEF_HPP
#define ISU_RPC_DEF_HPP

#include <xstddef>
#include <archive.hpp>
#include <boost/noncopyable.hpp>
#include <iostream>
#include <rpclog.hpp>
#include <boost/crc.hpp>
extern rpclog mylog;
#ifdef _MSC_VER
#define _PLATFORM_WINDOWS
#else
#define _PLATFORM_UNKNOW
#endif
inline size_t crc16l(const char *ptr, size_t len)
{
	boost::crc_basic<16> mcrc(0x1021, 0xFFFF, 0);
	mcrc.process_bytes(ptr, len);
	return mcrc.checksum();
}
typedef std::size_t size_t;
typedef unsigned int func_ident;
typedef unsigned long long ulong64;
typedef unsigned int rpcver;
typedef void* sysptr_t;
typedef int socket_type;
typedef boost::noncopyable mynocopyable;

enum rpc_request_msg{ 
	rpc_error, rpc_success,rpc_success_and_multicast,
	rpc_async, rpc_source_error };

//仅udp,tcp作为通信方式,tcp和udp混合通信
enum client_mode{ udp = 0x01, tcp = 0x10, udp_and_tcp = 0x11 };

const size_t RPC_SERVER_PORT = 8083;
const size_t DEFAULT_RPC_BUFSIZE = 1024;

typedef void(*rpc_callback)(sysptr_t);
typedef void(*mulitrpc_callback)(sockaddr_in addr, sysptr_t arg);
typedef size_t(*multirpc_callback_real)(const_memory_block blk,
	sockaddr_in addr, sysptr_t arg);

//flag
//组播地址,多播地址,普通地址,ipv6,ipv4,同步,异步
//仅在本机有效

#define RPCINFO_FLAG_MULTICAST 0x1
#define RPCINFO_FLAG_NORMAL_ADDR 0x10
#define RPCINFO_FLAG_ADDR_IPV4 0x100
#define RPCINFO_FLAG_ADDR_IPV6 0x1000
#define RPCINFO_FLAG_IS_SYNC_CALL 0x10000
#define RPCINFO_FLAG_IS_ASYNC_CALL 0x100000

struct rpcinfo
{
	size_t flag;
};

class rwlock;

struct sync_rpcinfo
	:public rpcinfo
{
	~sync_rpcinfo()
	{
		std::cout << "析构" << std::endl;
	}
	rwlock* locke;
	const_memory_block result;
};

struct async_rpcinfo
	:rpcinfo
{
	rpc_callback callback;
	sysptr_t parment;
};

struct multirpc_info
	:public rpcinfo
{
	multirpc_callback_real callback;
	sysptr_t parment;
};

class remote_procall
{
public:
	typedef boost::mpl::na callback_type;
	remote_procall(size_t flag = 0);
	virtual size_t operator()(const udpaddr&,const_memory_block) = 0;
	size_t flag() const;
	bool is_set(size_t) const;
	void set(size_t);
private:
	size_t _flag;
};

class active_event;
class sync_remote_procall
	:public remote_procall
{
public:
#ifndef REMOTE_CALLBACK_USE_RAW_POINTER
	typedef std::function<size_t(const_memory_block)> sync_callback;
#else
	typedef size_t(*sync_callback)(const_memory_block);
#endif
	typedef sync_callback callback_type;

	remote_procall(active_event& eve, sync_callback, size_t flag = 0);
	virtual size_t operator()(const udpaddr&, const_memory_block);
private:
	sync_callback _callback;
	std::reference_wrapper<active_event> _event;
};

class async_remote_procall
	:public remote_procall
{
#ifndef REMOTE_CALLBACK_USE_RAW_POINTER
	typedef std::function<sysptr_t, const_memory_block> async_callback;
#else
	typedef size_t(*async_callback)
		(sysptr_t, const_memory_block) async_callback;
#endif
	typedef async_callback callback_type;

	async_remote_procall(async_callback callback,
		sysptr_t argument, int flag = 0);
	virtual size_t operator()(const udpaddr&, const_memory_block);

private:
	sysptr_t _argument;
	async_callback _callback;
};

class multicast_remote_procall
	:public remote_procall
{
#ifndef REMOTE_CALLBACK_USE_RAW_POINTER
	typedef std::function < size_t(const udpaddr&,size_t,
		sysptr_t, const_memory_block)> multicast_callback;
#else
	typedef size_t(*multicast_callback)(const udpaddr&,size_t,
		sysptr_t, const_memory_block) > multicast_callback;
#endif
	typedef multicast_callback callback_type;

	multicast_remote_procall(size_t wait_until,active_event& event,
		multicast_callback, sysptr_t argument, size_t flag = 0);
	virtual size_t operator()(const udpaddr&, const_memory_block);
private:
	mulitrpc_callback _callback;
	sysptr_t _argument;
	size_t wait_until;
	std::reference_wrapper<active_event> _event;
};

struct rpc_trunk
{
	remote_procall* rpc;
};


#define ID_ISSYNC 0x80000000
struct rpcid
{
	rpcid();
	rpcid(const ulong64&);

	operator ulong64() const;

	size_t funcid;
	size_t id;
};

struct rpc_head
{
	rpcid id;
	rpcinfo* info;
};
//字节对齐问题
inline size_t archived_size(const rpc_head& head)
{
	size_t ret = 0;
	if (head.id.id&ID_ISSYNC)
		ret = sizeof(size_t)*2 + sizeof(rpcinfo);
	else
		ret = sizeof(size_t) + sizeof(rpcinfo);
	return ret;
}

inline size_t archive_to(memory_address buffer, size_t size, 
	const rpc_head& value)
{
	size_t ret = 0;
	if (value.id.id&ID_ISSYNC)
		ret = archive_to(buffer,size, value.id, value.info);
	else
		ret = archive_to(buffer, size, value.id.funcid, value.info);
	return ret;
}

inline size_t rarchive_from(const void* buf, size_t size, rpc_head& value)
{
	size_t ret = 0;
	if (value.id.id&ID_ISSYNC)
		ret = rarchive(buf, size, value.id, value.info);
	else
		ret = rarchive(buf, size, value.id.funcid, value.info);
	return ret;
}

inline size_t rarchive(const void* buf, size_t size, rpc_head& value)
{
	return rarchive_from(buf, size, value);
}
func_ident rpc_number_generic();

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

void logit(const char* str);

//
struct mycount
{
	char flag() const;//inface 没啥意义
	void set_flag(char flag);
	void clear_flag();

	char group_type : 8;
	size_t size : 24;
};

struct group_impl
{
	mycount miscount;//mis的数量
	size_t mislength;//大多数mis的长度
	size_t that_length_count;//有多少个连续的与mislength相同的mis
	int nop;
};


//为了扩展性，不用enum作为定义
#define GRPACK_ASYNC 0x80
#define GRPACK_HAVE_MSG_LOCK 0x40
#define GRPACK_SYNCPACK_FORCE_SIZE 0x20
#define GRPACK_IS_SINGAL_PACK 0x10
#define GRPACK_COMPRESSED 0x8
#define GRPACK_LAST_3BIT_IS_SET 0x7

//
#define GRP_STANDBY 0x01
#define GRP_MANUALLY_STANDBY 0x10
#define GRP_LENGTH_BROKEN 0x100



#endif