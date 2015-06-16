#ifndef ISU_ACTIVE_EVENT_HPP
#define ISU_ACTIVE_EVENT_HPP

#include <atomic>
#include <rpclock.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>

class active_event
	:boost::noncopyable
{
public:
	active_event(size_t wait_until = 1);
	~active_event();
	/*
		wait a job done
	*/
	void singal_wait();
	/*

	*/
	void singal_post();
	/*
		New job come
	*/
	void post();
	/*
		When an job done.call it
	*/
	void active();
	/*
		Wait all job done.
	*/
	void wait();
	/*
		It will active all wait thread.
	*/
	void cancel();
	/*
		Get finished job.
	*/
	size_t actived() const;
	/*
		It will active all wait thread.
			and reset value
	*/
	void reset(size_t limit = 0);
	/*
		Get limit
	*/
	size_t active_limit() const;
	/*
		ready
	*/
	void ready_wait();
	/*
		For up
	*/
	void active_stoped();
private:
	std::atomic_size_t _limit, _now, _cancel_count, _waiting;
	boost::mutex _kernel_lock, _stop_lock;
	CONDITION_VARIABLE _var;
	CRITICAL_SECTION _cs;
};
#endif