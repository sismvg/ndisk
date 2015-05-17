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

//��udp,tcp��Ϊͨ�ŷ�ʽ,tcp��udp���ͨ��
enum client_mode{ udp = 0x01, tcp = 0x10, udp_and_tcp = 0x11 };

const size_t RPC_SERVER_PORT = 8083;
const size_t DEFAULT_RPC_BUFSIZE = 1024;

typedef void(*rpc_callback)(sysptr_t);
typedef void(*mulitrpc_callback)(sockaddr_in addr, sysptr_t arg);
typedef size_t(*multirpc_callback_real)(const_memory_block blk,
	sockaddr_in addr, sysptr_t arg);

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
		std::cout << "����" << std::endl;
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
//�ֽڶ�������
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
	char flag() const;//inface ûɶ����
	void set_flag(char flag);
	void clear_flag();

	char group_type : 8;
	size_t size : 24;
};

struct group_impl
{
	mycount miscount;//mis������
	size_t mislength;//�����mis�ĳ���
	size_t that_length_count;//�ж��ٸ���������mislength��ͬ��mis
	int nop;
};


//Ϊ����չ�ԣ�����enum��Ϊ����
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