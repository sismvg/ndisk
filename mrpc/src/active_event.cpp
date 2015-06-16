
#include <rpcdef.hpp>
#include <active_event.hpp>

active_event::active_event(size_t wait_until)
	:_limit(wait_until), _now(0), _cancel_count(0), _waiting(0)
{
	InitializeConditionVariable(&_var);
	InitializeCriticalSection(&_cs);
}

active_event::~active_event()
{
	cancel();
}

void active_event::post()
{
	++_limit;
}

void active_event::ready_wait()
{
	_stop_lock.lock();
}

void active_event::active_stoped()
{
	_stop_lock.unlock();
}

void active_event::active()
{
	_kernel_lock.lock();
	++_now;
	if (_limit == _now)
	{
		active_stoped();
	}
	_kernel_lock.unlock();
}

void active_event::singal_wait()
{
	_kernel_lock.lock();
}

void active_event::singal_post()
{
	_kernel_lock.unlock();
}

void active_event::wait()
{
	_stop_lock.lock();
	_stop_lock.unlock();
}

void active_event::cancel()
{
	_stop_lock.unlock();
}

size_t active_event::actived() const
{
	return _now;
}

void active_event::reset(size_t limit)
{
	_kernel_lock.lock();
	_now = 0;
	if (limit != 0)
	{
		_limit = limit;
	}
	_kernel_lock.unlock();
}

size_t active_event::active_limit() const
{
	return _limit;
}