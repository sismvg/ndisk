#ifndef ISU_RPC_STUB_DEFINE_HPP
#define ISU_RPC_STUB_DEFINE_HPP

#include <rpcdef.hpp>
#include <rpclock.hpp>
#include <rpc_rarchive.hpp>

std::shared_ptr<rpc_client> _init_rpc_client()
{
	std::shared_ptr<rpc_client> ret;
	udpaddr addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(RPC_SERVER_PORT);
	addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	auto* ptr = new rpc_client(transfer_by_udp, addr);
	ret.reset(ptr);
	ret->run<rpc_client_tk>();
	return ret;
}

inline std::shared_ptr<rpc_client> get_ifx_rpc_client()
{
	static std::shared_ptr<rpc_client> ret = _init_rpc_client();
	return ret;
}

template<class Cur, class Def>
struct default_pointer_type
{
	typedef typename boost::mpl::if_ <
		typename std::is_same<Cur, void>::type,
		Cur,Def > ::type type;
};

namespace
{
	template<class R, class... Arg>
	size_t call_impl(const argument_container& args,
		MYARG, R(*fn)(Arg...))
	{
		return rpc_rarchive_call<R, Arg...>::
			call(args.buffer, args.size, MYARG_USE, fn);
	}

	
template<class R,class... Arg>
struct client_stub
{
	//所有的type*在传输后都会变成type&
	typedef R(*type)(Arg...);
	typedef int remote_group_type;

	/*
		id:rpc ident for server.
		addr:default server address.
	*/
	client_stub(func_ident id,const udpaddr* addr)
		:ident(id), _multicast_lock(nullptr), server(addr)
	{}

	~client_stub()
	{
		delete _multicast_lock;
	}

	//自定义RPC生成
	/*
	template<class RpcGroup>
	R operator()(rpc_client* client, bool async, const udpaddr* addr,
		RpcGroup* group, remote_procall& rpc, Arg... arg)
	{
		_adjustment(client, addr, group);
		client->call(async, addr, group, ident, &rpc, arg...);
	}*/

	//同步RPC
	R operator()(Arg... arg)
	{
		return this->operator()(
			reinterpret_cast<udpaddr*>(nullptr), 
			reinterpret_cast<remote_produce_group*>(nullptr), arg...);
	}

	template<class RpcGroup>
	R operator()(const udpaddr* addr,
		RpcGroup* group, Arg ... arg)
	{
		return this->operator()(nullptr, addr, group, arg...);
	}

	/*
		Invoken a sync rpc call.
			you can set user-def client,server_address,
				and seralize group,arg is your function argument

	*/

	typedef std::function <
		void(const argument_container& args) > stub_sync_callback;

	template<class RpcGroup>
	R operator()(rpc_client* client, const udpaddr* addr,
		RpcGroup* group, Arg... arg)
	{
		R ret;
		size_t timeout = -1;
		//mylog.log(log_debug, "sync call back gen");
		auto callback = [&](argument_container args)
		{
			typedef typename default_pointer_type <
				RpcGroup, remote_produce_group > ::type type;
			//WARNING:保证锁住调用线程,所以ret一直有效
			//type::rarchive_skip_const<R&, Arg...>(args, ret, arg...);
			advance_in(args, 4);//WARNING:4bit长度
			rarchive_ncnt<R&, Arg...>::rarchive_cnt(
				args, ret, arg...);
			int val = 0;
		};

		//mylog.log(log_debug, "sync call back gen end;
	

		_adjustment(client, addr, group);	
		client->call(false, addr, group, ident,
			callback, timeout, arg...);
		return ret;
	}

	//异步RPC

	typedef std::function<void(sysptr_t)> stub_async_callback;

	void operator()(stub_async_callback callback, sysptr_t ptr, Arg... arg)
	{
		this->operator()(reinterpret_cast<rpc_client*>(nullptr),
			server, callback, ptr, arg...);
	}

	void operator()(rpc_client* client,const udpaddr* addr,
		stub_async_callback callback, sysptr_t ptr, Arg... arg)
	{
		//I hate cpp damn overload
		this->operator()(callback,ptr,client, addr, 
			reinterpret_cast<remote_produce_group*>(nullptr), arg...);
	}

