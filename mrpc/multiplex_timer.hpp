#ifndef ISU_MULTIPLEX_TIMER_HPP
#define ISU_MULTIPLEX_TIMER_HPP

#ifdef _DEBUG
#include <hash_set>
#endif

#include <list>
#include <atomic>
#include <functional>

#include <rpcdef.hpp>

#include <boost/smart_ptr/detail/spinlock.hpp>

#define KICK_DOWN_THE_LADDER 0x1
#define KICK_DOWN_PROCESSING 0x10
#define ADVANCE_EXPIRE 0x100
#define SET_ABD_EXPIRE 0x1000

class multiplex_timer
{//It is thread safe

public:
	typedef void* sysptr_t;
	typedef unsigned long dure_type;
	typedef unsigned long proid_type;

private:
	typedef HANDLE kernel_handle;
	typedef multiplex_timer self_type;

	struct _timer_args
	{
#ifdef _DEBUG
		size_t _debug_unique_ident;
#endif
		kernel_handle native_timer;
		self_type* self;
		volatile long than;
		volatile long interval;
		sysptr_t argument;
		//WARNING:这里必须这么写
		std::list<_timer_args>::iterator* handle;
	};
	typedef std::list<_timer_args>::iterator iterator;
public:

	typedef iterator* timer_handle;
	typedef std::function<void(timer_handle, sysptr_t)> timer_callback;
	typedef std::function<void(sysptr_t)> cleanup;

	enum timer_key{
		timer_all_exited=1,
		timer_exit=2,
		timer_exited_by_api=3
	};

	enum timer_state
	{
		timer_killing,
		timer_running
	};

	/*
	dure is interval of timer.
	*/
	multiplex_timer();
	multiplex_timer(timer_callback callback);
	multiplex_timer(timer_callback callback, cleanup);

	~multiplex_timer();

	/*
	set a timer.
	you can use cancel_timer give timer_handle
	to end.
	*/
	timer_handle set_timer(sysptr_t argument, proid_type proid);

	/*
		ready timer and get handle
	*/
	timer_handle ready_timer(sysptr_t argument, proid_type proid);

	/*
		start timer if it was started
		it will throw
	*/
	void start_timer(timer_handle);

	/*
	cancel_timer in next callback doen
	*/
	void cancel_timer(timer_handle hand);

	/*
	Don't wait callback done.
	Don't call it before cancel_timer_when_callback_exit
	*/
	void kill_timer(timer_handle hand);

	/*
	when callback exit you can use that
	*/
	void cancel_timer_when_callback_exit(timer_handle hand);

	/*
		next timeout how many ms from now
	*/
	proid_type get_expire_interval(timer_handle) const;

	/*
		get reserve argumetn
	*/
	sysptr_t get_argument(timer_handle) const;

	/*
		timer expire exchange
	*/
	void advance_expire(timer_handle, proid_type ms);

	/*
		timer expire change to abs time
		use the UTC count
	*/
	void set_abs_expire(timer_handle, proid_type utc);

	/*
	just kill
	*/
	void kill();

	/*
	when callback doen
	*/
	void stop();

	/*
		how many timer
	*/
	size_t queue_size();
private:
	timer_callback _callback;
	cleanup _cleanup;

	std::atomic_size_t _timer_state;

	kernel_handle _systimer;
	kernel_handle _complete;

	kernel_handle _complete_thread;

	boost::detail::spinlock _timers_lock;
	std::list<_timer_args> _timers;
	//impl

	void _init(timer_callback);
	void _cleanup_handle(timer_handle handle);
	timer_handle _alloc_handle(sysptr_t argument, proid_type proid);

	void _cleanup_all(kernel_handle key);
	void _cleanup_handle_impl(timer_handle handle);
	void _cleanup_user_argument(sysptr_t);

	void _delete_timer(kernel_handle handle, bool justkill);

	void _clear();
	void _erase(iterator iter);

	static void CALLBACK _crack(sysptr_t parment, BOOLEAN);
	void _crack_impl(timer_handle arg, iterator& handle);

	struct _complete_io_argument
	{
		self_type* self;
	};

	static unsigned long CALLBACK _complete_io(sysptr_t parment);
	void _complete_io_impl(sysptr_t parment);

#ifdef _DEBUG

	std::atomic_size_t _release_count;
	std::atomic_size_t _debug_ident_allocer;
	std::hash_set<size_t> _unqiue_set;
	void _debug_check_unique(size_t);

#endif
};

#endif