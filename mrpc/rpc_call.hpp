#ifndef ISU_RPC_CALL_HPP
#define ISU_RPC_CALL_HPP

#include <rpcdef.hpp>
#include <rpc_group_server.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/push_back.hpp>

#include <rpc_archive.hpp>
#define MYARG const rpc_head& head,rpc_request_msg msg,remote_produce_group& grp
#define MYARG_USE head,msg,grp

template<class T>
struct call_impl_wapper
{
	typedef T type;
	typedef type raw_type;
	typedef T real_type;
	raw_type& raw()
	{
		return value();
	}
	type& value()
	{
		return _val;
	}

	type _val;
};

template<class T>
struct call_impl_wapper < T* >
{
	typedef T type;
	typedef T* raw_type;
	typedef T* real_type;
	raw_type raw()
	{
		return &value();
	}

	type& value()
	{
		return _val;
	}

	type _val;
};

template<class T>
struct call_impl_wapper < T& >
{
	typedef T type;
	typedef T& real_type;
	typedef T raw_type;
	raw_type& raw()
	{
		return value();
	}

	type& value()
	{
		return _val;
	}
	T _val;
};
#define MPL_VEC boost::mpl::vector
#define MPL_BPL boost::mpl::push_back
struct nop_block
{

};

template<class T>
struct mywapper
{
	mywapper(T& val)
		:_ptr(&val)
	{}
	operator T()
	{
		return *ptr;
	}
	T* _ptr;
};

template<class T>
struct mywapper<T*>
{
	mywapper(T* val)
		:_ptr(val)
	{}
	operator T&()
	{
		return _ptr;
	}
	T* _ptr;
};

template<class T>
struct mywapper < T& >
{
	mywapper(T& val)
		:_ptr(&val)
	{}

};
/*
template<class Func,class T,class... Sem>
auto _fitting_call_impl(Func fn, T val, Sem... sem)
->decltype(fn(val,sem...))
{
	return _fitting_call_impl(std::bind(fn, mywapper<T>(val)), sem...);
}

template<class Func,class... Sem>
auto _fitting_call(Func fn,Sem... sem)
->decltype(fn(sem...))
{//拟合所有可转换的类型，允许指针到引用
	//不允许空指针(空ref)

}

template<class Func, class... Sem>
size_t rpc_rarchive_call_impl(const void* buf, unsigned long long size,
	MYARG,Func fn, Sem... sem)
{
	auto ret = fn(sem...);
	typedef typename real_type<decltype(ret)>::type type;
	auto blk = archive_ncnt<type&, Sem...>::archive_cnt(ret, sem...);
	grp.push_mission(head,msg, blk);
	return archived_size(sem...);
}

template<class T, class... Arg, class Func, class... Sem>
size_t rpc_rarchive_call_impl(const void* buf, unsigned long long size, 
	MYARG,Func fn, Sem... sem)
{
	call_impl_wapper<T> wapper;
	typedef typename call_impl_wapper<T>::type value_type;
	rarchive(buf, size, wapper.value());
	const char* cbuf = reinterpret_cast<const char*>(buf);
	cbuf += sizeof(value_type);
	size -= sizeof(value_type);
	return rpc_rarchive_call_impl<Arg...>(cbuf, size,
		MYARG_USE, fn, sem..., wapper.raw());
}

template<class R, class... Arg>
size_t rpc_rarchive_call(const void* buf, unsigned long long size,
	MYARG,R(*fn)(Arg...))
{
	return rpc_rarchive_call_impl<Arg...>(buf, size, MYARG_USE,fn);
}*/


template<class... Sem>
struct rpc_rarchive_call_impl
{
	template<class T,class... Arg,class Func>
	static size_t call(const void* buf, size_t size,
		MYARG, Func fn, Sem... sem)
	{
		call_impl_wapper<T> wap;
		size_t adv = rarchive(buf, size, wap.value());
		advance_in(buf, size, adv);
		typedef MPL_IF < std::integral_constant<bool, sizeof...(Arg) == 0>,
			rpc_rarchive_call_end<Sem...,T>,
			rpc_rarchive_call_impl < Sem..., T>> ::type type1;
		return type1::call<Arg...>(buf, size, MYARG_USE, fn, sem..., wap.raw());
	}

};

template<class... Sem>
struct rpc_rarchive_call_end
{
	template<class Func>
	static size_t call(const void* buf, size_t size,
		MYARG, Func fn, Sem... sem)
	{
		auto ret = fn(sem...);
		typedef typename real_type<decltype(ret)>::type type;
		const_memory_block blk = archive_ncnt<type&, Sem...>::archive_cnt(ret, sem...);
		vector_air ph;
		grp.push_mission(head, msg, ph, blk.size, reinterpret_cast<const char*>(blk.buffer));
		return archived_size(sem...);
	} 
};

template<class R,class... Arg>
struct rpc_rarchive_call
{
	static size_t call(const void* buf, size_t size,
		MYARG, R(*fn)(Arg...))
	{
		return rpc_rarchive_call_impl<>::
			call<Arg...>(buf, size, MYARG_USE,fn);
	}
};
#endif
