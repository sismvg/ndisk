#ifndef ISU_SERIALIZE_HPP
#define ISU_SERIALIZE_HPP

#include <vector>

#include <meta_archive.hpp>
#include <archive_size.hpp>

extern unsigned long long value;
//负责archived_size和一系列工作

template<class SerializeObject>
class serialize_meta_process
	:public meta_process_tag
{
public:
	typedef SerializeObject serialize_object;
	serialize_meta_process()
		:_index(0)
	{}
	//--

	template<class PreMetaProcess,class... PassedArg>
	size_t archived_size(PreMetaProcess pre, const PassedArg&... args)
	{ 
		return archived_size_real(pre, type_list<PassedArg...>(),
			args...);
	}

	//必须保证有调用过archived_size
	template<class PreMetaProcess,class... PassedArg>
	size_t archive_to(PreMetaProcess pre,
		memory_address buffer,size_t bufsize, const PassedArg&... args)
	{ 
		return archive_to_real(pre, type_list<PassedArg...>(),
			buffer, bufsize, args...);
	}

	template<class PreMetaProcess,class... PassedArg>
	size_t rarchive(PreMetaProcess pre,
		const_memory_address buffer, size_t bufsize, PassedArg&... args)
	{
		return rarchive_real(pre, type_list<PassedArg...>(),
			buffer, bufsize, args...);
	}

	template<class PreMetaProcess,class... RealArg,class... PassedArg>
	size_t archived_size_real(PreMetaProcess& pre,
		type_list<RealArg...> typelist, const PassedArg&... args)
	{
		return archived_size_real_impl(pre, typelist, args...);
	}

	template<class PreMetaProcess,class... RealArg,class... PassedArg>
	size_t archive_to_real(PreMetaProcess& pre,
		type_list<RealArg...> typelist,
		memory_address buffer,size_t bufsize,const PassedArg&... args)
	{
		return archive_to_real_impl(pre, typelist,
			buffer, bufsize, args...);
	}

	template<class PreMetaProcess,class... RealArg,class... PassedArg>
	size_t rarchive_real(PreMetaProcess& pre,
		type_list<RealArg...> typelist,
		const_memory_address buffer,size_t bufsize,PassedArg&... args)
	{
		return rarchive_real_impl(pre, typelist,
			buffer, bufsize, args...);
	}

	template<class PreMetaProcess>
	size_t archive_to_real(PreMetaProcess&, type_list<>,
		memory_address, size_t)
	{
		return 0;
	}

	template<class PreMetaProcess>
	size_t rarchive_real(PreMetaProcess&, type_list<>,
		const_memory_address, size_t)
	{
		return 0;
	}

	template<class PreMetaProcess>
	size_t archived_size_real(PreMetaProcess&, type_list<>)
	{
		return 0;
	}

private:
	size_t _index;
	std::vector<size_t> _sizes;
	std::vector<serialize_object> _objects;

	template<
		class PreMetaProcess,
		class RealT,class... RealArg,
		class T, class... PassedArg>
	size_t archived_size_real_impl(PreMetaProcess& pre,
		type_list<RealT,RealArg...> typelist, const T& arg,const PassedArg&... args)
	{
		return archived_size_real_dispath(pre, typelist,
			is_meta_process_object<RealT>::type(),arg, args...);
	}


	template<
		class PreMetaProcess,
		class RealT, class... RealArg,
		class T, class... PassedArg>
	size_t archive_to_real_impl(PreMetaProcess& pre,
		type_list<RealT,RealArg...> typelist,
		memory_address buffer, size_t bufsize, const T& arg, const PassedArg&... args)
	{
		return archive_to_real_dispath(pre, typelist,
			is_meta_process_object<RealT>::type(), buffer, bufsize, arg, args...);
	}

	template<
		class PreMetaProcess,
		class RealT, class... RealArg,
		class T, class... PassedArg>
	size_t rarchive_real_impl(PreMetaProcess& pre,
		type_list<RealT,RealArg...> typelist,
		const_memory_address buffer,size_t bufsize,
		T& arg,PassedArg&... args)
	{
		return rarchive_real_dispath(pre, typelist,
			is_meta_process_object<RealT>::type(),buffer,bufsize, arg, args...);
	}
	//---
	template<
		class PreMetaProcess,
		class RealT,class... RealArg,
		class T, class... PassedArg>
	size_t archived_size_real_dispath(PreMetaProcess& pre,
		type_list<RealT,RealArg...> typelist,std::true_type,
		const T& tag,const PassedArg&... args)
	{//用掉tag类型
		//必须转换成非const
		return const_cast<T&>(tag).archived_size_real(pre, type_list<RealArg...>(),
			args...);
	}


	template<
		class PreMetaProcess,
		class RealT, class... RealArg,
		class T, class... PassedArg>
	size_t archive_to_real_dispath(PreMetaProcess& pre,
		type_list<RealT,RealArg...> typelist,std::true_type,
		memory_address buffer, size_t bufsize, const T& tag, const PassedArg&... args)
	{
		return const_cast<T&>(tag).archive_to_real(pre, type_list<RealArg...>(),
			buffer, bufsize, args...);
	}

	template<
		class PreMetaProcess,
		class RealT, class... RealArg,
		class T, class... PassedArg>
	size_t rarchive_real_dispath(PreMetaProcess& pre,
		type_list<RealT,RealArg...> typelist,std::true_type,
		const_memory_address buffer,size_t bufsize,
		T& tag,PassedArg&... args)
	{
		return tag.rarchive_real(pre, type_list<RealArg...>(),
			buffer, bufsize, args...);
	}
		//
	template<
		class PreMetaProcess,
		class RealT,class... RealArg,
		class T, class... PassedArg>
	size_t archived_size_real_dispath(PreMetaProcess& pre,
		type_list<RealT,RealArg...> typelist, std::false_type,
		const T& arg,const PassedArg&... args)
	{
		_objects.push_back(serialize_object());

		auto& object = _objects.back();

		size_t size = object.ser_archived_size<RealT, const T>(arg);
		_sizes.push_back(size);

		return size += archived_size_real(pre,
			type_list<RealArg...>(), args...);
	}


	template<
		class PreMetaProcess,
		class RealT, class... RealArg,
		class T, class... PassedArg>
		size_t archive_to_real_dispath(PreMetaProcess& pre,
			type_list<RealT, RealArg...> typelist, std::false_type,
			memory_address buffer, size_t bufsize, const T& arg, const PassedArg&... args)
	{
		auto& object = _objects[_index];
		size_t size = _sizes[_index];
		++_index;
		object.ser_archive_to_with_size<RealT,const T>(buffer, bufsize, arg, size);
		incerment<1>(buffer, size);

		//用掉RealArg和RealT
		return size + archive_to_real(pre, type_list<RealArg...>(),
			buffer, bufsize, args...);
	}

	template<
		class PreMetaProcess,
		class RealT, class... RealArg,
		class T, class... PassedArg>
	size_t rarchive_real_dispath(PreMetaProcess& pre,
		type_list<RealT,RealArg...> typelist,std::false_type,
		const_memory_address buffer,size_t bufsize,
		T& arg,PassedArg&... args)
	{
		serialize_object obj;
		size_t size = obj.ser_rarchived_size<RealT,T>(arg);
		//WARNING:变长参数的size是不定的,只有rarchive才能知道

		size = obj.ser_rarchive_with_size<RealT, T>(buffer, bufsize, arg, size);
		//事实上
		incerment<1>(buffer, size);
		bufsize -= size;
		return size + rarchive_real(pre, type_list<RealArg...>(),
			buffer, bufsize, args...);
	}


};