	/*
		Just like sync call,but not block call thread.
			and not result_type. callback and ptr just like
				any local async function to do.
	*/
	template<class RpcGroup>
	void operator()(stub_async_callback callback,sysptr_t ptr,
		rpc_client* client,const udpaddr* addr,
		RpcGroup* grp, Arg... arg)
	{
		_adjustment(client, addr, grp);
		auto mycall = [callback,&arg...](sysptr_t ptrv, const argument_container& args)
		{
			typedef typename default_pointer_type <
				RpcGroup, remote_produce_group > ::type type;
			R ret;
			type::rarchive_skip_const<R&, Arg...>(args, ret, arg...);
			callback(ptrv);
		};
		client->call(true, addr, grp, ident,
		mycall, ptr, arg...);
	}

	//多播RPC

	typedef std::function <
		size_t(const udpaddr&, size_t, sysptr_t) > stub_multicast_callback;

	void operator()(const udpaddr* addr,size_t wait_until,
		stub_multicast_callback callback, sysptr_t ptr, Arg... arg)
	{
		this->operator()(nullptr, server, group,
			wait_until, callback, ptr, arg...);
	}

	void operator()(rpc_client* ctl,
		const udpaddr* addr,size_t wait_until,
		stub_multicast_callback callback, sysptr_t ptr,Arg... arg)
	{
		this->operator()(client, addr, group,
			wait_until, callback, ptr, arg...);
	}

	/*
		send rpc mission to multicast address.
			they will send back.
		you can set block count .until that be zero.
		oeprator() not return.
		and less callback will be async,all call will be lock
		for thread safe
	*/
	template<class RpcGroup>
	void operator()(rpc_client* client, 
		const udpaddr* addr,size_t wait_until,
		RpcGroup* group, stub_multicast_callback callback,
		sysptr_t ptr, Arg... arg)
	{//Warning:If you pass some non-const-ref or non-const-pointer.
		//Please make sure that argument is living until callback finish
		size_t timeout = -1;
		_adjustment(client, addr, group);
		R ret;
		client->call(false, *addr, group, ident,
			[&](const udpaddr& addr, size_t flag,
				sysptr_t ptr, const argument_container& args)
		{
			typedef typename default_pointer_type <
				RpcGroup, remote_produce_group > ::type type;
			auto wlock = _lock_multicast();
			type::rarchive_skip_const
				<R&, Arg..>(args, ret, arg...);
			callback(addr, flag, ptr);
			wlock.unlock();
		}, callback, ptr, wait_until, timeout, arg...);
	}

	func_ident ident;
	const udpaddr* server;

private:
	template<class RpcGroup>
	void _adjustment(rpc_client*& client,
		const udpaddr*& addr, RpcGroup* group)
	{
		if (client == nullptr)
			client = get_ifx_rpc_client().get();

		if (addr == nullptr)
			addr = server;
	}

	kernel_lock& _lock_multicast()
	{
		if (!_multicast_lock)
			_multicast_lock = new kernel_lock;
		return *_multicast_lock;
	}

	kernel_lock* _multicast_lock;
};

template<class R, class... Arg>
auto generic_remote_produce(rpc_client* client,
	func_ident ident, R(*fn)(Arg...))
	->client_stub < R, Arg... >
{
	return client_stub<R, Arg...>(ident, nullptr);
}

}

#define RPC_NUMBER_NAME(func) func##_number

#define RPC_NUMBER_DEF(func)\
	func_ident RPC_NUMBER_NAME(func)()\
	{\
		static const func_ident ret=rpc_number_generic();\
		return ret;\
	}

#define RPC_REGISTER_SERVER_STUB(func,stub_name)\
	static auto ident_generic=[=]() ->func_ident {\
	func_ident ident=RPC_NUMBER_NAME(func)();\
	return ident;\
		};\
	static func_ident ident=ident_generic();

#define RPC_SERVER_STUB(func,stub_name)\
	RPC_NUMBER_DEF(func)\
	size_t stub_name(const_memory_block blk,const rpc_head& head,\
	rpc_request_msg msg,remote_produce_group& grp)\
	{\
		size_t adv=call_impl(blk,head,msg,grp,func);\
		return adv;\
	}\
	static int func##var=([&]() ->int\
	{\
		rpc_server::register_rpc(RPC_NUMBER_NAME(func)(),stub_name);\
		return 0;\
	})();

#define RPC_CLIENT_STUB(func,stub_name,client)\
	auto stub_name=generic_remote_produce(client,\
		RPC_NUMBER_NAME(func)(),func);

#endif