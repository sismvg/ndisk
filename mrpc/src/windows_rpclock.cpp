
#include "platform/windows_rpclock.hpp"

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

void initlock::fin()
{
	++_count;
	if (_count == _max_count)
		_lock.write_unlock();
}

void initlock::ready_wait()
{
	_lock.write_lock();
}

void initlock::break_wait()
{
	_lock.write_unlock();
}

void initlock::wait()
{
	_lock.write_lock();
	_lock.write_unlock();
}