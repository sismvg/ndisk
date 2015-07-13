#ifndef ISU_RPC_CS_HPP
#define ISU_RPC_CS_HPP

#include <set>
#include <tuple>
#include <atomic>
#include <thread>
#include <memory>

#include <WinSock2.h>

#include <archive.hpp>

#include <rpcdef.hpp>
#include <rpclock.hpp>
#include <rpc_group.hpp>
#include <udt.hpp>

//两种多播模式
//同步多播：在收到第一个包或用户指定的包数量以前不会返回，
	//收到第一个包以后可以由用户指定是否继续接受，后续包一律异步，除非wait
//异步多播:所有包都是异步的，除非wait

#define RPC_SEND_BY_UDP 0x01
#define RPC_SEND_BY_TCP 0x10

class rpc_client;
class remote_produce_group;

//一个rpcgroup只能有一种arhcive

#ifndef REMOTE_CALLBACK_USE_RAW_POINTER
typedef std::function<void(sysptr_t, 
	const argument_container&)> async_callback;

typedef std::function<void(const argument_container&)> sync_callback;

typedef std::function < void(const udpaddr&, size_t, sysptr_t,
	const argument_container&) > multicast_callback;

#else
typedef size_t(*async_callback)(sysptr_t,const argument_container&);

typedef size_t(*sync_callback)(const argument_container&);

typedef size_t(*multicast_callback)(const udpaddr&，
	size_t, sysptr_t, const argument_container&);

#endif

/*
	一个trunk罢了
*/
class remote_produce_parasite//每个调用过rpc的线程的TLS中都有一个
	:public active_event
{
public:
	remote_produce_parasite(size_t wait_until = 1);
	~remote_produce_parasite();
	enum why_break{ normal_active, will_be_exit, mission_timeout, other };
	/*
		Stop call thread
	*/
	why_break block();
	/*
		Unlock be stop thread
		why_break:why unblock
	*/
	void set_unblock(why_break=normal_active);
	/*
		a wapper
	*/
	active_event& blocker();
	/*
		Every mission in current thread,must be regist
	*/
	void regist_remote_mission(remote_procall*);
	/*
		^ no lock ver
	*/
	void regist_remote_mission_without_lock(remote_procall*);
	/*
		Every mission stop,must be call that.
	*/
	void unregist_remote_mission(remote_procall*,size_t number=0);
	/*
		^no lock ver
	*/
	void unregist_remote_mission_without_lock(remote_procall*,size_t number=0);
private:
	why_break _why;
	//rpc_spinlock _spinlock;
	boost::mutex _spinlock;
	std::set<remote_procall*> _missions;
};

/*
	Socket must be support:
	int sendto(memory_block,flag,addr,length);
	memory_block recvfrom(flag,addr,length);
*/
class rpc_client_tk
	:mynocopyable
{
	typedef socket_type raw_socket;
	typedef udt socket_type;
	typedef std::tuple<rpc_head> tuple;
public:
	/*
		mgr:manager for auto select.
		local:sock bind address.
		mode:How to transfer.
		default_group:default group for send.
	*/
	rpc_client_tk(rpc_client& mgr, const udpaddr& local,
		transfer_mode mode, remote_produce_group* default_group = nullptr);
	~rpc_client_tk();
#define CALL_ARG bool async,const udpaddr* server,\
	remote_produce_group* group,func_ident ident

#define CALL_ARG_USE async,server,group,ident
	
	/*
		Call a mission.
		Support user-define remote procall
	*/
	template<class... Arg>
	void call(CALL_ARG, remote_procall* rpc,Arg... arg)
	{
		static std::hash_set<size_t> keep;
		auto crack = [&](const tuple& tp)
		{
			auto head=std::get<0>(tp);
			auto iter = keep.find(head.id.exparment());
			if (iter != keep.end())
			{
				mylog.log(log_debug, "same id", head.id.exparment());
			}
			else
			{
				keep.insert(head.id.exparment());
			}
			_get_group(group).push_mission(std::get<0>(tp), arg...);
		};
		_call_impl(CALL_ARG_USE, rpc, crack);
	}
	/*
		Call a sync mission.
		Callback:your rarchive must be in here.
		timeout:when timeout be zero,it is return
	*/
	template<class... Arg>
	void call(CALL_ARG, sync_callback callback,
		size_t timeout, Arg... arg)
	{

		auto& parsite = _manager->get_parasite();

		auto* rpc = new sync_remote_procall(
		parsite.blocker(),
		[&](const argument_container& args)
		->size_t
		{
			callback(args);
			return 0;
		});
	//	mylog.log(log_debug, "real address:", rpc);

		call(CALL_ARG_USE, rpc, arg...);
	}
	
	/*
		Call a async mission,it is return before set.
		Callback:When remote call done it will be call.
		and rarchive must be in here.
		argument:argument
	*/

	template<class... Arg>
	void call(CALL_ARG, async_callback callback,
		sysptr_t argument, Arg... arg)
	{
		auto mycall = 
			[=](sysptr_t ptr, const argument_container& args)
		{
			callback(ptr, args);
			return 0;
		};
		auto* rpc = new async_remote_procall(mycall, argument);

		call(CALL_ARG_USE, rpc, arg...);
	}

	/*
		Call to call multicast group member.
		Callback:When any multicast call done.
		it will be call.
		wait_until:Your group member count must be bigger than it
		when wait_until be zero,function call return.
		timeout:when it is be zero,call return.
		*/
	template<class... Arg>
	void call(CALL_ARG, multicast_callback callback,
		sysptr_t argument, size_t wait_until, size_t timeout, Arg... arg)
	{
		auto& blocker = _manager.get().get_parasite().blocker();
		auto* rpc = 
		new multicast_remote_procall(wait_until,blocker,
			[&](const udpaddr& addr,size_t flag,
				sysptr_t ptr, const argument_container& args)
			->size_t 
		{
			callback(addr, flag, ptr, args);
			return 0;
		}, argument, 0);
		call(CALL_ARG_USE, rpc, arg...);
	}

	/*
		Start recv and send thread for remote produce call.
		when recv and send thread inited,event will be set.
		*/
	void run(active_event* event = nullptr);
	/*
		Stop that thread and it will active all waiting rpc thread.
		*/
	void stop();
	/*
		Group must living until rpc_client_tk gone,
		and pre group will be release by youself
		*/
	remote_produce_group*
		set_default_group(remote_produce_group* group);
	/*
		Get address
		*/
	const udpaddr& address() const;
private:
	rpc_client* _manager;
	remote_produce_group* _default_group;
	transfer_mode _mode;
	udpaddr	_address;
	raw_socket	_tcpsock;
	socket_type	_udpsock;

	remote_produce_group& _get_group(remote_produce_group* group);
	/*
		return trans is by udp or tcp
	*/
	bool _send(remote_produce_group& group,
		const udpaddr& server, bool force_udp = false);
	//uh... nevery mind
	int _send_by_tcp(const udpaddr& server, remote_produce_group&);
	int _send_by_udp(const udpaddr& server, remote_produce_group&);
	/*
		make middleware and callback
	*/
	void _process(const udpaddr& server, const_memory_block memory);

	std::shared_ptr<remote_produce_group> _group;

	std::thread* _recver;/*用于接收rpc*/
	void _recver_thread(active_event*);
	
	void _init(rpc_client& mgr, const udpaddr& local,
		transfer_mode mode, remote_produce_group*);
	void _init_socket();

	
	tuple _ready_rpc(CALL_ARG, remote_procall* rpc);
	void _start_rpc(bool async, remote_produce_group& group,
		const udpaddr& server, remote_procall* rpc);

	rpc_head _make_rpc_head(bool async, func_ident ident);

	void _call_impl(CALL_ARG, 
		remote_procall* rpc,std::function<void(const tuple&)> crack);

	static bool _is_async(const rpc_head&);
};

