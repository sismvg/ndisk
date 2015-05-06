#ifndef ISU_RPCTIMER_HPP
#define ISU_RPCTIMER_HPP

#include <rpcdef.hpp>

#include <map>
#include <atomic>
#include <functional>

#include <rpclock.hpp>

//MSG:�����Ż�-mstime,rpcimter,
//����˳���Ż�
//�㷨�Ż��Լ���timer���switch
//Ҳ��Ӧ��֧�ַ�ͳһcallback

class mstime
{
public:
	typedef unsigned long long mytime_t;
	mstime();
	mstime(mytime_t abstime, mytime_t timeout = 0, mytime_t window = 0);

	mytime_t abstime() const;//������_abstime��ֵ
	mytime_t timeout() const;
	mytime_t window() const;
	void set_abstime(mytime_t time);

	bool in_window(const mstime&) const;
	bool in_window(const mytime_t&) const;
	void incertime(mytime_t);
private:
	friend class rpctimer;
	mytime_t _abstime;
	mytime_t _timeout;
	mytime_t _window_time;
};

bool operator ==(const mstime&, const mstime&);
bool operator >=(const mstime&, const mstime&);
bool operator !=(const mstime&, const mstime&);
bool operator <(const mstime&, const mstime&);
bool operator >(const mstime&, const mstime&);
bool operator <=(const mstime&, const mstime&);

mstime::mytime_t operator -(const mstime&, const mstime&);
mstime::mytime_t operator +(const mstime&, const mstime&);

//��ʱ����ɺ�kill time(��¥����)
#define KICK_DOWN_THE_LADDER 0x01
//��ɺ��趨timeoutʱ��
#define EXIT_SET_TIMEROUT 0x10

#define INVAILD_SYS_TIMER_HANDLE 0

#define TIMER_NOACTIVE_FOREVER -1

class rpctimer
	:private mynocopyable
{
public:
	typedef //��ʱ�����,����
		std::function<void(size_t, sysptr_t)> callback;
	typedef size_t timer_handle;
	typedef unsigned long long mytime_t;
public:
	typedef mstime mstime_t;

	rpctimer(callback);
	~rpctimer();

	void cancel_timer(timer_handle);
	//Ĭ�϶���ѭ������
	timer_handle set_timer(sysptr_t arg,
		mytime_t timeout, mytime_t window = 0);
	void cancel_timer_when_exit(timer_handle);
	size_t timer_count() const;
private:

#ifdef _PLATFORM_WINDOWS

	typedef unsigned int system_timer_handle;
	typedef LPTIMECALLBACK sys_timer_callback;
	typedef unsigned int uint;
	typedef DWORD_PTR dword_ptr;

#define SYS_TIMER_CALLBACK_ARG uint timer_id,uint msg,\
	dword_ptr user,dword_ptr dw1,dword_ptr dw2

#define SYS_TIMER_CALLBACK_DEF(name)\
	void __stdcall name(SYS_TIMER_CALLBACK_ARG)

#else
	typedef int system_timer_handle;
#endif

	struct timer_arg
	{
		typedef size_t operbit_type;
		sysptr_t arg;
		operbit_type than;
	};

	rwlock _waitlist_lock;
	std::multimap<mstime_t, timer_handle> _waitlist;//�ȴ��е�����

	rwlock _timers_lock;
	std::map<timer_handle, timer_arg> _timers;//callback������¼

	typedef std::atomic_size_t asize_t;

	asize_t _handle_value;//���ڷ���timer���,�������
	size_t _alloc_handle();

	callback _callback;

	rwlock _member_lock;//����������Ա��lock
	mstime_t _active_time;
	timer_handle _handle;

	std::atomic<system_timer_handle> _sys_timer;//ϵͳ��ʱ��,handleֵ�Ĳ�������ԭ�ӵ�

	void _callback_than(timer_handle, timer_arg&);//��callback��ɺ�ø�ʲô

	void _rpctimer_impl(SYS_TIMER_CALLBACK_ARG);

	std::pair<mytime_t,bool> _closed(const mstime_t&);

	std::pair<std::map<rpctimer::timer_handle,
		rpctimer::timer_arg>::iterator, bool>//MSG:template^n
		_insert_timer(sysptr_t arg, const mstime_t&);

	void _insert_mission(timer_handle, const mstime_t&);

	void _erase_mission(const mstime_t&, timer_handle);
	void _erase_timer(timer_handle);

	bool _try_set_timer(mytime_t curtime, timer_handle handle,
		const mstime_t& newtm, sysptr_t par,timer_arg&);
	bool _try_set_next_timer(mytime_t curtime);

	//utility
	static mytime_t _abstime();
	static bool _kill_systimer(const system_timer_handle&);

	struct callback_arg
	{

		callback_arg();
		callback_arg(rpctimer*, timer_handle hand,
			mstime_t actime, sysptr_t par, mytime_t timeout, timer_arg& arg);

		rpctimer* my;
		timer_handle handle;
		mstime_t tm;
		sysptr_t parment;
		mytime_t actime_out;
		timer_arg* arg;
	};

	callback_arg _backarg;
	callback_arg  _make_callback_arg(timer_handle handle, mstime_t tm,
		mytime_t, sysptr_t ptr,timer_arg& arg);

	system_timer_handle _set_timer_impl(mytime_t timeout,
		sys_timer_callback, callback_arg*);

	static SYS_TIMER_CALLBACK_DEF(_rpctimer_callback);
};

#endif