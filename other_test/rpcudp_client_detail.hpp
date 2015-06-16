#ifndef ISU_RPCUDP_CLIENT_DETAIL_HPP
#define ISU_RPCUDP_CLIENT_DETAIL_HPP

#include <vector>

#include <slice.hpp>
#include <ackbitmap.hpp>
#include <shared_memory_block.hpp>

class udpclient_exit
	:public std::runtime_error
{
public:
	udpclient_exit()
		:std::runtime_error("udpclient get socket error")
	{}
};
static const size_t retry_standby = 0;
static const size_t retry_blocking = 1;

static const size_t group_transing = 0;
static const size_t group_waiting_ack = 1;
static const size_t group_transing_disable_ack = 2;
static const size_t group_transing_finished = 3;

struct _group
{
	//滑动窗口
	typedef shared_memory_block<const char> const_shared_memory;

	_group(const slicer& sli, const_shared_memory mem)
		:slicer_core(sli), memory(mem)
	{
		state = group_transing;

		//最多可以分成多少个分片
		size_t maxinum_slice_count = slicer::how_many_slice(mem.size(),
			sli.slice_size(), 0).first + 1;

		//这样就可以不用加锁即可r/w数据
		bitmap.extern_to(maxinum_slice_count);
	}

	_group(const _group&& rhs)
		:slicer_core(rhs.slicer_core)
	{
		size_t tmp = rhs.state;
		state.store(tmp);
		memory = rhs.memory;
		bitmap = rhs.bitmap;
	}

	ackbitmap bitmap;

	//分片数据功能提供者
	slicer slicer_core;

	//数据内存
	const_shared_memory memory;

	//组状态
	std::atomic_size_t state;
};

class rpcudp_client;

struct _retrans_trunk
{
	typedef slicer::iterator iterator;
	typedef shared_memory_block<const char> const_shared_memory;
	typedef multiplex_timer::timer_handle timer_handle;
	//最多15次重传
	_retrans_trunk(rpcudp_client* yourself)
		:client(yourself), retrans_limit(15)//经验所得
	{
		state = retry_standby;
		_last_retrans_tick = 0;
	}


	/*
		添加一个重传任务
	*/
	void push_retrans_job(const const_shared_memory&);
#ifdef _USER_DEBUG
	void push_retrans_job(const const_shared_memory&,
		size_t group_id, size_t slice_id);
#endif

	/*
		开始重传
	*/
	typedef int raw_socket;
	void do_retrans(timer_handle handle, raw_socket sock);

	//指向要重传的数据指针
	std::vector<const_shared_memory> _datas;

	rpcudp_client* client;

	//重传限制次数
	volatile long state;
	std::atomic_size_t _last_retrans_tick, retrans_limit;
#ifdef _USER_DEBUG
	size_t debug_group_id;
	size_t debug_slice_id;
#endif
private:
	bool _start_retrans();
	void _end_retrans(timer_handle handle);
	void _retrans_impl(const const_shared_memory& mem);
};


//只是不用写迭代器名字..
template<class Iter1,class Iter2>
struct _sender_trunk
{
	_sender_trunk(const Iter1& t1, const Iter2& t2)
		:first(t1), second(t2)
	{}

	Iter1 first;
	Iter2 second;
};

template<class Iter1,class Iter2>
inline _sender_trunk<Iter1, Iter2> declname(Iter1, Iter2)
{}

#endif