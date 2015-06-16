
#include <algorithm>

#include <archive.hpp>

#include <slice.hpp>

namespace
{
	void free_crack(const char* ptr)
	{
		free(const_cast<char*>(ptr));
	}

	void nop_deleter(const char* ptr)
	{

	}
}
//slice
slice::slice()
{
	_init(const_memory_block(nullptr, 0));
}

slice::slice(const_memory_block block)
{
	_init(block);
}

slice::size_t slice::size() const
{
	return _memory.size;
}

const_memory_block slice::memory() const
{
	return _memory;
}

const_memory_block slice::data() const
{
	return memory();
}

//impl
void slice::_init(const_memory_block block)
{//容错
	_memory = block;
}

//slicer
slicer::slicer()
{
	user_head_genericer gen;
	_init(const_memory_block(nullptr, 0),
		0, gen, 0, concrete);
}

slicer::slicer(const_memory_block memory,
	size_t slice_size, slice_mode mode)
{
	user_head_genericer gen;
	_init(memory, slice_size, gen, 0, mode);
	_check_argument_avaliable();
}

slicer::slicer(const_memory_block memory, size_t slice_size,
	user_head_genericer gen, size_t user_keep,slice_mode mode)
{
	_init(memory, slice_size, gen, user_keep, mode);
	_check_argument_avaliable();
}

const slicer_head& slicer::head()
{
	return _vector_head;
}

slicer::const_iterator slicer::begin() const
{
	return _slices.begin();
}

slicer::const_iterator slicer::end() const
{
	return _slices.end();
}

slicer::iterator slicer::begin()
{
	_slice();
	return _slices.begin();
}

slicer::iterator slicer::end()
{
	return _slices.end();
}


slicer::size_t slicer::size() const
{
	return _slices.size();
}

const bool slicer::slice_end() const
{
	return _used_memory == memory().size;
}

const_memory_block slicer::memory() const
{
	return _memory;
}

size_t slicer::slice_size() const
{
	return _slice_size;
}

size_t slicer::compute_seralize_size(
	size_t data_size, size_t slice_size,size_t keep)
{
	auto pair = how_many_slice(data_size, slice_size, keep);
	return pair.first*slice_size + pair.second;
}
//impl

std::pair<size_t, size_t> slicer::
	how_many_slice(size_t data_size, size_t slice_size, size_t keep)
{
	size_t slice_head_size = keep + sizeof(slice_head);

	if (slice_size < slice_head_size)
	{
		throw std::out_of_range("slice_size too small");
	}

	size_t slice_count =
		static_cast<size_t>(std::ceil(static_cast<double>(data_size) /
		(slice_size - slice_head_size)));//MSG:R hate warning

	size_t size_with_head = data_size + slice_head_size*slice_count;

	size_t last_slice_size = size_with_head - (slice_count-1)*slice_size;

	return std::make_pair(slice_count-1, last_slice_size);
}

void slicer::_init(const_memory_block memory, size_t slice_size,
	user_head_genericer gen, size_t keep, slice_mode mode)
{
	_gen = gen;
	_user_keep = keep;
	_mode = mode;
	_slice_size = slice_size;
	_used_memory = 0;
	_current_slice_id = 0;
	_memory = memory;
	if (memory.buffer)
	{
		size_t seralized_size =
			compute_seralize_size(memory.size, slice_size, keep);
		//TODO:with_head的时候data_size需要调整
		_vector_head.data_size = memory.size;

		auto pair = how_many_slice(memory.size,
			slice_size, keep);
	}
}

slicer::iterator slicer::_generic()
{
	const_memory_block mem = memory();

	const char* data_ptr =
		reinterpret_cast<const char*>(mem.buffer);

	if (_used_memory > mem.size)
	{
		throw std::runtime_error("");
	}

	//分片头
	slice_head head;
	head.start = _used_memory;
	size_t less = mem.size - _used_memory;

	archive_packages packages(_user_keep + archived_size<slice_head>(),
		archive_packages::dynamic);
	
	if (_gen)
	{
		_gen(_current_slice_id, _used_memory, packages);
	}
//	auto head_proxy = packages.push_value(head);
	//head_proxy->slice_id = _current_slice_id++;

	size_t head_size = packages.size();

	size_t data_slice = _slice_size - head_size;
	size_t data_size = data_slice > less ? less : data_slice;
	//head_proxy->slice_size = data_slice + head_size;

	const_memory_block data_range(data_ptr + _used_memory, data_size);

#ifdef _USER_DEBUG
	//head_proxy->debug_crc = rpc_crc(data_range);
#endif

	_slices.emplace_back(packages.memory().first, slice(data_range));
	_used_memory += data_size;

	return --_slices.end();
}

void slicer::_generic_to(size_t slice_count)
{
	while (_current_slice_id != slice_count)
	{
		_generic();
	}
}

void slicer::generic_all()
{
	while (!this->slice_end())
	{
		_generic();
	}
}

slicer::iterator slicer::get_next_slice()
{
	return _generic();
}

void slicer::_slice()
{
	size_t used = 0;//已经使用的原始数据内存数量
	size_t slice_used = 0;//分片后内存使用数量


	size_t less = _memory.size;

	size_t slice_id = 0;
	while (memory().size-_used_memory)
	{
		_generic();
	}
}

const_memory_block slicer::_alloc_slice_data(size_t start, size_t size)
{
	const_memory_block blk = _memory;
	if (blk.size < (start + size))
	{
		throw std::runtime_error("");
	}
	blk = _memory;
	advance_in(blk, start);
	blk.size = size;
	return blk;
}

void slicer::_check_argument_avaliable()
{
	if ((_mode&dynamic_head) && (_mode&concrete))
	{
		throw std::runtime_error("slice vector's mode"\
			"dynamic_head can not with concrete");
	}
}

combination::combination()
{
	slicer_head head;
	memset(&head, 0, sizeof(head));
	_init(head);
}

combination::combination(const slicer_head& head)
{
	_init(head);
}

void combination::insert(const std::pair<slice_head,slice>& sli)
{
	insert(sli.first.start, sli.second.data());
} 

void combination::insert(size_t slice_start, const const_memory_block& data)
{
	if (_recved.find(slice_start) == _recved.end())
	{
		auto memory = _get_range(slice_start, data);

		deep_copy_to(memory, data);
		_avaliable_memory += data.size;
		_recved.insert(slice_start);
	}
}

std::pair<combination::shared_memory, size_t> 
	combination::data() const
{
	return std::make_pair(_memory, _avaliable_memory);
}

bool combination::accepted_all() const
{
	return _avaliable_memory == _head.data_size;
}

combination::size_t combination::accepted() const
{
	return _recved.size();
}

//impl

void combination::_init(const slicer_head& head)
{
	_avaliable_memory = 0;
	_head = head;
	_memory.reset(new bit[_head.data_size], _head.data_size);
}

memory_block combination::_get_range(size_t start,
	const const_memory_block& data)
{
	memory_block blk = _memory;
	advance_in(blk, start);
	blk.size = data.size;
	return blk;
}