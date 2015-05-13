#ifndef ISU_RPC_CALL_HPP
#define ISU_RPC_CALL_HPP

#include <rpcdef.hpp>
#include <rpc_group_server.hpp>

#define MYARG const rpc_head& head,rpc_request_msg msg,rpc_group_server& grp
#define MYARG_USE head,msg,grp

template<class Func, class... Sem>
size_t rpc_rarchive_call_impl(const void* buf, unsigned long long size,
	MYARG,Func fn, Sem... sem)
{
	auto ret = fn(sem...);
	grp.push_mission(head,msg, ret);
	return archived_size(sem...);
}

template<class T, class... Arg, class Func, class... Sem>
size_t rpc_rarchive_call_impl(const void* buf, unsigned long long size, 
	MYARG,Func fn, Sem... sem)
{
	T var;
	rarchive(buf, size, var);
	const char* cbuf = reinterpret_cast<const char*>(buf);
	cbuf += sizeof(T);
	size -= sizeof(T);
	return rpc_rarchive_call_impl<Arg...>(cbuf, size, MYARG_USE,fn, sem..., var);
}

template<class R, class... Arg>
size_t rpc_rarchive_call(const void* buf, unsigned long long size,
	MYARG,R(*fn)(Arg...))
{
	return rpc_rarchive_call_impl<Arg...>(buf, size, MYARG_USE,fn);
}
#endif
