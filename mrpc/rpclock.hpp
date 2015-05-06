#ifndef ISU_RPCLOCK_HPP
#define ISU_RPCLOCK_HPP

#ifdef _WIN32
#include "platform/windows_rpclock.hpp"
#else
#include "platform/linux_rpclock.hpp"
#endif
//语法糖宏
//WARNING:不是线程安全的..
#define AUTO_LOCK_DEF(name,lkn,ukn,tkn)\
template<class T>\
class name\
{\
public:\
	name(T& lk)\
		:_lk(&lk), _is_unlocked(false)\
	{\
		lock();\
	}\
	~name()\
	{\
		if (!_is_unlocked)\
			unlock();\
	}\
	void lock()\
	{\
		_lk->lkn();\
	}\
	void unlock()\
	{\
		_is_unlocked = true;\
		_lk->ukn();\
	}\
	bool try_lock()\
	{\
		return _lk->tkn();\
	}\
private:\
	T* _lk;\
	bool _is_unlocked;\
}

AUTO_LOCK_DEF(auto_lock, lock, unlock, try_lock);
AUTO_LOCK_DEF(auto_rlock, read_lock, read_unlock, read_try_lock);
AUTO_LOCK_DEF(auto_wlock, write_lock, write_unlock, write_try_lock);

#define RETW(lk,lock) return_wapper<lk<decltype(lock)>>(lock);

template<class T,class T2>
T return_wapper(T2& t)
{
	return T(t);
}

#define ISU_AUTO_LOCK(lock) RETW(auto_lock,lock)
#define ISU_AUTO_RLOCK(lock) RETW(auto_rlock,lock)
#define ISU_AUTO_WLOCK(lock) RETW(auto_wlock,lock)

#endif