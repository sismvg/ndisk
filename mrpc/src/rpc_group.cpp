
#include <rpc_group.hpp>
#include <rpc_group_middleware.hpp>

bool remote_produce_group::is_registed=
	remote_produce_middleware::register_group(
	remote_produce_group::serid, [=](seralize_uid uid)
{
	assert(uid == remote_produce_group::serid);
	return std::shared_ptr<remote_produce_group>(new
		remote_produce_group(1));
});

remote_produce_group::remote_produce_group(size_t mission_count)
	:_inserted(0), _event(nullptr)
{
	_head.trunk.event_ptr = nullptr;
	reset(mission_count);
}

remote_produce_group::~remote_produce_group()
{

}

memory_block remote_produce_group::to_memory()
{
	_update_head();
	return memory_block(&(*_memory.begin()), _memory.size() );
}

void remote_produce_group::set_trunk(const group_trunk& trunk)
{
	_head.trunk = trunk;
}

void remote_produce_group::reset(size_t mission_count)
{
	size_t tmp = mission_count == 0 ? _head.mission_count : mission_count;
	_inserted = 0;
	_head.flag = 0;
	_head.reailty_marble = 0;
	_head.mission_count = tmp;
	_head.uid = serid;
	_memory.clear();
	_memory.resize(sizeof(_head));
	_update_head();
}

bool remote_produce_group::is_standby() const
{
	return _head.mission_count == _inserted;
}

size_t remote_produce_group::mission_count() const
{
	return _head.mission_count;
}

void remote_produce_group::post()
{
	if (_event)
		_event->post();
}
 
void remote_produce_group::done()
{
	if (_event)
		_event->active();
}

void remote_produce_group::wait(size_t timeout)
{
	if (_event == nullptr)
	{
		_event = new active_event(0);
	}
	_event->wait();
}


void remote_produce_group::ready_wait()
{
	if (_event == nullptr)
	{
		_event = new active_event(0);
	}
	_event->ready_wait();
}
//impl

void remote_produce_group::_update_head()
{
	assert(_memory.size() >= sizeof(_head));
	if (!_head.trunk.event_ptr)
		_head.trunk.event_ptr = _event;
	memcpy(&(*_memory.begin()), &_head, sizeof(_head));
}

void remote_produce_group::_insert(const void* buffer, size_t size)
{
	auto* begin = reinterpret_cast<const char*>(buffer);
	auto* end = begin + size;

	_memory.insert(_memory.end(),
		begin, end);
}