class rpc_client
	:mynocopyable
{
public:
	//server的sock类型必须和socktype一样
	/*
		transfre mode
		local:default client address, 
			|address.port-client_limit|的范围会被client占用
		client_limit:Create how many thread for do rpc.
	*/
	rpc_client(transfer_mode mode,
		const sockaddr_in& local, size_t client_limit = 0);
	~rpc_client();
	/*
		Just some wapper
	*/
	template<class... Arg>
	void call(CALL_ARG, remote_procall* rpc, Arg... arg)
	{
		_mycall_impl(CALL_ARG_USE, rpc, arg...);
	}

	template<class... Arg>
	void call(CALL_ARG, sync_callback callback,
		size_t timeout, Arg... arg)
	{
		_mycall_impl(CALL_ARG_USE, callback, timeout, arg...);
	}

	template<class... Arg>
	void call(CALL_ARG, async_callback callback,
		sysptr_t argument, Arg... arg)
	{
		_mycall_impl(CALL_ARG_USE, callback, argument, arg...);
	}

	template<class... Arg>
	void call(CALL_ARG, multicast_callback callback,
		sysptr_t argument, size_t wait_until, size_t timeout, Arg... arg)
	{
		_mycall_impl(CALL_ARG_USE, callback,
			argument, wait_until, timeout, arg...);
	}

	/*
		Start client threads
	*/
	template<class ClientType>
	void run()
	{
		_init_clients_impl([&](const sockaddr_in& addr, rpc_client* mgr)
			->rpc_client_tk*
		{
			auto* ret = new ClientType(*mgr, addr, _mode, nullptr);
			return ret;
		});
	}

	/*
		client must create by new,
			and client count not big than client_limit.
	*/
	void add_client(rpc_client_tk* client);

	/*
		Now have how many thread for rpc.
	*/
	const size_t client_count() const;

	/*
		When server ptr==nullptr 
			Use that
	*/
	const udpaddr& default_server() const;
	void set_server(const udpaddr&);

private:

	friend class rpc_client_tk;
	/*
		Don't care!
	*/
	static active_event& get_event();
	static remote_produce_parasite& get_parasite();

private:
	template<class... Arg>
	void _mycall_impl(CALL_ARG,Arg... arg)
	{
		if (server == nullptr)
			server = &_server;
		auto* client_ptr = &_get_client();
		client_ptr->call(CALL_ARG_USE, arg...);
	}

	transfer_mode _mode;
	udpaddr _local, _server;

	typedef std::atomic_size_t asize_t;

	//TODO:_next_index and _get_client可以写的更细一点，以带来更好平衡负载

	size_t _next_index();

	asize_t _alloc_loop;
	size_t _limit_of_client;

	rpc_client_tk& _get_client();
	//WARNING:_src和_locks必须在第一次rpc调用前初始化完成,并且完成后不能被修改
	std::vector<rpc_client_tk*> _src;

	void _init(transfer_mode mode, const sockaddr_in& addr, size_t limit);
	/*
		Start a thread for rpc
	*/
	void _async(rpc_client_tk&, active_event&);

	//utility :杂项，用来减少代码重复度的东西罢了
	const sockaddr_in& _address(const sockaddr_in* addr);

	void _init_clients_impl(
		std::function<rpc_client_tk*(const sockaddr_in&, rpc_client*)> client_maker);
};

#endif