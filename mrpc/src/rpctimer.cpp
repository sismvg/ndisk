
#include <winsock2.h>
#include <vector>
#include <rpctimer.hpp>
#include <algorithm>

rpctimer::rpctimer(callback call)
	:_callback(call), _handle(0),
	_sys_timer(INVAILD_SYS_TIMER_HANDLE),
	_active_time(TIMER_NOACTIVE_FOREVER, 0, 0)
{
}


rpctimer::~rpctimer()
{
	using namespace std;
	close();
}

void rpctimer::close()
{
	_waitlist_lock.lock();
	_waitlist.clear();
	_kill_systimer(_sys_timer);
	_waitlist_lock.unlock();
}

void rpctimer::cancel_timer(timer_handle handle)
{
	if (_handle == handle)
	{
		timeKillEvent(_sys_timer);
		handle = 0;
		_active_time = mstime_t(-1, 0, 0);
		_sys_timer = 0;
		_waitlist_lock.lock();
		//	mylog.log(log_debug, "wait list lock in cancel timer");
		_try_set_next_timer(_abstime());
		_waitlist_lock.unlock();
		//	mylog.log(log_debug, "wait list unnnnlock in cancel timer");
	}
	else
	{
		_timers.erase(handle);
	}
}

void rpctimer::cancel_timer_when_exit(timer_handle handle)
{
	auto iter = _timers.find(handle);
	if (iter != _timers.end())
	{
		iter->second.than |= KICK_DOWN_THE_LADDER;
	}
}

size_t rpctimer::timer_count() const
{
	return _timers.size();
}

bool rpctimer::_try_set_timer(mytime_t curtime, timer_handle handle,
	const mstime_t& newtm, sysptr_t par, timer_arg& arg)
{
	auto breaked_tm = _closed(newtm);
	if (breaked_tm.second)
	{
		auto lasthandle = _handle;
		_kill_systimer(_sys_timer);

		mytime_t event_time = newtm.abstime() - curtime;

		_backarg = _make_callback_arg(handle, newtm,
			event_time, par, arg);

		_sys_timer = _set_timer_impl(event_time,
			_rpctimer_callback, &_backarg);

		//第二个参数为当前时间多少ms以后要回到这个定时器来
		if (lasthandle != INVAILD_SYS_TIMER_HANDLE)
		{
			_insert_mission(lasthandle, _active_time);
		}
		//最后的处理
		_handle = handle;
		_active_time = newtm;
		return true;
	}
	return false;
}

rpctimer::timer_handle rpctimer::set_timer(
	sysptr_t par, mytime_t timeout, mytime_t window)
{//保证主定时器任务绝不在timeout_map中
	auto curtime = _abstime();

	mstime_t newtm(curtime, timeout, window);

	_timers_lock.lock();
	auto pair = _insert_timer(par, newtm);
	auto handle = pair.first->first;
	_timers_lock.unlock();

	//mylog.log(log_error, "list lock in set_timer");
	_waitlist_lock.lock();
	if (!_try_set_timer(curtime, handle, newtm, par, pair.first->second))
	{
		_insert_mission(handle, newtm);
	}
	//mylog.log(log_error, "list unnnnlock in set_timer");
	_waitlist_lock.unlock();
	return handle;
}

std::pair<rpctimer::mytime_t, bool>
rpctimer::_closed(const mstime_t& tm)
{
	if (_active_time > tm)
	{
		return std::make_pair(_active_time - tm, true);
	}
	return std::make_pair(0, false);
}

std::pair<std::map<rpctimer::timer_handle,
	rpctimer::timer_arg>::iterator, bool>
	rpctimer::_insert_timer(sysptr_t arg, const mstime_t& tm)
{
	timer_handle handle = _alloc_handle();

	timer_arg tmarg;
	tmarg.arg = arg;
	tmarg.than = 0;

	return _timers.insert(std::make_pair(handle, tmarg));
}

void rpctimer::_insert_mission(timer_handle handle,
	const mstime_t& tm)
{
	_waitlist.insert(std::make_pair(tm, handle));
}

size_t rpctimer::_alloc_handle()
{
	return ++_handle_value;
}

