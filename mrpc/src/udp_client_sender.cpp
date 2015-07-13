
#include <rpcudp.hpp>
#include <rpcudp_client.hpp>

#ifdef _USER_DEBUG
#include <rpcudp_detail.hpp>
#endif

rpcudp_client::rpcudp_client(rpcudp& manager, const udpaddr& address,
	size_t default_window, size_t rtt, size_t cleanup_timeout)
	:_rtt(rtt), _window(default_window), _address(address),
	_timeout_of_cleanup(cleanup_timeout), _sliding_factor(0.3),
	_cleanup_timer(nullptr), _manager(&manager)
{
#ifdef _LOG_RPCUDP_RUNNING
	mylog.log(log_debug, "rpcudp_client init");
#endif
	_init();
}

template<class T>
void _detail_copy_atomic(T& lhs, const T& rhs)
{
	size_t ret = rhs;
	lhs.store(ret);
}

rpcudp_client::~rpcudp_client()
{
#ifdef _LOG_RPCUDP_RUNNING
	mylog.log(log_debug,
		"udp_clienet_sender destruct");
#endif

	if (_cleanup_timer)
		_manager->_ack_timer.cancel_timer(_cleanup_timer);

	/*
		提醒关闭罢了
	*/
	_window.close();
	_window.post(1, exit_because_destruct);

	//停下所有timer,这里保证析构时不会再有recved_from任务进行中
	//一定要按照这个lock的顺序来
	auto combin_lock = ISU_AUTO_LOCK(_coms_acks_lock);
	auto group_lock = ISU_AUTO_LOCK(_groups_lock);
	//-表示清空所有的ack
	_manager->_clean_ack_impl(nullptr, this, -1);
	std::for_each(_groups.begin(), _groups.end(),
		[&](std::pair<const group_ident, _group>& pair)
	{//删了所有的定时器
		auto& bitmap = pair.second.bitmap;
		for_each(bitmap.begin(),bitmap.end(),
			[&](ackbitmap::value_type& pair)
		{
			if (pair.second.first)
			{
				_manager->_timer.cancel_timer(pair.second.first);
				pair.second.first = nullptr;
			}
		});
	});

}
//impl

void rpcudp_client::_init()
{
	_rtt = (std::max)(static_cast<size_t>(_rtt), 15u);
	_timeout_of_cleanup = (std::max)(
		static_cast<size_t>(_timeout_of_cleanup), 15u);
	if (!(_manager->_udpmode&DISABLE_ACK_NAGLE))
	{
		_cleanup_timer = _manager->_ack_timer.set_timer(
			this, _get_timeout_of_cleanup());
	}
#ifdef _USER_DEBUG
	_retry_count = 0;
#endif
}

rpcudp_client::dynamic_memory_block 
	rpcudp_client::_alloc_udpbuf(size_t size)
{
	return dynamic_memory_block(size);
}

size_t rpcudp_client::_get_mtu()
{
	//TODO:修改
	return 1472;
}

size_t rpcudp_client::_get_timeout_of_retrans()
{
//	return 30
	return 100;
	return static_cast<size_t>((_rtt) *(1 + _sliding_factor));
		//+ _timeout_of_cleanup;;
}

size_t rpcudp_client::_get_timeout_of_cleanup()
{
	return _timeout_of_cleanup;
}

void rpcudp_client::_copy_slice_to(dynamic_memory_block& buffer,
	const slicer::value_type& value)
{
	buffer.push(value.first);
	buffer.push(value.second.data());
}

void rpcudp_client::_do_send_to(const group_iterator& group_iter,
	size_t slice_size, int flag)
{
	size_t start = 0;

	auto& group = group_iter->second;
	group.state = group_transing;

	size_t slice_id = 0;

#ifdef _LOG_RPCUDP_RUNNING
	mylog.log(log_debug, "_do_send_to start"\
		"send group_id:", group_iter->first);
#endif

	auto& core = group.slicer_core;

#ifdef _USER_DEBUG
	size_t debug_slice_id = 0;
#endif

	while (!core.slice_end())
	{
		auto iter = core.get_next_slice();

		dynamic_memory_block buffer(slice_size);

		_copy_slice_to(buffer, *iter);
		auto* retrans_ptr = new _retrans_trunk(this);
#ifdef _USER_DEBUG
		retrans_ptr->push_retrans_job(
			buffer.memory(), group_iter->first, debug_slice_id++);
#else
//		retrans_ptr->push_retrans_job(buffer);
#endif
		//发送

		if (_window.wait() != exit_because_destruct)
		{

#ifdef _LOG_RPCUDP_RUNNING
			mylog.log(log_debug, "_do_send_to enter the window "\
				"less:", _window.enabled_slice());
#endif
			timer_handle handle = _manager->_timer.set_timer(retrans_ptr,
				_get_timeout_of_retrans());

			//不用担心thread safe,map 在不添加/删除元素时
			//thread safe
			group.bitmap.set_handle(slice_id++, handle);

			_manager->_sendto_impl(buffer.memory(), flag,
				_address, sizeof(udpaddr));
		}
		else
		{
#ifdef _LOG_RPCUDP_RUNNING
			mylog.log(log_debug,
				"_do_send_to get exit msg");
#endif
			return;//不会再发送东西了
		}
	}
	//由于可能本机收到了所有的ack,然后在clean_data之间
	//想要删除并标记组,却由于GROUP_transing状态而失败
	//所以需要在这里尝试清理组

	_groups_lock.lock();
	group.state = group_waiting_ack;

#ifdef _LOG_RPCUDP_RUNNING
	mylog.log(log_debug,
		"_do_send_to send stop group_id:", group_iter->first);
#endif
	_try_cleanup_group_impl(group_iter);
	_groups_lock.unlock();
}

void rpcudp_client::_retrans_trunk_release(sysptr_t ptr)
{
	delete reinterpret_cast<_retrans_trunk*>(ptr);
}

int rpcudp_client::sendto(const_shared_memory& memory,const udpaddr& dest)
{
	size_t slice_size = _get_mtu();

	size_t group_id = _manager->_alloc_group_id();

	size_t max_bits =
		static_cast<size_t>(slice_size*rpcudp::_max_user_head_rate);

	auto genericer = [&](size_t slice_id,size_t slice_start,
		archive_packages& buffer)//由于序列化规则3
	{
		typedef rpcudp::sender_modules_manager
			::module_processor processor;

		size_t magic = SLICE_WITH_DATA;
		auto magic_proxy = buffer.push_value(magic);

		udp_header_ex ex;
		ex.group_id = group_id;
		ex.slihead.data_size = memory.size();
		ex.slice_id = slice_id;
		buffer.push_value(ex);

		_manager->_sender_modules.execute_as(
			[&](std::pair<size_t, processor>& pair)
		{
			auto ret = pair.second(pair.first, *this,
				buffer, max_bits);
			magic |= ret;
		});

		*magic_proxy = magic;
	};

	//初始化窗口
	_groups_lock.lock();
	auto pair=_groups.emplace(group_id,
		_group(slicer(memory, slice_size, genericer, 0), memory));
	_groups_lock.unlock();

	if (!pair.second)
	{
		mylog.log(log_error,
			"ack_waitlist instert new item faild");
		return -1;
	}

	auto& core = pair.first->second.slicer_core;

	//初始化窗口
#ifdef _LOG_RPCUDP_RUNNING
	mylog.log(log_debug, "sendto start slice data");
#endif

	_do_send_to(pair.first, slice_size, 0);
	return 0;
}