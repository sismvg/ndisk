
#include <archive.hpp>

#include <archive_packages.hpp>

archive_packages::archive_packages(
	size_t package_size, package_type type, size_t reserve)
	//size永远放在第一个0,并且不能被压缩
	:_memory(reserve)
{
	//3-slot成员数量

	size_t* array[] =
	{
		&_slot_of_size, &_slot_of_type, &_slot_of_package_size
	};

	for (size_t index = 0; index != 3; ++index)
	{
		(*array[index]) = index;
	}

	_init(package_size, type);
}

void archive_packages::clear()
{
	size_t type = this->type();
	size_t size = this->package_size();
	_memory.clean_data();
	_init(size, static_cast<package_type>(type));
}

size_t archived_size(const block_wapper& wapper)
{
	return wapper._blk.size;
}

std::size_t archive_to(void* buffer, size_t size, const block_wapper& wapper)
{
	memory_block blk(buffer, size);
	deep_copy_to(blk, wapper._blk);
	return wapper._blk.size;
}

memory_block archive(const block_wapper& wapper)
{
	size_t size = wapper._blk.size;
	char* array = new char[size];
	archive_to(array, size, wapper);
	return memory_block(array, size);
}

void archive_packages::fill(size_t count)
{
	_memory.fill(count);
}

size_t archive_packages::push_back(const_memory_block blk)
{
	return archive_in_here(true, block_wapper(blk));
}

archive_packages::iterator 
	archive_packages::begin()
{
	return _start();
}


archive_packages::iterator
	archive_packages::end()
{
	return _start() + size();
}


archive_packages::const_iterator
	archive_packages::begin() const
{
	return const_cast<self_type*>(this)->begin();
}


archive_packages::const_iterator
	archive_packages::end() const
{
	return const_cast<self_type*>(this)->end();
}

size_t archive_packages::size() const
{
	return _memory.size();
}

size_t archive_packages::package_count() const
{
	return _slot(_slot_of_size);
}

archive_packages::package_type archive_packages::type() const
{
	if (_type == -1)
	{
		return static_cast<package_type>(_slot(_slot_of_type));
	}
	return static_cast<package_type>(_type);
}

size_t archive_packages::memory_size() const
{
	return _memory.size();
}

size_t archive_packages::package_size() const
{
	return _slot(_slot_of_package_size);
}

archive_packages::memory_type archive_packages::memory()
{
	auto mem = _memory.memory();
	return memory_type(mem, mem.size());
}

archive_packages::const_memory_type archive_packages::memory() const
{
	return const_cast<self_type*>(this)->memory();
}

char* archive_packages::_last()
{
	return _start() + size();
}

void archive_packages::_try_extern_memory(size_t size,bool reserve)
{
	if (reserve)
		_memory.dynamic_extern(size);
	else
		_memory.static_extern(size);
}

//impl

void archive_packages::_init(size_t size, package_type type)
{
	//动态包格式下这些参数都没有意义
	if (type != dynamic)
	{
		_type = -1;
		size_t buffer [] = { 0, type, size };
		_memory.push(memory_block(buffer, sizeof(buffer)));
	}
	else
	{
		_type = type;
	}
}

char* archive_packages::_start()
{
	return _memory.begin();
}

const char* archive_packages::_start() const
{
	return _memory.begin();
}

size_t& archive_packages::_slot(size_t index)
{
	return const_cast<size_t&>(const_cast<const self_type*>
		(this)->_slot(index));
}

const size_t& archive_packages::_slot(size_t index) const
{
	return (reinterpret_cast<const size_t*>(_start()))[index];
}


std::pair < size_t, archive_packages::packages_info >
	archive_packages::from_memory(const_memory_block blk)
{
	packages_info info;
	size_t adv = rarchive(blk.buffer, blk.size, info);
	return std::make_pair(adv, info);
}
//

packages_analysis::packages_analysis(const_memory_block block)
	:_memory(block), _size(0), _start(0)
{
	_init(block);
}

const_memory_block packages_analysis::next_block()
{
	_generic();
	return _impl.back();
}

void packages_analysis::generic_all()
{
	size_t pre = 0;
	do
	{
		pre = _impl.size();
		_generic();
	} while (pre != _impl.size());
}

packages_analysis::iterator packages_analysis::begin()
{
	return _impl.begin();
}

packages_analysis::iterator packages_analysis::end()
{
	return _impl.end();
}

packages_analysis::const_iterator packages_analysis::begin() const
{
	return _impl.begin();
}

packages_analysis::const_iterator packages_analysis::end() const
{
	return _impl.end();
}

size_t packages_analysis::size() const
{
	return _size;
}

size_t packages_analysis::analysised() const
{
	return _impl.size();
}

//impl

void packages_analysis::_generic()
{
	if (_start == _memory.size)
		return;
	else if (_start > _memory.size)
	{
		throw std::out_of_range("generic out of range");
	}

	size_t size = _info.avg_size;
	if (_info.type == archive_packages::dynamic_analysis)
	{
		auto* ptr =
			reinterpret_cast<const char*>(_memory.buffer) + _start;
		size=*reinterpret_cast<const size_t*>(ptr);
		//记录用的size的调整
		_start += sizeof(size_t);
	}

	const_memory_block blk(
		reinterpret_cast<const char*>(_memory.buffer) + _start, size);
	_start += size;
	++_size;
	_impl.push_back(blk);
}

void packages_analysis::_init(const_memory_block blk)
{
	auto pair = archive_packages::from_memory(blk);
	_start += pair.first;
	_info = pair.second;
}