#ifndef ISU_RPC_CALL_HPP
#define ISU_RPC_CALL_HPP

template<class Func, class... Sem>
auto rpc_rarchive_call_impl(void* buf, unsigned long long size, Func fn, Sem... sem)
->decltype(fn(sem...))
{
	return fn(sem...);
}

template<class T, class... Arg, class Func, class... Sem>
auto rpc_rarchive_call_impl(void* buf, unsigned long long size, Func fn, Sem... sem)
->decltype(rpc_rarchive_call_impl<Arg...>(buf, size, fn, sem..., T()))
{
	T var;
	rarchive(buf, size, var);
	char* cbuf = reinterpret_cast<char*>(buf);
	cbuf += sizeof(T);
	size -= sizeof(T);
	return rpc_rarchive_call_impl<Arg...>(cbuf, size, fn, sem..., var);
}

template<class R, class... Arg>
auto rpc_rarchive_call(void* buf, unsigned long long size, R(*fn)(Arg...))
->decltype(rpc_rarchive_call_impl<Arg...>(buf, size, fn))
{
	return rpc_rarchive_call_impl<Arg...>(buf, size, fn);
}

#endif
