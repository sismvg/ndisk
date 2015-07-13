#ifndef ISU_SHARED_MEMORY_BLOCK_HPP
#define ISU_SHARED_MEMORY_BLOCK_HPP

#include <memory>
#include <functional>
#include <archive_def.hpp>

#define IS_CNT(type1) is_const<type1>::type::value

#define SHARED_MEMORY_BLOCK_CHECK_CONST \
using namespace std;\
static_assert(\
	(IS_CNT(BitType)) || (!IS_CNT(BitType) && !IS_CNT(Bit)),\
	"function _init's Bit have to const when BitType const");

template<class BitType>
class shared_memory_block
	:public std::shared_ptr<BitType>
{
public:
	typedef char bit;
	typedef std::shared_ptr<BitType> father_type;
	typedef shared_memory_block<BitType> self_type;
	typedef std::function<void(const bit* ptr)> deleter_type;

	shared_memory_block()
	{
		_init(reinterpret_cast<BitType*>(nullptr), 0);
	}

	//WARNING:因为没有办法获取内存的大小,所以要屏蔽掉这两个构造

	template<class Bit>
	shared_memory_block(const shared_memory_block<Bit>& sptr)
	{
		SHARED_MEMORY_BLOCK_CHECK_CONST;
		static_cast<father_type&>(*this) = sptr;
		_memblk = sptr.operator const_memory_block();
	}

	shared_memory_block(const father_type& sptr,
		size_t size, deleter_type deleter)
		:father_type(sptr, deleter)
	{
		_init(sptr.get(), size);
	}
	//

	shared_memory_block(memory_block blk, deleter_type deleter)
		:father_type(_real_buf(blk),deleter)
	{
		_init(blk.buffer,blk.size);
	}

	shared_memory_block(const_memory_block blk, deleter_type deleter)
		:father_type(_real_buf(blk),deleter)
	{
		_init(reinterpret_cast<const char*>(blk.buffer),blk.size);
	}

	shared_memory_block(const father_type& sptr, size_t size)
		:father_type(sptr)
	{
		_init(sptr.get(), size);
	}

	template<class BitType>
	shared_memory_block(BitType* memory, size_t size,
		deleter_type deleter = nullptr)
		:father_type(memory, _real_deleter(deleter))
	{
		_init(memory, size);
	}

	template<class Blk>
	shared_memory_block(Blk blk, deleter_type deleter = nullptr)
		: father_type(_real_buf(blk), _real_deleter(deleter))
	{
		_init(blk.buffer, blk.size);
	}

	std::size_t size() const
	{
		return _memblk.size;
	}

	void reset()
	{
		father_type::reset();
		_init(reinterpret_cast<BitType*>(nullptr), 0);
	}

	template<class BitType>
	void reset(BitType* memory, size_t size)
	{
		father_type::reset(memory);
		_init(memory, size);
	}

	operator memory_block() const
	{
		typedef char Bit;
		SHARED_MEMORY_BLOCK_CHECK_CONST;
		return memory_block(
			const_cast<std::remove_const<BitType>::type*>(
			reinterpret_cast<const char*>(_memblk.buffer)),
			_memblk.size);
	}

	operator const_memory_block() const
	{
		typedef const char Bit;
		return _memblk;
	}

private:

	deleter_type _real_deleter(deleter_type ptr)
	{
		return ptr == nullptr ? deleter : ptr;
	}

	template<class T>
	BitType* _real_buf(T buf)
	{
		auto* ptr = reinterpret_cast<const BitType*>(buf.buffer);
		return const_cast<BitType*>(ptr);
	}

	static void deleter(const char* ptr)
	{
		delete [] ptr;
	}

	template<class Bit>
	void _init(Bit* memory,size_t size)
	{
		SHARED_MEMORY_BLOCK_CHECK_CONST;
		_memblk = const_memory_block(memory, size);
	}

	const_memory_block _memblk;
};

#undef IS_CNT
#endif