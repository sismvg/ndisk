#ifndef ISU_WINDOWS_RWLOCK_HPP
#define ISU_WINDOWS_RWLOCK_HPP

#include <windows.h>
class rwlock
{
public:
	//try_lock是不允许调用的,但是为了AUTO_LOCK容易实现还是提供了
	//调用try_lock会有一个throw出来
	rwlock();
	~rwlock();
	void read_lock();
	void read_unlock();
	bool read_try_lock();

	inline void reset()
	{
		InitializeSRWLock(&_lock);
	}
	void write_lock();
	void write_unlock();
	bool write_try_lock();
private:
public:
	SRWLOCK _lock;
};
#endif