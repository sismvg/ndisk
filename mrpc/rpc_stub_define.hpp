#ifndef ISU_RPC_STUB_DEFINE_HPP
#define ISU_RPC_STUB_DEFINE_HPP

#include <rpcdef.hpp>
#include <rpc_rarchive.hpp>
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
	static func_ident ident=ident_generic()

template<class R,class... Arg>
size_t call_impl(const_memory_block blk,
	MYARG,R(*fn)(Arg...))
{
	return rpc_rarchive_call<R, Arg...>::
		call(blk.buffer, blk.size, MYARG_USE, fn);
}
#define RPC_SERVER_STUB(func,stub_name)\
	size_t stub_name(const_memory_block blk,const rpc_head& head,\
	rpc_request_msg msg,rpc_group_server& grp)\
	{\
		size_t adv=call_impl(blk,head,msg,grp,func);\
		return adv;\
	}\
	static int func##var=([&]() ->int\
	{\
		rpc_server::register_rpc(RPC_NUMBER_NAME(func)(),stub_name);\
		return 0;\
	})();

#define RPC_ASYNC_CLIENT_STUB(func,stub_name,client)\
	template<class... Arg>\
	void stub_name(rpc_group_client* grp,const sockaddr_in* addr,\
		rpc_callback callback,sysptr_t ptr,Arg... arg)\
				{\
			client.rpc_generic_async(grp,addr,callback,ptr,\
				RPC_NUMBER_NAME(func)(),arg...);\
		return;\
				}

template<class R,class... Arg>
struct client_call_
{
	//所有的type*都会变成type&
	typedef R(*type)(Arg...);
	client_call_()
		:client(nullptr), number(-1),
			group(nullptr), addr(nullptr)
	{}

	client_call_(size_t nm, rpc_client& cl)
		:client(&cl), number(nm)
	{}

	R operator()(Arg... arg)
	{//同步RPC
		return this->operator()(client, addr, group, arg...);
	}

	R operator()(rpc_group_client* client,
		const sockaddr_in* addr, Arg ... arg)
	{
		return this->operator()(client, addr, group, arg...);
	}

	R operator()(rpc_client* clt, const sockaddr_in* addr,
		rpc_group_client* grp, Arg... arg)
	{//If addr is a multicast address.it will stop when anyone server
		//send request and rpc call suceess.
		const_memory_block ret =
			clt->rpc_generic(grp, addr,
			number, arg...);
		typedef R result_type;
		static_assert(!std::is_reference<result_type>::type::value,
			"remote process call can not return a reference");
		result_type func_result;
		advance_in(ret, 4);
		rarchive_ncnt<result_type&, Arg...>::rarchive_cnt(
			ret, func_result, arg...);
		return func_result;
	}

	void operator()(rpc_callback callback, sysptr_t ptr, Arg... arg)
	{
		this->operator()(client, addr, group, callback, ptr, arg...);
	}

	void operator()(rpc_client* clt,const sockaddr_in* addr,
		rpc_callback callback, sysptr_t ptr, Arg... arg)
	{
		this->operator()(clt, addr, group, callback, ptr, arg...);
	}
	
	void operator()(rpc_client* ctl, const sockaddr_in* addr,
		rpc_group_client* grp, rpc_callback callback, sysptr_t ptr,
		Arg... arg)
	{
		ctl->rpc_generic_async(grp, addr,
			callback, ptr, number, arg...);
	}

	void operator()(const sockaddr_in* addr,
		mulitrpc_callback callback, sysptr_t ptr, Arg... arg)
	{
		this->operator()(client, addr, group, callback, ptr, arg...);
	}

	void operator()(rpc_client* ctl, const sockaddr_in* addr,
		mulitrpc_callback callback, sysptr_t ptr,Arg... arg)
	{
		this->operator()(ctl, addr, group, callback, ptr, arg...);
	}

	void operator()(rpc_client* ctl, const sockaddr_in* addr,
		rpc_group_client* grp, mulitrpc_callback callback,
		sysptr_t ptr, Arg... arg)
	{//Warning:If you pass some non-const-ref or non-const-pointer.
		//Please make sure that argument is living until callback finish
		struct lambda_arg
		{
			mulitrpc_callback callback;
			sysptr_t ptr;
		};
		struct lambda
		{
			void callback(const_memory_block blk,
				sockaddr_in addr, sysptr_t par)
			{
				auto* ptr = reinterpret_cast<lambda_arg*>(par);
				ptr->callback(addr, ptr->callback);
				rarchive_ncnt<result_type&, Arg...>::
					call(blk, result, arg...);
				delete ptr;
			}
		};

		auto* myarg = new lambda_arg;
		myarg->callback = callback;
		myarg->ptr = ptr;
		clt->rpc_generic(grp, addr, number, lambda::callback, myarg, arg...);
	}

	const sockaddr_in* addr;
	rpc_group_client* group;
	size_t number;
	rpc_client* client;
};

template<class R, class... Arg>
auto call_impl2(rpc_client& client,size_t number,R(*fn)(Arg...))
->client_call_ < R, Arg... >
{
	return client_call_<R, Arg...>(number,client);
}
#define RPC_CLIENT_STUB(func,stub_name,client)\
	auto stub_name=call_impl2(client,RPC_NUMBER_NAME(func)(),func);
/*
	template<class... Arg>\
	auto stub_name(Arg&... arg)\
		->decltype(func(arg...))\
		{\
		const_memory_block ret=\
			client.rpc_generic(nullptr,nullptr,\
				RPC_NUMBER_NAME(func)(),arg...);\
		typedef decltype(func(arg...)) result_type;\
		result_type func_result;\
		std::cout<<ret.size<<std::endl;\
		rarchive_ncnt<result_type&,Arg...>::rarchive_cnt(\
			ret, func_result, arg...);\
		return func_result;\
		}
*/
#define RPC_CLIENT_STUB_WITH_GROUP(func,stub_name,client)\
	template<class... Arg>\
	auto stub_name(rpc_group_client* grp,const sockaddr_in* addr, Arg... arg)\
		->decltype(func(arg...))\
		{\
		const_memory_block ret=\
			client.rpc_generic(grp,addr,\
				RPC_NUMBER_NAME(func)(),arg...);\
		typedef decltype(func(arg...)) result_type;\
		result_type func_result;\
		rarchive(ret.buffer,ret.size,func_result);\
		return func_result;\
		}	
#endif