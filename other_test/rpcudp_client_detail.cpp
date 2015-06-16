
#include <rpcudp_client_detail.hpp>
#include <rpcudp_client.hpp>
#include <rpcudp.hpp>

void _retrans_trunk::push_retrans_job(const const_shared_memory& mem)
{
	_datas.push_back(mem);
}

#ifdef _USER_DEBUG

void _retrans_trunk::push_retrans_job(const const_shared_memory& mem,
	size_t group_id, size_t slice_id)
{
	_datas.push_back(mem);
	debug_group_id = group_id;
	debug_slice_id = slice_id;
}

#endif

//impl
void _retrans_trunk::do_retrans(timer_handle handle,raw_socket sock)
{
	if (_start_retrans())
	{
	//	mylog.log(log_debug, "start retrans");
		/*TODO:
		auto set = _get_server_unrecved();
		for (auto iter = set.begin(); iter != set.end(); ++iter)
		{
			_retrans_impl(_datas[*iter]);
		}*/
		for_each(_datas.begin(), _datas.end(),
			[&](const const_shared_memory& memory)
		{
			_retrans_impl(memory);
		});
		_end_retrans(handle);
	}
}

bool _retrans_trunk::_start_retrans()
{
	return true;
}

void _retrans_trunk::_end_retrans(timer_handle handle)
{
	auto& timer = this->client->_manager->_timer;

	timer.advance_expire(handle,
		timer.get_expire_interval(handle));
}

void _retrans_trunk::_retrans_impl(const const_shared_memory& mem)
{
#ifdef _USER_DEBUG
	++client->_retry_count;
#endif
	client->_manager->_sendto_impl(mem, 0,
		client->_address, sizeof(udpaddr));
}