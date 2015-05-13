#ifndef ISU_ARCHIVE_DEF_HPP
#define ISU_ARCHIVE_DEF_HPP

#include <utility>
#include <xstddef>
#include <iterator>
#include <type_traits>

#include <boost/mpl/if.hpp>

extern int ptrv;
template<class T>
struct real_type
{
	typedef typename std::remove_reference<T>::type type;
};

template<class T>
class iter_wapper
{
public:
	typedef T type;
};

template<class T, std::size_t Size>
class iter_wapper < T[Size] >
{
public:
	typedef T* type;
};

template<class T>
class iterator_value_type
{
public:
	typedef typename
		std::iterator_traits<T>::value_type type;
};

template<template<class> class WIter, class Con>
class iterator_value_type < WIter<Con> >
{
public:
	typedef typename Con::value_type type;
};


class vector_air{};

template<class T>
class is_air
{
public:
	static const bool value = std::is_same<T, vector_air>::value;
};

typedef std::size_t size_t;
typedef void* memory_address;
typedef const void* const_memory_address;

struct memory_block
{
	memory_block()
		:buffer(nullptr), size(0)
	{}

	memory_block(memory_address addr, size_t sz)
		:buffer(addr), size(sz)
	{}

	memory_address buffer;
	size_t size;
};

struct const_memory_block
{
	const_memory_block()
		:buffer(nullptr),size(0)
	{}

	const_memory_block(const_memory_address addr, size_t sz)
		:buffer(addr), size(sz)
	{}

	const_memory_block(const memory_block& blk)
		:buffer(blk.buffer), size(blk.size)
	{}

	const_memory_address buffer;
	size_t size;
};

typedef long long long64;

template<class MemBlk>
MemBlk advance(const MemBlk& blk, size_t adv)
{
	MemBlk ret = blk;
	advance_in(ret, adv);
	return ret;
}

template<class MemBlk>
void advance_in(MemBlk& blk, size_t adv)
{
	advance_in(blk.buffer, blk.size, adv);
}

#define ADVANCE_BLK(name) name##emory_block blk;\
	blk.buffer=buffer;\
	blk.size=size;\
	advance_in(blk,adv);\
	return blk

inline memory_block 
	advance(void* buffer, size_t size, size_t adv)
{
	ADVANCE_BLK(m);
}

inline const_memory_block 
	advance(const void* buffer, size_t size, size_t adv)
{
	ADVANCE_BLK(const_m);
}

template<class Void>//MSG:嗯...don't worry
inline void advance_in(Void*& buffer, size_t& size, size_t adv)
{//一律当成非const,当然内部不会修改他
	const auto* tmp = reinterpret_cast<const char*>(buffer);
	tmp += adv;
#ifdef _DEBUG
	if (adv > size)
		throw std::length_error("advance_in adv"\
								 "have to equal less to size");
#endif
	size -= adv;
	buffer = reinterpret_cast<Void*>(const_cast<char*>(tmp));
}

//鉴于这些东西的重载太多。。所以直接扔了个宏
#define MYIF boost::mpl::if_

template<size_t value>
using constant = std::integral_constant < size_t, value > ;

template<class T1,class T2>
using mysame = std::is_same < T1, T2 > ;

#define NOT_TYPE(tp) typename mysame<T, tp >::type;

template<class T>
struct is_noptype
{
	static const bool not_char = !mysame<T, char>::type::value,
		not_uchar = !mysame<T, unsigned char>::type::value,
		not_bool = !mysame<T, bool>::type::value,
		_1bit = mysame < constant<sizeof(T)>,
		constant < 1 >> ::type::value;

	static const bool value =
		not_char&&not_uchar&&not_bool&&_1bit;

	typedef std::integral_constant<bool, value> type;
};

template<class T>
struct is_multiple_type_impl
{
	static const bool value = false;
};


template<class T>
struct is_multiple_type
{
	static const bool value =
		is_multiple_type_impl<T>::value;
};

template<class T>
struct is_diamond_type
{
	static const bool value = false;
};

typedef constant<0> pod_type;
typedef constant<1> nop_type;
typedef constant<2> multiple_type;
typedef constant<3> diamond_type;
typedef constant<4> normal_type;

template<class T>
struct detail_of_type_impl
{
	//懒得写那么多层包装了，直接全部用myif写出来好了
	typedef typename
		MYIF<typename std::is_pod<T>::type,
			pod_type, normal_type>::type t1;

	typedef typename
		MYIF<typename is_noptype<T>::type,
			nop_type, t1>::type type;
};

template<class T>
struct detail_of_type
{//判断该类型的类型信息（POD啥的）
	typedef typename detail_of_type_impl<T>::type type;
};

template<class T>
struct constant_length
{
	typedef typename detail_of_type_impl<T>::type type_detail;
	static const size_t tmp = type_detail::value;
	static const bool value = (
		tmp == pod_type::value ||
		tmp == normal_type::value ||
		tmp == nop_type::value);
};

typedef std::true_type true_type;
typedef std::false_type false_type;

#ifndef NOP
#define NOP
#endif

#define CV_INCERMENT(cv)\
	cv char* cptr = reinterpret_cast<cv char*>(ptr);\
	size_t size = (BitCnt*loop_count);\
	cptr += size;\
	ptr=cptr;\
	return size

template<size_t BitCnt,class Ptr>
size_t incerment(Ptr*& ptr, size_t loop_count)
{
	CV_INCERMENT(NOP);
}

template<size_t BitCnt,class Ptr>
size_t incerment(const Ptr*& ptr, size_t loop_count)
{
	CV_INCERMENT(const);
}

#endif