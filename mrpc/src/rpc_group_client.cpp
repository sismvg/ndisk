
#include <rpc_group_client.hpp>
#include <rpc_group_middleware.hpp>
#include <iostream>
rpc_group_client::~rpc_group_client()
{

}

rpc_group_client::rpc_group_client(
	size_t length, size_t mission_count,size_t flag,initlock* lock)
	:_lock(lock), _flag(0),_mission_count(0)
{
	if (!_lock && (flag&GRPACK_HAVE_MSG_LOCK))
		_lock = new initlock(mission_count);
	_impl.mislength = length;
	_impl.miscount.group_type = flag;
	_impl.that_length_count = 0;
	_impl.miscount.size = mission_count;
	if (flag&GRPACK_IS_SINGAL_PACK)
	{
		//这个模式下出了8bit头以外全部都是数据
		//并且lock是无效的
		char grphead = GRPACK_IS_SINGAL_PACK;
		_memory.push_back(grphead);
	}
	else
	{
		_impl.miscount.size = mission_count;
		_memory.resize(_head_size(flag));
	}
}

rpc_group_client::rpc_group_client(const rpc_group_middleware& ware)
{
	rpc_group_client tmp(ware.item_length(),
		ware.mission_count(), ware._impl.miscount.group_type,
		ware._lock);
	*this = tmp;
}

const_memory_block rpc_group_client::group_block()
{
	memory_block ret;
	ret.buffer = &_memory.front();
	ret.size = _memory.size();

	auto packflag = _impl.miscount.group_type;
	if (packflag&GRPACK_HAVE_MSG_LOCK)
	{
		size_t adv=archive_to(ret.buffer, ret.size, _impl,
			reinterpret_cast<int>(_lock));
		int val = 0;
	}
	else if (!(packflag&GRPACK_IS_SINGAL_PACK))
	{
		archive_to(ret.buffer, ret.size, _impl);
	}
	else
	{
		archive_to(ret.buffer, ret.size, _impl.miscount.group_type);
	}
	return ret;
}

size_t rpc_group_client::size() const
{
	return _mission_count;
}

void rpc_group_client::reset()
{
	//
	auto packflag = _impl.miscount.group_type;
	size_t head_size = _head_size(packflag);
	if (head_size)
	{
		_memory.erase(_memory.begin() + head_size, _memory.end());
	}
	_mission_count = 0;

	_flag = 0;
	//有点麻烦
}

size_t rpc_group_client::memory_size() const
{
	return _memory.size();
}

bool rpc_group_client::standby()
{
	return _flag&GRP_STANDBY ||
		(!(_flag&GRP_MANUALLY_STANDBY) && _impl.miscount.size == _mission_count);
}

void rpc_group_client::force_standby()
{
	_flag |= GRP_STANDBY;
}

void rpc_group_client::set_manually_standby()
{
	_flag |= GRP_MANUALLY_STANDBY;
}

void rpc_group_client::wait()
{
	_lock->wait();
}
	
void rpc_group_client::ready_wait()
{
	_lock->ready_wait();
}
//impl

size_t rpc_group_client::_head_size(size_t flag)
{
	if (flag&GRPACK_IS_SINGAL_PACK)
		return 1;
	size_t total_length = sizeof(group_impl);
	if (_impl.mislength)
		total_length += _mission_count*_impl.mislength;
	if (flag&GRPACK_HAVE_MSG_LOCK)
		total_length += sizeof(initlock*);
	return total_length;
}

void rpc_group_client::_update_grpinfo(size_t size)
{
	++_mission_count;
	if (!(_flag&GRP_LENGTH_BROKEN) && size == _impl.mislength)
		++_impl.that_length_count;
	else
		_flag |= GRP_LENGTH_BROKEN;

	if (_mission_count > _impl.miscount.size)
		_impl.miscount.size = _mission_count;
	if (_lock)
	{
	//	if (_mission_count > _lock->max_count())
		//	_lock->set_max_count(_mission_count);
	}
}