void rpctimer::_rpctimer_impl(SYS_TIMER_CALLBACK_ARG)
{
	//return;
	//user是该定时器的mytime_t结构

	callback_arg* arg = reinterpret_cast<callback_arg*>(user);

	mstime_t* time = &arg->tm;
	mytime_t curtime = _abstime();

	_member_lock.lock();
	_active_time.set_abstime(curtime);
	_member_lock.unlock();
	this->_callback(arg->handle, arg->parment);

	_callback_than(arg->handle, *arg->arg);

	//WARNING:map iter的copy construct非常慢
	typedef std::vector<decltype(_waitlist.begin())> vec_type;
	vec_type vec;

	//需要保存一次再调用，由于规则：每个定时器同一时间，只能被调用一次

	//	mylog.log(log_error, "list lock in timer_impl");
	_waitlist_lock.lock();
	auto fn = [&](vec_type::reference iter)
	{
		auto& pair = *iter;

		auto myiter = _timers.find(pair.second);
		if (myiter != _timers.end())
		{
			auto arg = myiter->second;
			_callback(myiter->first, arg.arg);
			_callback_than(iter->second, arg);

			mstime_t tm = pair.first;
			tm.set_abstime(curtime);
			_insert_mission(pair.second, tm);
			_waitlist.erase(iter++);
		}
		else
		{
			mylog.log(log_error, "error can not find it");
		}

	};
	//	mylog.log(log_error, "list unnnnnnlock in timer_impl");
	_waitlist_lock.unlock();
	//mylog.log(log_debug, "list relock in timer_impl");
	_waitlist_lock.lock();
	//	mylog.log(log_debug, "list relock in");
	for (auto iter = _waitlist.begin(); iter != _waitlist.end();)
	{
		auto& pair = *iter;
		//调用所有time在mstime_t的窗口期的函数
		if (_timers.find(pair.second) == _timers.end())
		{
			//	mylog.log(log_debug, "list relock in find");
			_waitlist.erase(iter++);
		}
		else if (pair.first.in_window(curtime) || curtime > pair.first)
		{
			//		vec.push_back(iter++);
			//	mylog.log(log_debug, "list relock in in_window");
			fn(iter);
			//	mylog.log(log_debug, "list relock in fn");
		}
		else
		{
			//	mylog.log(log_debug, "list relock in incerment");
			++iter;
		}
	}
	//
	if (!_try_set_next_timer(curtime))
	{
		if (arg->actime_out != _active_time.timeout())
		{
			//mylog.log(log_debug, "list reunnnnlock in timer_impl - nex_timer");
			_waitlist_lock.unlock();
			system_timer_handle tmp = _sys_timer;
			arg->actime_out = _active_time.timeout();
			_sys_timer = _set_timer_impl(_active_time.timeout(),
				_rpctimer_callback, arg);
			_kill_systimer(tmp);
			return;
		}
	}
	//mylog.log(log_debug, "list reunnnlock in timer_impl");
	_waitlist_lock.unlock();
}

bool rpctimer::_try_set_next_timer(mytime_t curtime)
{
	auto iter = _waitlist.begin();
	if (iter != _waitlist.end())
	{
		auto& pair = *iter;
		auto timer_iter = _timers.find(pair.second);
		//第二个比较是为了防止因为时间流逝造成的
		//虽然是同一个包，但优先级却轮转了
		if (timer_iter != _timers.end() && pair.second != _handle)
		{
			sysptr_t par = timer_iter->second.arg;
			if (_try_set_timer(curtime, pair.second,
				pair.first, par, timer_iter->second))
			{
				_waitlist.erase(iter++);
				return true;
			}
		}
	}
	return false;
}

void rpctimer::_erase_timer(timer_handle handle)
{
	_timers.erase(_timers.find(handle));
}

void rpctimer::_callback_than(timer_handle handle, timer_arg& arg)
{
	if (arg.than&KICK_DOWN_THE_LADDER)
	{
		cancel_timer(handle);
	}
}

