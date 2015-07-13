#ifndef ISU_ARCHIVE_HPP
#define ISU_ARCHIVE_HPP

#include <archive_size.hpp>
#include <serialize.hpp>

template<class... Arg>
size_t archived_size(const Arg&... arg)
{
	serialize<> core;
	return core.archived_size(arg...);
}

template<class... Arg>
memory_block archive(const Arg&... arg)
{
	serialize<> core;
	return core.archive(arg...);
}

template<class... Arg>
size_t archive_to(memory_address buffer, size_t bufsize, const Arg&... arg)
{
	serialize<> core;
	return core.archive_to(buffer, bufsize, arg...);
}

template<class... Arg>
size_t archive_to(memory_block block, const Arg&... arg)
{
	return archive_to(block.buffer, block.size, arg...);
}

template<class... Arg>
size_t rarchive(const_memory_address buffer, size_t bufsize, Arg&... arg)
{
	serialize<> core;
	return core.rarchive(buffer, bufsize, arg...);
}

template<class... Arg>
size_t rarchive_from(const_memory_address buffer, size_t bufsize, Arg&... arg)
{
	return rarchive(buffer, bufsize, arg...);
}

template<class... Arg>
size_t rarchive(const_memory_block block, Arg&... arg)
{
	return rarchive(block.buffer, block.size, arg...);
}
/*
template<class T1, class T2, class... Arg>
size_t rarchive_impl_air(vector_air air, const void* buf,
	std::size_t buf_size, T1& count, T2 iter, Arg&... arg)
{
	//iter_wapper是为了纠正 type[size]这样的数组传入的时候不被识别为指针的问题
	size_t presize = buf_size;
	ITER_VALUE_TYPE(T2);
	auto value_size = archived_size<value_type>();

	size_t sizeof_cnt = incerment<1>(buf,
		rarchive_from(buf, buf_size,count));
	buf_size -= sizeof_cnt;

	auto tmp_count = count;
	while (tmp_count--)
	{
		value_type tmp;
		size_t incsize = incerment<1>(buf,
			rarchive_from(buf, buf_size, tmp));
		if (incsize > buf_size)
			return -1;
		buf_size -= incsize;
		(*iter) = tmp;
		++iter;
	}
	if (tmp_count != -1)//表明数据量不够
	{
		//TODO:memory leak error
		throw std::runtime_error("memory out");
	}
	return presize - buf_size + rarchive_impl(buf, buf_size, arg...);
}

inline size_t rarchive_impl(const void* buf, std::size_t buf_size)
{
	return 0;
}

template<class... Arg>
size_t rarchive_impl(const void* buf, std::size_t buf_size, 
	vector_air air, Arg&... arg)
{
	return rarchive_impl_air(air, buf, buf_size, arg...);
}

template<class T, class... Arg>
size_t rarchive_impl(const void* buf, std::size_t buf_size, T& a1, Arg&... arg)
{
	size_t a1_size = 
		incerment<1>(buf, rarchive_from(buf, buf_size, a1));
	return a1_size+rarchive_impl(buf, buf_size - a1_size, arg...);
}

template<class... Arg>
size_t rarchive(const_memory_block blk, Arg&... arg)
{
	return rarchive(blk.buffer, blk.size, arg...);
}

template<class... Arg>
size_t rarchive(const void* buf, std::size_t buf_size, Arg&... arg)
{
	return rarchive_impl(buf, buf_size, arg...);
}

template<class... Arg>
memory_block archive(const Arg&... arg)
{
	std::size_t buf_size = archived_size(arg...);
	char* buf = new char[buf_size];
	archive_to(buf, buf_size, arg...);
	memory_block ret;
	ret.buffer = buf;
	ret.size = buf_size;
	return ret;
}

inline std::size_t archive_to_impl(void* buf, std::size_t buf_size)
{
	return 0;
}

template<class T,class T2,class... Arg>
std::size_t archive_to_impl(void* buf, std::size_t buf_size,
	const vector_air& air, const T& count, T2 iter, Arg... arg)
{
	auto tmp_count = count;
	size_t sizeof_cnt = incerment<1>(buf, archive_to(buf, buf_size, count));

	size_t copyed_size = 0;
	while (tmp_count--)
	{
		size_t used= incerment<1>(buf,
			archive_to(buf, buf_size, (*iter++) ) );
		copyed_size += used;
		buf_size -= used;
	}
	//没有copy完也不会出错
	return copyed_size + sizeof_cnt +
		archive_to_impl(buf, buf_size, arg...);
}
/*
template<class T>
std::size_t archive_to(void* buffer, std::size_t bufsize, const T& value)
{
	return archive_to_and_size(buffer, bufsize, value);
}*/

/*
template<class... Arg>
std::size_t archive_to(void* buffer, std::size_t bufsize, const Arg&... arg)
{
	return archive_to_impl(buffer, bufsize, arg...);
}

template<class T,class... Arg>
std::size_t archive_to_impl(void* buf, std::size_t buf_size, 
	const T& a1,const Arg&... arg)
{
	size_t result = archive_to(buf, buf_size, a1);
	size_t incer_size = incerment<1>(buf, result);
	result += archive_to_impl(buf, buf_size - incer_size, arg...);
	return result;
}

//为单一参数设定，用来自定义archive
template<class T>
size_t rarchive_from(const void* buffer, size_t size,
		T& value, size_t archlimit = -1)//最后一个参数可..保留好了
{
	//会无视const
	size_t arched_size = archived_size(value);
	memcpy(
		const_cast<void*>(
		reinterpret_cast<const void*>(&value)), buffer, arched_size);
	return arched_size;
}

template<class T>
size_t archive_to_and_size(memory_address buffer, size_t size,
	const T& value, size_t value_size = 0)
{
	size_t archsize = value_size == 0 ?
		archived_size(value) : value_size;

	memcpy(buffer, reinterpret_cast<const void*>(&value), archsize);
	return archsize;
}*/
/*
template<class T>
memory_block archive(const T& value)
{
	memory_block blk;
	blk.size = archived_size(value);
	blk.buffer = new char[blk.size];
	archive_to_and_size(blk.buffer, blk.size, value, blk.size);
	return blk;
}*/
#endif
