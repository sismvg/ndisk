#ifndef ISU_ARCHIVE_PREDEFINE_HPP
#define ISU_ARCHIVE_PREDEFINE_HPP

#include <archive.hpp>

#include <iterator>
#include <iostream>
#include <set>
#include <map>
#include <list>
#include <vector>
#include <deque>
#include <queue>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <forward_list>
#include <array>
//这里有
//map,set,multimap,multiset
//vector,list,deque,array,forward_list
//stack,queue,priority_queue
//unordered_set&map&multiset&multimap

#define UNPACK(def) def
#define ARC_DEFAULT_SIZE =0
#define ARCHIVE_TO(name,tmp,tmpg) tmp size_t archive_to(memory_address buffer,\
	size_t size,const name UNPACK(tmpg)& value,size_t value_size ARC_DEFAULT_SIZE)

#define ARCHIVE(name,tmp,tmpg) tmp memory_block archive(const name UNPACK(tmpg)& value)
#define RARCHINE_FROM(name,tmp,tmpg) tmp size_t rarchive_from(const void* buffer,\
	size_t size,name UNPACK(tmpg)& value)
#define RARCHIVE(name,tmp,tmpg) tmp size_t rarchive(const void* buffer,\
	size_t size,name UNPACK(tmpg)& value)
#define ARCHIVED_SIZE(name,tmp,tmpg) tmp size_t \
	archived_size(const name UNPACK(tmpg)& value)
	
#define STA template<class T>
#define MTA template<class T1,class T2>
#define STAU <T>
#define MTAU <T1,T2>
/*
#define ARCHIVE_GROUP(name,tmp,tmpg)\
	ARCHIVED_SIZE(std::##name,tmp,tmpg);\
	ARCHIVE(std::##name,tmp,tmpg);\
	ARCHIVE_TO(std::##name,tmp,tmpg);\
	RARCHINE_FROM(std::##name,tmp,tmpg);\
	RARCHIVE(std::##name,tmp,tmpg)

ARCHIVE_GROUP(pair,MTA,MTAU);
ARCHIVE_GROUP(map,MTA,MTAU);
ARCHIVE_GROUP(set,STA,STAU);
ARCHIVE_GROUP(multimap,MTA,MTAU);
ARCHIVE_GROUP(multiset,MTA,MTAU);
ARCHIVE_GROUP(vector,STA,STAU);
ARCHIVE_GROUP(list,STA,STAU);
ARCHIVE_GROUP(deque,STA,STAU);

#define MTA_ARRAY template<class T1,size_t Size>
#define MTAU_ARRRAY <T1,Size>

ARCHIVE_GROUP(array, MTA_ARRAY, MTAU_ARRRAY);
ARCHIVE_GROUP(forward_list,STA,STAU);
ARCHIVE_GROUP(stack,STA,STAU);
ARCHIVE_GROUP(queue,STA,STAU);
ARCHIVE_GROUP(priority_queue,STA,STAU);
ARCHIVE_GROUP(unordered_map,MTA,MTAU);
ARCHIVE_GROUP(unordered_set,MTA,MTAU);
ARCHIVE_GROUP(unordered_multimap,MTA,MTAU);
ARCHIVE_GROUP(unordered_multiset,MTA,MTAU);
	
*/

template<class T>
size_t stl_archived_size_impl(const T& vec, std::true_type)
{
	typedef typename T::value_type value_type;
	return vec.size()*archived_size<value_type>() + sizeof(size_t);
}

template<class T>
size_t stl_archived_size_impl(const T& vec, std::false_type)
{
	size_t result = 0;
	typedef typename T::value_type value_type;
	std::for_each(vec.begin(), vec.end(),
		[&](const value_type& val)
	{
		result += archived_size(val);
	});
	return result + sizeof(size_t);
}

template<class T>
size_t stl_archived_size(const T& vec)
{
	//Must be support interface:size,begin,end
	//Must be support typedef:value_type
	return stl_archived_size_impl(vec,
		std::integral_constant<bool, constant_length<T>::value>());
}

