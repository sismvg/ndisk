#ifndef ISU_SINGLE_ARCHIVE_DISPATH_HPP
#define ISU_SINGLE_ARCHIVE_DISPATH_HPP

//给size_wapper一次跳转调用的函数
//若要给对象自定义默认 archive,则必须生成这几个函数

#include <type_traits>

#include <archive_def.hpp>

template<class T>
size_t archive_to_with_size(memory_address buffer, size_t size,
	const T& value, size_t value_size)
{
	memcpy(buffer, std::addressof(value), value_size);
	return value_size;
}

template<class T>
size_t rarchive_with_size(const_memory_address buffer, size_t size,
	T& value, size_t value_size)
{
	memcpy(std::addressof(value), buffer, value_size);
	return value_size;
}

//size_wapper跳转

/*
优化效率的东西
*/

struct basic_size_wapper
{
	basic_size_wapper(size_t size)
		:_size(size)
	{}

	inline size_t size() const
	{
		return this->_size;
	}
private:
	size_t _size;
};

template<class T>
struct size_wapper
	:basic_size_wapper
{
	size_wapper(T& value, size_t size)
		:basic_size_wapper(size), _value(&value)
	{}

	T& value()
	{
		return *_value;
	}
	T& value() const
	{
		return *_value;
	}

private:
	T* _value;
};

template<class T>
size_t archived_size(const size_wapper<T>& value)
{
	return value.size();
}

template<class T>
size_t archive_to_with_size(memory_address buffer, size_t size,
	const size_wapper<T>& value, size_t value_size)
{
	return archive_to_with_size(buffer, size,
		value.value(), value_size);
}

template<class T>
size_t rarchive_with_size(const_memory_address buffer, size_t size,
	size_wapper<T>& value, size_t value_size)
{
	return rarchive_with_size(buffer, size,
		value.value(), value_size);
}

memory_block default_alloc_archive_memory(size_t size);
void default_free_archive_memory(void*, size_t size);
void default_free_archive_memory(const_memory_block);

#endif