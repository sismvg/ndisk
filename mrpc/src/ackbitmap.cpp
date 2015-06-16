
#include <vector>
#include <algorithm>

#include <archive.hpp>

#include <ackbitmap.hpp>

int abs_distance(size_t lhs, size_t rhs)
{
	return static_cast<int>(lhs) - rhs;
}

ackbitmap::ackbitmap(size_t size)
	:_got(0)
{
}

void ackbitmap::extern_to(size_t size)
{
	for (size_t index = 0; index != size; ++index)
		add(index,nullptr);
}

void ackbitmap::set_handle(size_t slice_id, timer_handle handle)
{
	_set[slice_id].first = handle;
}

ackbitmap::get_result ackbitmap::get(size_t limit,
	archive_packages& pac)
{
	//put端不应该用这个
	get_result ret;
	
	std::vector<size_t> vec;
	acks result;

	bool is_liner = true;

	size_t last_key = 0;
	limit = (std::min)(limit, _set.size());
	for (size_t index = 0; index != limit; ++index)
	{
		size_t key = _get_ack();
		if (key == -1)
			break;
		vec.push_back(key);
		//
		if ((key - last_key) != 1 && last_key != 0)
			is_liner = false;
		last_key = key;
	}
	ret.ack_count = vec.size();
	if (vec.size() == 1)
	{
		pac.archive_in_here(false, ACK_SINGAL, vec.front());
	}
	/*else if (is_liner)//如果是连续的，其差值和必定为数量
	{
		//压缩
		pac.archive_in_here(false, ACK_COMPRESSED,
			vec.front(), vec.back());
	}*/
	else
	{
		//back的末尾的数字也算进ack中,如果+1则会在
		//bitset[vec.back()]时崩溃
	/*	boost::dynamic_bitset<> bitset(vec.back()+1);
		size_t start = vec[0];
		bitset.set(10);
		for (size_t index = 0; index != vec.size(); ++index)
		{
			bitset.set(vec[index] - start);
		}
		std::string str;
		boost::to_string(bitset, str);*/
		//因为bitset进行to_string的时候是会照内存序倒过来
		pac.archive_in_here(false, vector_air(), vec.size(), vec.begin());
		/*
		pac.archive_in_here(false, ACK_MULTI, start, vec.size(),
			vector_air(), str.size(), str.rbegin());
			*/
	}
	return ret;
}

ackbitmap::iterator ackbitmap::begin()
{
	return _set.begin();
}

ackbitmap::iterator ackbitmap::end()
{
	return _set.end();
}

ackbitmap::const_iterator ackbitmap::begin() const
{
	return _set.begin();
}

ackbitmap::const_iterator ackbitmap::end() const
{
	return _set.end();
}

ackbitmap::put_result ackbitmap::put(
	const_memory_block blk, ack_callback callback)
{
	put_result ret;
	size_t magic = 0;
	ret.used_memory += sizeof(size_t);
	advance_in(blk, rarchive(blk.buffer, blk.size, magic));

	if (magic&ACK_SINGAL)
	{//这时acks里面只有一个size_t
		const size_t* key = reinterpret_cast<const size_t*>(blk.buffer);
		ret.accepted = _set_ack(*key, callback);
		ret.used_memory += sizeof(size_t);
	}
	else if (magic&ACK_MULTI)
	{
	//	std::string bitstr;
		std::vector<size_t> bitstr;
		size_t start = 0;
		size_t size = 0;
	//	ret.used_memory += rarchive(blk.buffer, blk.size, start, size,
	//		vector_air(), size, std::back_inserter(bitstr));
		ret.used_memory = rarchive(blk, vector_air(), 
			size, std::back_inserter(bitstr));
		for (size_t index = 0; index != bitstr.size(); ++index)
		{
		//	if (bitstr[index] == '1')//TODO:也许要注意平台的问题
		//	{
			//	ret.accepted+=_set_ack(start + index, callback);
			ret.accepted += _set_ack(bitstr[index], callback);
		//	}
		}
	}
	else if (magic&ACK_COMPRESSED)
	{//slice_id从0开始
		assert(false);
		size_t start = 0, size = 0;
		rarchive(blk.buffer, blk.size, start, size);
		//compress的区间其实是[first,last]
		//所以要调整下size
		for (; start != size+1; ++start)
		{
			ret.accepted += _set_ack(start, callback);
		}
		ret.used_memory += sizeof(size_t) * 2;
	}
	return ret;
}

ackbitmap::put_result ackbitmap::put(
	const_memory_block ack_blk)
{
	return put(ack_blk, ack_callback());
}

ackbitmap::put_result ackbitmap::put(const acks& object,ack_callback call)
{
	return put(object.block, call);
}

void ackbitmap::add(index_type index,timer_handle handle)
{
	_set.insert(std::make_pair(index,
		std::make_pair(handle, 0)));
}

size_t ackbitmap::size() const
{
	return _set.size();
}

size_t ackbitmap::got() const
{
	return _got;
}

void ackbitmap::clear()
{
	_set.clear();
}

bool ackbitmap::accpeted_all() const
{
	return _got == _set.size();
}

bool ackbitmap::have(index_type index) const
{
	auto iter = _set.find(index);
	bool ret = false;
	if (iter != _set.end())
	{
		ret = iter->second.second > 0;
	}
	return ret;
}
//impl

size_t ackbitmap::_set_ack(size_t key,ack_callback call)
{
	size_t ret = -1;
	auto iter = _set.find(key);
	if (iter != _set.end())
	{
		ret = ++iter->second.second;
		if (ret == 1)
		{
			++_got;
			if (call)
				call(key, iter->second.first);
		}
	}
	return ret;
}

size_t ackbitmap::_get_ack()
{
	size_t result_key = -1;
	if (!_set.empty())
	{
		auto iter = _set.begin();
		result_key = iter->first;
		_set.erase(iter);
	}
	return result_key;
}