template<class T>
size_t stl_archive_to(void* buffer, size_t size, const T& vec)
{
	typedef typename T::value_type value_type;
	size_t result = 0;
	size_t s0 = incerment<1>(buffer,
		archive_to(buffer, size, vec.size()));
	size -= s0;
	std::for_each(vec.begin(), vec.end(),
		[&](const value_type& val)
	{
		size_t s1 = incerment<1>(buffer, archive_to(buffer, size, val));
		size -= s1;
		result += s1;
	});
	return result + s0;
}

template<class T>
size_t stl_rarchive_from_back_insert_iterator(
	const void* buffer, size_t size, T& vec)
{
	size_t count = 0;
	auto iter = std::back_insert_iterator<T>(vec);
	vector_air ph;
	return rarchive(buffer, size, ph, count, iter);
}

template<class T>
size_t stl_rarchive_from_insert_iterator(
	const void* buffer, size_t size, T& vec)
{
	size_t count = 0;
	auto iter = std::insert_iterator<T>(vec, vec.begin());
	vector_air ph;
	return rarchive(buffer, size, ph, count, iter);
}

#define INSERT_ITER insert_iterator
#define PUSH_ITER back_insert_iterator

#define MAKE_NAME(n1,n2) n1##n2

#define STL_GROUP_REALIZE(name,tmp,tmpg,iter)\
	ARCHIVED_SIZE(std::##name,tmp,tmpg)\
	{\
		return stl_archived_size(value);\
	}\
	ARCHIVE_TO(std::##name,tmp,tmpg)\
	{\
		return stl_archive_to(buffer,size,value);\
	}\
	ARCHIVE(std::##name,tmp,tmpg)\
	{\
		memory_block ret;\
		ret.size=archived_size(value);\
		ret.buffer=new char[ret.size];\
		archive_to(ret.buffer,ret.size,value);\
		return ret;\
	}\
	RARCHINE_FROM(std::##name,tmp,tmpg)\
	{\
		size_t count=0;\
		auto ret= MAKE_NAME(stl_rarchive_from_,iter) (buffer,size,value);\
		return ret;\
	}\
	RARCHIVE(std::##name,tmp,tmpg)\
	{\
		return rarchive_from(buffer,size,value);\
	}


//pair ,stack 要自己来
ARCHIVED_SIZE(std::pair, MTA, MTAU)
{
	return archived_size(value.first) + archived_size(value.second);
}

ARCHIVE_TO(std::pair, MTA, MTAU)
{
	return archive_to(buffer, size, value.first, value.second);
}

ARCHIVE(std::pair, MTA, MTAU)
{
	return archive_to(buffer, size, value);
}

RARCHINE_FROM(std::pair, MTA, MTAU)
{
	return rarchive_from(buffer, size, value.first, value.second);
}

RARCHIVE(std::pair, MTA, MTAU)
{
	return rarchive_from(buffer, size, value);
}

STL_GROUP_REALIZE(map, MTA, MTAU, INSERT_ITER);
STL_GROUP_REALIZE(set, STA, STAU, INSERT_ITER);
STL_GROUP_REALIZE(multimap, MTA, MTAU, INSERT_ITER);
STL_GROUP_REALIZE(multiset, MTA, MTAU, INSERT_ITER);
STL_GROUP_REALIZE(vector, STA, STAU, PUSH_ITER);
STL_GROUP_REALIZE(list, STA, STAU, PUSH_ITER);
STL_GROUP_REALIZE(deque, STA, STAU, PUSH_ITER);

#define MTA_ARRAY template<class T1,size_t Size>
#define MTAU_ARRRAY <T1,Size>

STL_GROUP_REALIZE(array, MTA_ARRAY, MTAU_ARRRAY, PUSH_ITER);
STL_GROUP_REALIZE(forward_list, STA, STAU, PUSH_ITER);
STL_GROUP_REALIZE(queue, STA, STAU, PUSH_ITER);
STL_GROUP_REALIZE(priority_queue, STA, STAU, PUSH_ITER);
STL_GROUP_REALIZE(unordered_map, MTA, MTAU, INSERT_ITER);
STL_GROUP_REALIZE(unordered_set, MTA, MTAU, INSERT_ITER);
STL_GROUP_REALIZE(unordered_multimap, MTA, MTAU, INSERT_ITER);
STL_GROUP_REALIZE(unordered_multiset, MTA, MTAU, INSERT_ITER);
#endif