#ifndef ISU_DYNAMIC_MEMORY_BLOCK_HPP
#define ISU_DYNAMIC_MEMORY_BLOCK_HPP

#include <allocators>
#include <archive_def.hpp>
#include <shared_memory_block.hpp>

//负责内存的整合和生存期
template<class Value = char, class Allocator = std::allocator<char>>
class dynamic_memory_block
{
public:
	typedef dynamic_memory_block self_type;
	typedef std::size_t size_t;
	typedef Value value_type;
	typedef Allocator allocator;
	typedef shared_memory_block<value_type> shared_memory;
	typedef shared_memory_block<const value_type> const_shared_memory;

	dynamic_memory_block(size_t size = 0)
		:_size(0)
	{
		_extern_memory_to(0, size);
	}

	/*
		填充一块数据
	*/
	void fill(size_t count)
	{
		_try_extern_memory_to(size(), count, false);
		_size += count;
	}

	/*
	塞入一块数据,并返回一个该数据的起始位置
	*/
	size_t push(const_memory_block mem)
	{
		const char* ptr =
			reinterpret_cast<const char*>(mem.buffer);

		_push_impl(ptr, ptr + mem.size);
		return _size - mem.size;
	}

	/*
		请求一块内存用于复制数据
	*/
	template<class Func>
	void alloc_to_copy(size_t size, Func fn,bool reserve=true)
	{
		_try_extern_memory_to(this->size(), size, reserve);
		memory_block blk(begin() + this->size(), size);
		fn(blk);
		_size += size;
	}

	template<class Iter>
	void push(Iter begin, Iter end)
	{
		_push_impl(begin, end);
	}

	template<class T>
	T& read(const size_t index)
	{
		return
			*reinterpret_cast<T*>(
			reinterpret_cast<char*>(begin())[index]);
	}

	template<class T>
	const T& read(const size_t index) const
	{
		return const_cast<self_type*>(this)->read(index);
	}

	size_t size() const
	{
		return _size;
	}

	size_t cap() const
	{
		return _memory.size();
	}

	shared_memory memory()
	{
		return shared_memory(_memory, size());
	}

	const_shared_memory memory() const
	{
		return const_cast<self_type*>(this)->memory();
	}

	typedef value_type* iterator;
	typedef const value_type* const_iterator;

	iterator begin()
	{
		return _memory.get();
	}

	iterator end()
	{
		return _memory.get() + _memory.size();
	}

	const_iterator begin() const
	{
		return const_cast<self_type*>(this)->begin();
	}

	const_iterator end() const
	{
		return const_cast<self_type*>(this)->end();
	}

	void clean_data()
	{
		_size = 0;
	}

	void dynamic_extern(size_t ex)
	{
		_try_extern_memory_to(size(), ex);
	}

	void static_extern(size_t ex)
	{
		//false:disable reserve memory
		_try_extern_memory_to(size(), ex, false);
	}

private:
	void _deleter(const value_type* ptr)
	{
		_alloc.deallocate(const_cast<value_type*>(ptr), cap());
	}

	template<class Iter>
	void _push_impl(Iter begin, Iter end)
	{
		size_t ex = std::distance(begin, end);
		size_t start = size();
		if (_rest() < ex)
		{
			_extern_memory_to(cap(), ex);
		}
		_copy(this->begin() + start, begin, cap() - start, ex);
		_size += ex;
	}

	size_t _rest() const
	{
		return cap() - size();
	}

	void _try_extern_memory_to(size_t pre, 
		size_t ex, bool enable_reserve = true)
	{
		if (cap() < pre + ex)
		{
			_extern_memory_to(pre, ex);
		}
	}

	void _extern_memory_to(size_t pre,
		size_t ex, bool enable_reserve = true)
	{
		if (enable_reserve)
			_adjustment(pre);
		size_t to_size = pre + ex;

		value_type* ptr =
			_alloc.allocate(to_size);

		if (this->begin())
		{
			_copy(ptr, this->begin(), to_size, size());
		}

		_memory = shared_memory(memory_block(ptr, to_size),
			_get_deleter());
	}

	std::function<void(const value_type*)> _get_deleter()
	{
		return [&](const value_type* ptr)
		{
			_deleter(ptr);
		};
	}

	void _adjustment(size_t& size)
	{
		size *= 2;
	}

	template<class Iter, class Iter2>
	void _copy(Iter dest, Iter2 src, size_t dest_size, size_t src_size)
	{
		if (dest_size < src_size)
			throw std::overflow_error("dynamic_memory_block dest to small");
		memcpy(dest, src, src_size);
	}

	size_t _size;
	shared_memory _memory;
	allocator _alloc;
};
#endif