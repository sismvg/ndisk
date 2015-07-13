#ifndef ISU_RFC_1832_XDR_HPP
#define ISU_RFC_1832_XDR_HPP

#include <array>
#include <cstddef>

#include <meta_archive.hpp>
#include <shared_memory_block.hpp>
#include <memory_block_utility.hpp>

#include <xdr_convert.hpp>
#include <xdr_fundamental_wrap.hpp>
#include <xdr_type_define.hpp>

struct platform_is_bigendian
{
	typedef std::false_type type;
	static const bool value = false;
};

struct xdr_order_is_bigendian
{
	typedef std::true_type type;
	static const bool value = true;
};

//���ڴ水��xdr��׼�ڴ�����

inline void reserve_block_data(const memory_block& data)
{
	typedef char bit;
	bit* begin = reinterpret_cast<bit*>(data.buffer);
	std::reverse(begin, begin + data.size);
}

inline void local_to_xdr(const memory_block& data)
{
	if (!platform_is_bigendian::value)
	{
		reserve_block_data(data);
	}
}

inline void xdr_to_local(const memory_block& data)
{
	//��������һ���Ĵ���..
	local_to_xdr(data);
}

inline void to_xdr_order(const const_memory_block& from, const memory_block& to)
{
	//ֻ�����ֽ���
	if (!platform_is_bigendian::value)
	{
		deep_copy_to(to, from);
		xdr_to_local(to);
	}
}

inline void from_xdr_order(const const_memory_block& from, const memory_block& to)
{
//û����
	to_xdr_order(from, to);
}

inline size_t xdr_align(size_t size)
{
	return ((size + XDR_MININUM_ALIGN - 1) & (~(XDR_MININUM_ALIGN - 1)));
}

template<class T>
using xdr_type = xdr_fundamental_wrap < T > ;

typedef xdr_type<int> xdr_int;
typedef xdr_type<unsigned int> xdr_uint;

typedef xdr_type<char> xdr_char;
typedef xdr_type<unsigned char> xdr_uchar;

typedef xdr_type<wchar_t> xdr_wchar_t;

typedef xdr_type<int> xdr_int;
typedef xdr_type<unsigned int> xdr_uint;

typedef xdr_type<long> xdr_long;
typedef xdr_type<unsigned long> xdr_ulong;

typedef xdr_type<short> xdr_short;
typedef xdr_type<unsigned short> xdr_ushort;

typedef xdr_type<long long> xdr_hyper;
typedef xdr_type<unsigned long long> xdr_uhyper;

typedef xdr_type<int32_t> xdr_int32;
typedef xdr_type<uint32_t> xdr_uint32;

typedef xdr_hyper xdr_long64;
typedef xdr_uhyper xdr_ulong64;

typedef xdr_type<float> xdr_float;
typedef xdr_type<double> xdr_double;

template<class T>
using xdr_enum = xdr_type < T > ;

//���������ݰ���xdr��ʽ�������
//����Ĳ�������֧��xdr_type or xdr_class_type or xdr_complex_type.

//С����С�����ֽڵ�������û�ж���xdr_archive��,��������ŵĶ����ֽ����ݴ���
//����ʹ��Ԥ�����xdr_archive


template<class T>
struct xdr_filter_wapper
{
	xdr_filter_wapper(T& value)
		:_value(&value)
	{}

	T* _value;//well.. decltype need that

	auto operator*()
		->decltype(xdr_convert(*_value))
	{
		return xdr_convert(*_value);
	}

	auto operator*() const
		->decltype(xdr_convert(*_value))
	{
		return xdr_convert(*_value);
	}
	
};

struct xdr_filter
{
	SERALIZE_OBJECT_TEPMLATE
	static std::pair<bool,xdr_filter_wapper<T>> archive_filter(T& value)
	{
		return std::make_pair(true, xdr_filter_wapper<T>(value));
	}

	SERALIZE_OBJECT_TEPMLATE
	static std::pair<bool, xdr_filter_wapper<T>> rarchive_filter(T& value)
	{
		return std::make_pair(true, xdr_filter_wapper<T>(value));
	}
};

//��������͸���ṹ
typedef xdr_type<std::size_t> xdr_size_t;
typedef char __xdr_real_bit;
typedef shared_memory_block<__xdr_real_bit> xdr_opaque;
typedef shared_memory_block<const __xdr_real_bit> const_xdr_opaque;

template<>
struct is_fixed_length_data<xdr_opaque>
{
	static const bool value = false;
	typedef std::false_type type;
};

template<>
struct is_fixed_length_data < const_xdr_opaque >
{
	static const bool value = false;
	typedef std::false_type type;
};

inline std::size_t archived_size(const const_xdr_opaque& data)
{
	return data.size();
}

inline std::size_t archive_to_with_size(memory_address buffer, size_t bufsize,
	const const_xdr_opaque& value, size_t value_size)
{
	memcpy(buffer, value.get(), value_size);
	return value_size;
}

inline std::size_t rarchive_with_size(const_memory_address buffer, size_t bufsize,
	const_xdr_opaque& value, size_t value_size)
{
	__xdr_real_bit* ptr = new __xdr_real_bit[value_size];
	memcpy(ptr, buffer, value_size);
	value = const_xdr_opaque(memory_block(ptr, value_size));
	return value_size;
}

inline std::size_t archived_size(const xdr_opaque& value)
{
	return archived_size(static_cast<const const_xdr_opaque&>(value));
}

inline std::size_t archive_to_with_size(memory_address buffer, size_t bufsize,
	const xdr_opaque& value, size_t value_size)
{
	return archive_to_with_size(buffer, bufsize,
		static_cast<const const_xdr_opaque&>(value), value_size);
}

inline std::size_t rarchive_with_size(const_memory_address buffer, size_t bufsize,
	xdr_opaque& value, size_t value_size)
{
	//WAR-IMPLE.�������һ��.����const_xdr_opaqueû��ת������const�汾
	__xdr_real_bit* ptr = new __xdr_real_bit[value_size];
	memcpy(ptr, buffer, value_size);
	value = xdr_opaque(memory_block(ptr, value_size));
	return value_size;
}

template<std::size_t N>
using xdr_fixed_length_opaque= std::array <__xdr_real_bit, N > ;

//xdr_����

template<class T,std::size_t N>
using xdr_array = std::array < T, N > ;
#endif