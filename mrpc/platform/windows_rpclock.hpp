#ifndef ISU_WINDOWS_RPCLOCK_HPP
#define ISU_WINDOWS_RPCLOCK_HPP

#include "windows_rwlock.hpp"

#include <atomic>
#include <boost/smart_ptr/detail/spinlock.hpp>

class rpclock
{
public:
	rpclock();
	void lock();
	void unlock();
	bool try_lock();
private:
	boost::detail::spinlock _lock;
};

class initlock
{
public:
	initlock(size_t init_count);
	~initlock();
	void fin();
	void ready_wait();
	void break_wait();
	void wait();
private:
	std::atomic_size_t _count;
	size_t _max_count;
	rwlock _lock;
};
#endif