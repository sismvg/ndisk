#ifndef ISU_RPC_STUB_DEFINE_HPP
#define ISU_RPC_STUB_DEFINE_HPP

#include <rpcdef.hpp>

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

#define RPC_SERVER_STUB(func,stub_name)\
	size_t stub_name(const_memory_block blk,const rpc_head& head,\
	rpc_request_msg msg,rpc_group_server& grp)\
	{\
		size_t adv=rpc_rarchive_call(blk.buffer,blk.size,\
		head,msg,grp,func);\
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



#define RPC_CLIENT_STUB(func,stub_name,client)\
	template<class... Arg>\
	auto stub_name(Arg... arg)\
		->decltype(func(arg...))\
				{\
		const_memory_block ret=\
			client.rpc_generic(nullptr,nullptr,\
				RPC_NUMBER_NAME(func)(),arg...);\
		typedef decltype(func(arg...)) result_type;\
		result_type func_result;\
		rarchive(ret.buffer,ret.size,func_result);\
		return func_result;\
				}

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