#ifndef ISU_WINDOWS_RWLOCK_HPP
#define ISU_WINDOWS_RWLOCK_HPP

#include <windows.h>
class rwlock
{
public:
	//try_lock�ǲ�������õ�,����Ϊ��AUTO_LOCK����ʵ�ֻ����ṩ��
	//����try_lock����һ��throw����
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