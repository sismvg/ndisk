#ifndef MEMORY_BLOCK_UTILITY_HPP
#define MEMORY_BLOCK_UTILITY_HPP

#include <archive_def.hpp>

template<class T>
memory_block make_block(T& val)
{
	return memory_block(&val, sizeof(val));
}

template<class T>
const_memory_block make_block(const T& val)
{
	return const_memory_block(&val, sizeof(val));
}

inline void memcpy_safe_and_adv(
	memory_block& dest, const const_memory_block& src)
{
	memcpy(dest.buffer, src.buffer, src.size);
	advance_in(dest, src.size);
}

template<class T>
memory_block deep_copy(const T& obj)
{
	memory_block result;
	result.buffer = new char[obj.size];
	memcpy(result.buffer, obj.buffer, obj.size);
	result.size = obj.size;
	return result;
}

template<class T, class T2>
void deep_copy_to(T& dest, const T2& src)
{
	if (dest.size < src.size)
		throw std::out_of_range("deep_copy_to out of range");
	memcpy(dest.buffer, src.buffer, src.size);
}

template<class T>
void archive_without_size(void* buffer, size_t bufsize, const T& block)
{
	memory_block dest_block(buffer, bufsize);
	deep_copy_to(dest_block, block);
}

#endif