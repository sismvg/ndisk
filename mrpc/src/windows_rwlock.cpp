
#include <stdexcept>
#include <platform/windows_rwlock.hpp>

rwlock::rwlock()
{
	InitializeSRWLock(&_lock);
}

rwlock::~rwlock()
{

}

void rwlock::read_lock()
{
	AcquireSRWLockShared(&_lock);
}

void rwlock::read_unlock()
{
	ReleaseSRWLockShared(&_lock);
}

bool rwlock::read_try_lock()
{
	throw std::runtime_error("");
	return false;
}

void rwlock::write_lock()
{
	AcquireSRWLockExclusive(&_lock);
}

void rwlock::write_unlock()
{
	ReleaseSRWLockExclusive(&_lock);
}

bool rwlock::write_try_lock()
{
	throw std::runtime_error("");
	return false;
}
