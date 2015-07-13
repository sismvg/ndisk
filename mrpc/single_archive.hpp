#ifndef ISU_SINGLE_ARCHIVE_HPP
#define ISU_SINGLE_ARCHIVE_HPP

#include <archive_def.hpp>
#include <single_archive_dispath.hpp>
//��������/�����л���������
//�û�����������Щ����
//Ĭ�ϵ�serzalie_objectҲ���õ���Щ����

/*
	��ȡ�������л��Ժ���ڴ�ռ�ô�С
*/

/*��archvie_def���Ѿ�������
template<class T>
size_t archived_size(const T& value)
{
	return sizeof(T);
}*/

/*
	���л�һ���ڴ�,������һ�����ݿ�.
	�����ݿ�ʹ��default_free_archive_memory�ͷ�
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
	���������л���ָ�����ڴ浱��.������ռ�õ��ڴ�����
*/
template<class T>
size_t archive_to(memory_address buffer, size_t bufsize, const T& value)
{	
	return archive_to_with_size(buffer, bufsize,
		value, archived_size(value));
}

/*
	ͬ��,ֻ����������memory_block�ĳ���
*/
template<class T>
size_t archive_to(const memory_block& buffer, const T& value)
{
	return archive_to(buffer.buffer, buffer.size, value);
}

/*
	������ת���ɶ���,������ʹ�õ����ڴ�
*/
template<class T>
size_t rarchive(const_memory_address buffer, size_t bufsize, T& value)
{
	return rarchive_with_size(buffer, 
		bufsize, value, archived_size(value));
}

/*
	ͬ��,ֻ����������memory_block�ĳ���
*/
template<class T>
size_t rarchive(const const_memory_block& buffer, T& value)
{
	return rarchive(buffer.buffer, buffer.size, value);
}

#endif