class serialize_b1
{
protected:
	memory_block _memory;
};

template<class SerializeObject>
class basic_serialize
	:public serialize_b1
{
public:
	typedef SerializeObject serialize_object;

};

template<class SerializeObject=serialize_object<nop_filter>>//序列化功能提供
class serialize
	:public basic_serialize<SerializeObject>
{
public:
	typedef SerializeObject serialize_object;
	typedef serialize_meta_process <
		serialize_object> meta_process;

	serialize()
	{}

	template<class... PassedArg>
	size_t archived_size(const PassedArg&... args)
	{
		return archived_size_real(
			type_list<PassedArg...>(), args...);
	}

	template<class... PassedArg>
	void archive_to_with_size(memory_address buffer,
		size_t bufsize, const PassedArg&... args)
	{
		return archive_to_real(buffer, bufsize,
			type_list<PassedArg...>(), args...);
	}

	template<class... PassedArg>
	memory_block archive(const PassedArg&... args)
	{
		return archive_real(
			type_list<PassedArg...>(), args...);
	}

	template<class... PassedArg>
	size_t archive_to(memory_address buffer,
		size_t bufsize, const PassedArg&... args)
	{
		return archive_to_real(buffer, bufsize,
			type_list<PassedArg...>(), args...);
	}

	template<class... PassedArg>
	size_t archive_to(const memory_block& buffer, const PassedArg&... args)
	{
		return archive_to(buffer->buffer, buffer->size, args...);
	}

	template<class... PassedArg>
	size_t rarchive(const_memory_address buffer,
		size_t bufsize, PassedArg&... args)
	{
		return rarchive_real(buffer, bufsize,
			type_list<PassedArg...>(), args...);
	}

	template<class... PassedArg>
	size_t rarchive(const const_memory_block& buffer, PassedArg&... args)
	{
		return rarchive(buffer.buffer, buffer.size, args...);
	}

	//带有real_type
	template<class... RealArg,class... PassedArg>
	size_t archived_size_real(type_list<RealArg...> typelist,
		const PassedArg&... args)
	{
		return _process.archived_size_real(_process, typelist, args...);
	}

	template<class... RealArg, class... PassedArg>
	void archive_to_with_size_real(memory_address buffer,
		size_t bufsize, type_list<RealArg...> typelist,const PassedArg&... args)
	{
		_process.archive_to_real(_process, typelist,
			buffer, bufsize, args...);
	}

	template<class... RealArg, class... PassedArg>
	memory_block archive_real(type_list<RealArg...> typelist,
		const PassedArg&... args)
	{
		auto size = archived_size_real(typelist, args...);
		_memory = default_alloc_archive_memory(size);

		memory_block block = _memory;
		archive_to_with_size_real(block.buffer, block.size,
			typelist, args...);

		return block;
	}

	template<class... RealArg, class... PassedArg>
	size_t archive_to_real(memory_address buffer,
		size_t bufsize, type_list<RealArg...> typelist, const PassedArg&... args)
	{
		size_t size = _process.archived_size(typelist, args...);
		_process.archive_to_real(_process, typelist,
			buffer, bufsize, args...);
		return size;
	}

	template<class... RealArg, class... PassedArg>
	size_t archive_to_real(const memory_block& buffer, 
		type_list<RealArg...> typelist, const PassedArg&... args)
	{
		return archive_to_real(buffer->buffer, buffer->size,
			typelist, args...);
	}

	template<class... RealArg, class... PassedArg>
	size_t rarchive_real(const_memory_address buffer,
		size_t bufsize, type_list<RealArg...> typelist, PassedArg&... args)
	{
		return _process.rarchive_real(_process, typelist,
			buffer, bufsize, args...);
	}

	template<class... RealArg, class... PassedArg>
	size_t rarchive_real(const const_memory_block& buffer,
		type_list<RealArg...> typelist ,PassedArg&... args)
	{
		return rarchive_real(buffer.buffer, buffer.size, typelist, args...);	
	}
protected:
	meta_process _process;
};


#endif