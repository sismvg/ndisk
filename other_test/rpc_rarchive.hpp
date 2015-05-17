#ifndef ISU_RPC_COPYABLE_RARCHIVE_HPP
#define ISU_RPC_COPYABLE_RARCHIVE_HPP
#include <rpc_archive_def.hpp>
#include <archive.hpp>
template<class T>
struct rnop_cnt_impl
{
	static size_t size(T val)
	{
		return size_impl(typename is_no_need_copy<T>::type(), val);
	}

	static size_t size_impl(std::true_type, T val)
	{
		return 0;
	}

	static size_t size_impl(std::false_type, T val)
	{
		typedef typename
			std::remove_pointer <
			typename std::remove_reference<T>::type > ::type type;
		std::cout << typeid(type).name() << std::endl;
		return sizeof(type);
	}
};
template<class... Arg>
struct rnop_cnt
{
	static size_t size(Arg... arg)
	{
		return rnop_cnt_impl<Arg...>::size(arg...);
	}
};

template<class T, class... Arg>
struct rarchive_size_ncnt_impl
{
	static size_t size(T val, Arg... arg)
	{
		typedef typename MPL_IF <
			std::integral_constant<bool, sizeof...(Arg) == 1>,
			nop_cnt<Arg...>,
			archive_size_ncnt_impl < Arg... >> ::type type;
		return rnop_cnt<T>::size(val) +
			type::size(arg...);
	}
};

template<class... Arg>
struct rarchive_size_ncnt
{
	static size_t size(Arg... arg)
	{
		return rarchive_size_ncnt_impl<Arg...>::size(arg...);
	}
};


template<class T>
struct rnop_cnt_arc_impl
{
	static size_t rarchive_to_cnt(const_memory_block blk, T val)
	{
		return rarchive_to_cnt_impl(typename is_no_need_copy<T>::type(),
			blk, val);
	}

	static size_t rarchive_to_cnt_impl(std::true_type, const_memory_block blk, T val)
	{
		return 0;
	}

	static size_t rarchive_to_cnt_impl(std::false_type, const_memory_block blk, T val)
	{//要么是ref,要么是ptr
		return _impl(blk, typename std::is_pointer<T>::type(), val);
	}

	static size_t _impl(const_memory_block blk, std::true_type, T val)
	{
		size_t adv = rarchive(blk.buffer, blk.size, *val);
		return adv;
	}

	static size_t _impl(const_memory_block blk, std::false_type, T val)
	{
	//	std::cout << "数据" << std::endl;
		return rarchive(blk.buffer, blk.size, val);
	}
};

template<class... Arg>
struct rnop_cnt_arc
{
	static size_t rarchive_to_cnt(const_memory_block blk, Arg... arg)
	{
		return rnop_cnt_arc_impl<Arg...>::rarchive_to_cnt(blk, arg...);
	}
};

template<class T, class... Arg>
struct rarchive_cnt_impl
{
	static size_t rarchive_to_cnt(const_memory_block blk, T val, Arg... arg)
	{
		typedef typename MPL_IF <
			std::integral_constant<bool, sizeof...(Arg) == 1>,
			rnop_cnt_arc<Arg...>,
			rarchive_cnt_impl < Arg... >> ::type type;
		size_t adv = rnop_cnt_arc<T>::rarchive_to_cnt(blk, val);
		advance_in(blk, adv);
		return adv + type::rarchive_to_cnt(blk, arg...);
	}
};

template<class... Arg>
struct rarchive_ncnt
{
	static size_t rarchive_cnt(const_memory_block blk, Arg... arg)
	{
		return rarchive_cnt_impl<Arg...>::rarchive_to_cnt(blk, arg...);
	}
};


#endif