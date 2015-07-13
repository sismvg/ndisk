#ifndef ISU_META_ARCHIVE_HPP
#define ISU_META_ARCHIVE_HPP

#include <vector>

#include <single_archive.hpp>

#include <type_list.hpp>

/*
	元流程控制模块
	用来控制编译过程
*/

struct meta_process_tag
{//一个tag类,所有的元流程控制对象都必须从这里继承
public:
};

//判断是否是元过程
template<class T>
struct is_meta_process_object
{
	typedef std::is_base_of<meta_process_tag, T> fn;
	typedef typename fn::type type;
	static const bool value = fn::value;
};

//减少代码量的宏

#define META_PROCESS_TEMPLATE_START\
 	template<\
	class PreMetaProcess,\
	class... RealArg,\
	class T, class... PassedArg>

#define META_PROCESS_TEMPLATE_END\
	template<class PreMetaProcess>

#define META_PROCESS_TEMPLATE\
	template<\
	class PreMetaProcess,\
	class RealT,class... RealArg,\
	class T, class... PassedArg>

#define ARCHIVED_SIZE_TRUE_IMPL size_t archived_size_impl(PreMetaProcess& pre, \
	std::true_type

#define ARCHIVED_SIZE_FALSE_IMPL size_t archived_size_impl(PreMetaProcess& pre,\
	std::false_type

#define ARGS_EXTERN(name) const T& name,const PassedArg&... args
#define ARGS_EXTERN_NCONST(name) T& name,PassedArg&... args

#define ARCHIVED_TO_WITH_SIZE_FALSE_IMPL\
	size_t archive_to_with_size_impl(PreMetaProcess& pre,std::false_type,\
		memory_address buffer, size_t bufsize

#define ARCHIVED_TO_WITH_SIZE_TRUE_IMPL\
	size_t archive_to_with_size_impl(PreMetaProcess& pre,std::true_type,\
		memory_address buffer, size_t bufsize

#define RARCHIVE_IMPL_FALSE\
	size_t rarchive_impl(PreMetaProcess& pre, std::false_type,\
		const_memory_address buffer, size_t bufsize

#define RARCHIVE_IMPL_TRUE\
	size_t rarchive_impl(PreMetaProcess& pre, std::true_type,\
		const_memory_address buffer, size_t bufsize

#define RARCHIVE_1(name)\
	size_t rarch##name(PreMetaProcess& pre,\
		const_memory_address buffer, size_t bufsize

#define RARCHIVE_PASS RARCHIVE(ive_pass)

#define ARCHIVE_TO_WITH_SIZE(name)\
	size_t archive_to_with##name(PreMetaProcess pre,\
		memory_address buffer, size_t bufsize

#define ARCHIVE_TO_WITH_SIZE_PASS ARCHIVE_TO_WITH_SIZE(_size_pass)

//
/*
	序列/反序列化包装,指定了如何将对象变成内存
	Filter则提供一层弹性间接层
*/

#define SERALIZE_OBJECT_TEPMLATE template<class RealT,class T,std::size_t Index=0>
#define SERALIZE_TEMPLATE_USE <RealT,T,Index>

struct nop_filter
{//啥也不做

	//返回一个pair,bool-value

	SERALIZE_OBJECT_TEPMLATE
	static std::pair<bool,T*>
		archive_filter(T& value)
	{
		return std::make_pair(true, &value);
	}

	SERALIZE_OBJECT_TEPMLATE
	static std::pair<bool,T*> 
		rarchive_filter(T& value)
	{
		return std::make_pair(true, &value);
	}
};

//一旦archive被修改,那么rarchive一定会被修改
//所以放到了同一个元函数中
//seralize_object的流程
//获取数据->filter(数据)->是否有效->无则return,否则正常执行

#define SERZLIE_UNPAIR *pair.second




#define SERZLIZE_FILTER_DO(result,defval,call)\
	result ret=defval;\
	auto pair = Filter:: call SERALIZE_TEMPLATE_USE(value); \
	if (pair.first)\
		ret=

#define ARCHIVE_FILTER(result,defval)\
	SERZLIZE_FILTER_DO(result,defval,archive_filter)

#define RARCHIVE_FILTER(result,defval)\
	SERZLIZE_FILTER_DO(result,defval,rarchive_filter)

template<class Filter=nop_filter>
struct serialize_object
{
public:
	//Thanks for lazy template generic. we can save a lot of bits.
	SERALIZE_OBJECT_TEPMLATE
	size_t ser_archived_size(T& value)
	{
		ARCHIVE_FILTER(size_t, 0)
			archived_size(SERZLIE_UNPAIR);
		return ret;
	}

	SERALIZE_OBJECT_TEPMLATE
	size_t ser_rarchived_size(T& value)
	{
		RARCHIVE_FILTER(size_t, 0)
			archived_size(SERZLIE_UNPAIR);
		return ret;
	}


	SERALIZE_OBJECT_TEPMLATE
		memory_block ser_archive(const T& value)
	{
		ARCHIVE_FILTER(memory_block,
			memory_block(nullptr, 0))
			archive(SERZLIE_UNPAIR);
		return ret;
	}

	SERALIZE_OBJECT_TEPMLATE
		size_t ser_archive_to(const memory_block& buffer, const T& value)
	{
		ARCHIVE_FILTER(size_t, 0)
			archive_to(buffer.buffer, buffer.size, SERZLIE_UNPAIR);
		return ret;
	}

	SERALIZE_OBJECT_TEPMLATE
		size_t ser_archive_to(memory_address buffer, size_t bufsize, const T& value)
	{
		ARCHIVE_FILTER(size_t, 0)
			archive_to(buffer, bufsize, SERZLIE_UNPAIR);
		return ret;
	}

	SERALIZE_OBJECT_TEPMLATE
		size_t ser_archive_to_with_size(memory_address buffer,size_t bufsize,
		const T& value,size_t value_size)
	{
		ARCHIVE_FILTER(size_t, 0)
			archive_to_with_size(buffer, bufsize, SERZLIE_UNPAIR, value_size);
		return ret;
	}

	SERALIZE_OBJECT_TEPMLATE
		size_t ser_rarchive(const_memory_address buffer, size_t bufsize, T& value)
	{
		RARCHIVE_FILTER(size_t, 0)
			rarchive(buffer, bufsize, SERZLIE_UNPAIR);
		return ret;
	}

	SERALIZE_OBJECT_TEPMLATE
		size_t ser_rarchive_with_size(const_memory_address buffer,
		size_t bufsize, T& value,size_t value_size)
	{
		RARCHIVE_FILTER(size_t, 0)
			rarchive_with_size(buffer, bufsize, SERZLIE_UNPAIR, value_size);
		return ret;
	}

	SERALIZE_OBJECT_TEPMLATE
		size_t ser_rarchive(const const_memory_block& buffer, T& value)
	{
		RARCHIVE_FILTER(size_t, 0)
			rarchive(buffer, SERZLIE_UNPAIR);
		return ret;
	}
};

//archive-meta-process-seralize_object->filter->to memory
#endif