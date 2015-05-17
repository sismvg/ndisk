#ifndef ISU_COPYABLE_HPP
#define ISU_COPYABLE_HPP

#include <archive.hpp>
#include <rpc_archive_def.hpp>

template<class T>
struct nop_cnt_impl
{
	static size_t size(T val)
	{
		return size_impl(typename is_no_need_copy<T>::type(), val);
	}

	static size_t size_impl(std::true_type,T val)
	{
		return 0;
	}

	static size_t size_impl(std::false_type, T val)
	{
		typedef typename
			std::remove_pointer <
			typename std::remove_reference<T>::type > ::type type;
		return sizeof(type);
	}
};
template<class... Arg>
struct nop_cnt
{
	static size_t size(Arg... arg)
	{
		return nop_cnt_impl<Arg...>::size(arg...);
	}
};

template<class T, class... Arg>
struct archive_size_ncnt_impl
{
	static size_t size(T val, Arg... arg)
	{
		typedef typename MPL_IF <
			std::integral_constant<bool, sizeof...(Arg) == 1>,
			nop_cnt<Arg...>,
			archive_size_ncnt_impl < Arg... >> ::type type;
		return nop_cnt<T>::size(val) +
			type::size(arg...);
	}
};

template<class... Arg>
struct archive_size_ncnt
{
	static size_t size(Arg... arg)
	{
		return archive_size_ncnt_impl<Arg...>::size(arg...);
	}
};


template<class T>
struct nop_cnt_arc_impl
{
	static size_t archive_to_cnt(memory_block blk,T val)
	{
		return archive_to_cnt_impl(typename is_no_need_copy<T>::type(),
			blk, val);
	}

	static size_t archive_to_cnt_impl(std::true_type,memory_block blk, T val)
	{
		return 0;
	}

	static size_t archive_to_cnt_impl(std::false_type, memory_block blk, T val)
	{//要么是ref,要么是ptr
		return _impl(blk, typename std::is_pointer<T>::type(), val);
	}

	static size_t _impl(memory_block blk, std::true_type, T val)
	{
		size_t adv= archive_to(blk.buffer, blk.size, *val);
		return adv;
	}

	static size_t _impl(memory_block blk, std::false_type, T val)
	{
		return archive_to(blk.buffer, blk.size, val);
	}
};

template<class... Arg>
struct nop_cnt_arc
{
	static size_t archive_to_cnt(memory_block blk, Arg... arg)
	{
		return nop_cnt_arc_impl<Arg...>::archive_to_cnt(blk, arg...);
	}
};

template<class T,class... Arg>
struct archive_cnt_impl
{
	static size_t archive_to_cnt(memory_block blk,T val, Arg... arg)
	{
		typedef typename MPL_IF <
			std::integral_constant<bool, sizeof...(Arg) == 1>,
			nop_cnt_arc<Arg...>,
			archive_cnt_impl < Arg... >> ::type type;
		size_t adv = nop_cnt_arc<T>::archive_to_cnt(blk, val);
		advance_in(blk, adv);
		return adv + type::archive_to_cnt(blk, arg...);
	}
};

template<class... Arg>
struct archive_ncnt
{
	static memory_block archive_cnt(Arg... arg)
	{
		size_t size = archive_size_ncnt<Arg...>::size(arg...);
		memory_block blk;
		blk.buffer = new char[size]; blk.size = size;

		archive_cnt_impl<Arg...>::archive_to_cnt(blk, arg...);
		return blk;
	}
};

#endif
