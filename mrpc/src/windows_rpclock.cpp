
#include "platform/windows_rpclock.hpp"
#include <stdexcept>
#include <iostream>
rpclock::rpclock()
{

}
void rpclock::lock()
{
	_lock.lock();
}

void rpclock::unlock()
{
	_lock.unlock();
}

bool rpclock::try_lock()
{
	return _lock.try_lock();
}


//initlock
initlock::initlock(size_t init_count)
	:_max_count(init_count), _count(0)
{}

initlock::~initlock()
{}

void initlock::force_unlock()
{

	_lock.write_unlock();
}

void initlock::fin()
{
	if (_waiter)
	{
		++_count;
		if (_count == _max_count)
		{
			--_waiter;
			force_unlock();
		}
		else if (_count > _max_count)
		{
			throw std::runtime_error("wtf");
		}
	}
}

void initlock::ready_wait()
{
	++_waiter;
	_lock.write_lock();
}

void initlock::break_wait()
{
	_lock.write_unlock();
	force_unlock();
}

void initlock::wait()
{
	_lock.write_lock();
	force_unlock();
}