rpctimer::mytime_t rpctimer::_abstime()
{
	FILETIME time;
	GetSystemTimeAsFileTime(&time);
	LARGE_INTEGER inter;
	inter.LowPart = time.dwLowDateTime;
	inter.HighPart = time.dwHighDateTime;
	SetWaitableTimer(NULL, &inter, 0, nullptr, nullptr, FALSE);
	return inter.QuadPart / 10000;
}

rpctimer::callback_arg rpctimer::_make_callback_arg(
	timer_handle handle, mstime_t tm,
	mytime_t timeout, sysptr_t ptr, timer_arg& arg)
{
	return callback_arg(this, handle, tm, ptr, timeout, arg);
}

rpctimer::system_timer_handle
rpctimer::_set_timer_impl(mytime_t timeout,
sys_timer_callback callback, callback_arg* arg)
{
#ifdef _PLATFORM_WINDOWS
	return timeSetEvent(timeout, 0, callback,
		reinterpret_cast<dword_ptr>(arg), TIME_PERIODIC);
#else
#endif
}

bool rpctimer::_kill_systimer(const system_timer_handle& handle)
{
	bool is_invaild = (handle == INVAILD_SYS_TIMER_HANDLE);
	if (!is_invaild)
	{
		timeKillEvent(handle);
	}
	return GetLastError() != 0 && is_invaild;
}
void rpctimer::_rpctimer_callback(SYS_TIMER_CALLBACK_ARG)
{
	callback_arg* par = reinterpret_cast<callback_arg*>(user);
#ifdef _PLATFORM_WINDOWS 
	par->my->_rpctimer_impl(timer_id, 0,
		reinterpret_cast<DWORD_PTR>(par), dw1, dw2);
#else 

#endif 
}

mstime::mstime()
	:_timeout(0), _abstime(0), _window_time(0)
{}

mstime::mstime(mytime_t abstime,
	mytime_t timeout /* = 0 */,
	mytime_t window /* = 0 */) :
	_abstime(abstime), _timeout(timeout), _window_time(window)
{

}
void mstime::set_abstime(mytime_t time)
{
	_abstime = time;
}

mstime::mytime_t mstime::abstime() const
{
	return _abstime + timeout();
}

mstime::mytime_t mstime::timeout() const
{
	return _timeout;
}

mstime::mytime_t mstime::window() const
{
	return _window_time;
}

void mstime::incertime(mytime_t tm)
{
	_timeout += tm;
}

bool mstime::in_window(const mytime_t& tm) const
{
	auto abs = abstime();
	mytime_t range = 0;
	if (abs >= tm)
		range = abs - tm;
	else
		range = tm - abs;
	bool ret = range <= _window_time;
	return ret;
}

bool mstime::in_window(const mstime& tm) const
{
	auto abs = abstime();
	mytime_t range = 0;
	if (abs >= tm)
		range = abs - tm;
	else
		range = tm - abs;
	return range <= tm._window_time;
}

bool operator ==(const mstime& lhs, const mstime& rhs)
{
	return !(lhs<rhs) && !(lhs>rhs);
}

bool operator >=(const mstime& lhs, const mstime& rhs)
{
	return rhs > lhs || lhs == rhs;
}

bool operator !=(const mstime& lhs, const mstime& rhs)
{
	return !(lhs == rhs);
}

bool operator <(const mstime& lhs, const mstime& rhs)
{
	return lhs.abstime() < rhs.abstime();
}

bool operator >(const mstime& lhs, const mstime& rhs)
{
	return lhs.abstime() > rhs.abstime();
}

bool operator <=(const mstime& lhs, const mstime& rhs)
{
	return lhs < rhs || lhs == rhs;
}

mstime::mytime_t operator -(const mstime& lhs, const mstime& rhs)
{
	return lhs.abstime() - rhs.abstime();
}

mstime::mytime_t operator +(const mstime& lhs, const mstime& rhs)
{
	return lhs.abstime() + rhs.abstime();
}


//callbak arg
rpctimer::callback_arg::callback_arg()
{

}

rpctimer::callback_arg::callback_arg(rpctimer* th, timer_handle hand,
	mstime_t actime, sysptr_t par, mytime_t timeout, timer_arg& arg1)
	:my(th), handle(hand), tm(actime), actime_out(timeout),
	arg(&arg1), parment(par)
{

}