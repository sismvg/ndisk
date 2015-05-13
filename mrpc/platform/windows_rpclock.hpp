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

	inline size_t max_count() const
	{
		return _max_count;
	}

	inline void set_max_count(size_t count)
	{
		//注意,这里可能会有解锁操作
	/*	if (_max_count < count&&_count <= count)
		{
			force_unlock();
		}
		_max_count = count;*/
	}

	inline void reset(size_t max_count=0)
	{
	/*	_count = 0;
		if (max_count != 0)
			_max_count = max_count;*/
	}

	void plusfin(size_t count)
	{
		set_max_count(_max_count + count);
	}

	void wait();
	void force_unlock();
	inline void add_wait()
	{
//		++_max_count;
	}
private:
	std::atomic_size_t _count, _max_count;
	rwlock _lock;
};
#endif