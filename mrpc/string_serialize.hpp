#ifndef ISU_STRING_SERIALIZE_HPP
#define ISU_STRING_SERIALIZE_HPP

#include <string>
#include <single_archive.hpp>

//暂不支持自定义char_traits
inline size_t archived_size(const std::basic_string<char>& string)
{
	return string.size()*sizeof(char);
}

inline size_t archive_to_with_size(memory_address buffer, size_t size,
	const std::basic_string<char>& string, size_t value_size)
{//warning value_size永远指的是bit
	if (value_size == 0)
		return 0;
	memcpy(buffer, std::addressof(*string.begin()), value_size);
	return value_size;
}

inline size_t rarchive_with_size(const_memory_address buffer, size_t size,
	std::basic_string<char>& string, size_t value_size)
{
	typedef char T;
	const T* begin = reinterpret_cast<const T*>(buffer);
	const T* end = begin + value_size;
	if (string.capacity() >= value_size)
	{//嗯..只能期望优化了
		std::copy(begin, begin + string.size(), string.begin());
		std::copy(begin + string.size(),
			end, std::back_inserter(string));
	}
	else
	{//原来的string如果push_back一定会引发realloc,so, make new string
		string = std::basic_string<T>(begin, end);
	}
	return value_size;
}
#endif
