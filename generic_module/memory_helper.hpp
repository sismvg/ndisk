#ifndef ISU_MEMORY_HELPER_HPP
#define ISU_MEMORY_HELPER_HPP

#include <gobal_object.hpp>
#include <shared_memory_block.hpp>
#include <dynamic_memory_block.hpp>
#include <memory_block_utility.hpp>

#include <rpclock.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/smart_ptr/detail/spinlock.hpp>

class udp_spinlock
{
public:
	udp_spinlock()
	{
		_lock = BOOST_DETAIL_SPINLOCK_INIT;
	}

	~udp_spinlock()
	{

	}
	inline void lock()
	{
		_lock.lock();
	}
	inline void unlock()
	{
		_lock.unlock();
	}
	inline bool try_lock()
	{
		return _lock.try_lock();
	}
private:
	//rwlock _lock;
//	boost::mutex _lock;
	boost::detail::spinlock _lock;
};
#endif