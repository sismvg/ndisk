#ifndef ISU_SINGLE_ARCHIVE_HPP
#define ISU_SINGLE_ARCHIVE_HPP

#include <archive_def.hpp>
#include <single_archive_dispath.hpp>
//用来序列/反序列化单个对象
//用户可以重载这些函数
//默认的serzalie_object也会用到这些函数

/*
	获取对象序列化以后的内存占用大小
*/

/*在archvie_def中已经被定义
template<class T>
size_t archived_size(const T& value)
{
	return sizeof(T);
}*/

/*
	序列化一块内存,并返回一个数据块.
	该数据块使用default_free_archive_memory释放
*/
template<class T>
memory_block archive(const T& value)
{
	size_t size = archived_size(value);
	memory_block block = default_alloc_archive_memory(size);
	archive_to(block, size_wapper<const T>(value, size));
	return block;
}

/*
	将对象序列化到指定的内存当中.并返回占用的内存数量
*/
template<class T>
size_t archive_to(memory_address buffer, size_t bufsize, const T& value)
{	
	return archive_to_with_size(buffer, bufsize,
		value, archived_size(value));
}

/*
	同上,只不过加上了memory_block的抽象
*/
template<class T>
size_t archive_to(const memory_block& buffer, const T& value)
{
	return archive_to(buffer.buffer, buffer.size, value);
}

/*
	把数据转换成对象,并返回使用掉的内存
*/
template<class T>
size_t rarchive(const_memory_address buffer, size_t bufsize, T& value)
{
	return rarchive_with_size(buffer, 
		bufsize, value, archived_size(value));
}

/*
	同上,只不过加上了memory_block的抽象
*/
template<class T>
size_t rarchive(const const_memory_block& buffer, T& value)
{
	return rarchive(buffer.buffer, buffer.size, value);
}

#endif