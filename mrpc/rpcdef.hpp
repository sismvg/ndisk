#ifndef ISU_RPC_DEF_HPP
#define ISU_RPC_DEF_HPP

#include <xstddef>
#include <iostream>
#include <functional>

#include <boost/crc.hpp>
#include <boost/noncopyable.hpp>

#include <rpclog.hpp>
#include <archive.hpp>
#include <active_event.hpp>
#include <rpc_utility.hpp>

extern rpclog mylog;

#ifdef _MSC_VER
#define _PLATFORM_WINDOWS
#else
#define _PLATFORM_UNKNOW
#endif

size_t rpc_crc(const_memory_block);
size_t rpc_crc(const void* ptr, size_t length);

typedef std::size_t size_t;
typedef unsigned int func_ident;
typedef unsigned int rpcver;
typedef unsigned long long ulong64;
typedef void* sysptr_t;
typedef int socket_type;

typedef const_memory_block argument_container;
typedef boost::noncopyable mynocopyable;

typedef sockaddr_in udpaddr;

enum rpc_request_msg{
	rpc_error, rpc_success, rpc_success_and_multicast,
	rpc_async, rpc_source_error
};

enum transfer_mode{ transfer_by_udp = 0x1, transfer_by_tcp = 0x10 };

const size_t RPC_SERVER_PORT = 8083;

//flag
//组播地址,多播地址,普通地址,ipv6,ipv4,同步,异步
//仅在本机有效

#define RPCINFO_FLAG_MULTICAST 0x1
#define RPCINFO_FLAG_NORMAL_ADDR 0x10
#define RPCINFO_FLAG_ADDR_IPV4 0x100
#define RPCINFO_FLAG_ADDR_IPV6 0x1000
#define RPCINFO_FLAG_IS_SYNC_CALL 0x10000
#define RPCINFO_FLAG_IS_ASYNC_CALL 0x100000

class remote_procall
{
public:
	typedef boost::mpl::na callback_type;
	remote_procall(size_t flag = RPCINFO_FLAG_ADDR_IPV4);
	virtual void ready() = 0;
	virtual void start(const udpaddr&) = 0;
	virtual size_t operator()(const udpaddr&,const argument_container&) = 0;
	size_t flag() const;
	bool is_set(size_t) const;
	void unset(size_t);
	void set(size_t);
private:
	size_t _flag;
};

class sync_remote_procall
	:public remote_procall
{
public:
#ifndef REMOTE_CALLBACK_USE_RAW_POINTER
	typedef std::function<size_t(const argument_container&)> sync_callback;
#else
	typedef size_t(*sync_callback)(const argument_container&);
#endif
	typedef sync_callback callback_type;
	/*
		event:For lock produce thread.
		callback:To analyze the archived arguments.
		flag:Some rpc helper.<-def
	*/
	sync_remote_procall(active_event& event,
		sync_callback callback, size_t flag = 0);
	virtual void ready();
	virtual void start(const udpaddr&);
	virtual size_t operator()(const udpaddr&, const argument_container&);
private:
	sync_callback _callback;
	active_event* _event;
};

class async_remote_procall
	:public remote_procall
{
public:
#ifndef REMOTE_CALLBACK_USE_RAW_POINTER
	typedef std::function < size_t(sysptr_t,
		const argument_container&)> async_callback;
#else
	typedef size_t(*async_callback)
		(sysptr_t, const argument_container&) async_callback;
#endif
	typedef async_callback callback_type;
	/*
		callback:To analyze archived arguments.
		argument:Callback argument
		flag:^
	*/
	async_remote_procall(async_callback callback,
		sysptr_t argument, size_t flag = 0);
	virtual void ready();
	virtual void start(const udpaddr&);
	virtual size_t operator()(const udpaddr&, const argument_container&);

private:
	sysptr_t _argument;
	async_callback _callback;	
};

class multicast_remote_procall
	:public remote_procall
{
public:
#ifndef REMOTE_CALLBACK_USE_RAW_POINTER
	typedef std::function < size_t(const udpaddr&,size_t,
		sysptr_t, const argument_container&)> multicast_callback;
#else
	typedef size_t(*multicast_callback)(const udpaddr&,size_t,
		sysptr_t, const argument_container&) > multicast_callback;
#endif
	typedef multicast_callback callback_type;
	/*
		wait_until:When this parment not zero
			produce thread will blocking until wait_until to zero
			and when this parment be zero,all remote callback will be async.
		event:For blocking produce thread.
		callback:Callback
		argment:Callback argument
		flag:^
	*/
	multicast_remote_procall(size_t wait_until,active_event& event,
		multicast_callback callback, sysptr_t argument, size_t flag = 0);
	size_t wait_until() const;
	virtual void ready();
	virtual void start(const udpaddr&);
	virtual size_t operator()(const udpaddr&, const argument_container&);
private:
	multicast_callback _callback;
	sysptr_t _argument;
	size_t _wait_until;
	active_event* _event;
};

/*
	For lockfree programming
*/
struct rpc_trunk
{
	typedef size_t thread_id_type;
	sysptr_t ptr;
	remote_procall* rpc;
};

/*
rpcid._id will set it
*/
#define ID_ISSYNC 0x80000000

struct rpcid
{
	rpcid();
	rpcid(const ulong64&);

	operator ulong64() const;
	ulong64 to_ulong64() const;

	bool is_sync_call() const;
	size_t funcid() const;
	size_t exparment() const;
private:
	size_t _funcid;
	size_t _id;
};

struct rpc_head
{
	rpcid id;
	rpc_trunk trunk;
};

func_ident rpc_number_generic();

//Group item detail
#define GRPACK_ASYNC 0x80
#define GRPACK_HAVE_MSG_LOCK 0x40
#define GRPACK_SYNCPACK_FORCE_SIZE 0x20
#define GRPACK_IS_SINGAL_PACK 0x10
#define GRPACK_COMPRESSED 0x8
#define GRPACK_LAST_3BIT_IS_SET 0x7

//group state
#define GRP_STANDBY 0x01
#define GRP_MANUALLY_STANDBY 0x10
#define GRP_LENGTH_BROKEN 0x100
#endif