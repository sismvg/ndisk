
#include <rpcudp.hpp>
#include <rpcudp_client.hpp>

#include <rpcudp_detail.hpp>
#include <archive_packages.hpp>
#include <script_cpapa.hpp>

bool rpcudp_client::recved_from(
	const udpaddr& addr, const_shared_memory const_mem)
{
#ifdef _LOG_RPCUDP_RUNNING
	mylog.log(log_debug, "recved_from got package");
#endif
	return _process_package_impl(addr, const_mem) != -1;
}

//impl
size_t rpcudp_client::_process_package_impl(
	const udpaddr& addr, const_memory_block memory)
{
	packages_analysis sis(memory);
	rpcudp_detail::__slice_head_attacher attacher(sis);

#ifdef _LOG_RPCUDP_RUNNING
	mylog.log(log_debug, "analysis the data");
#endif

	typedef rpcudp::recver_modules_manager
		::module_processor processor;

	_manager->_recver_modules.
		execute_as([&](std::pair<size_t, processor>& pair)
	{
		pair.second(pair.first, *this, attacher, sis);
	});
#ifdef _LOG_RPCUDP_RUNNING
	mylog.log(log_debug, "stop analysis the data");
#endif
	return memory.size;
}

ackbitmap::get_result rpcudp_client::_ask_ack_impl(
	size_t max_ack,archive_packages& pac)
{
	auto& map = _nagle_acks;

#ifdef _LOG_RPCUDP_RUNNING
	mylog.log(log_debug, "try got the nagled acks");
#endif

	ackbitmap::get_result result;
	result.ack_count = 0;
	for (auto iter = map.begin(); iter != map.end();)
	{
		auto& bitmap = iter->second;
		if (bitmap.size() != 0 && max_ack >= 1)
		{//group_id-acks
			pac.push_value(iter->first);

			auto tmp = bitmap.get(max_ack, pac);

			if (tmp.ack_count > 0)
			{
				result.ack_count += tmp.ack_count;
				result.group_count += tmp.group_count;
			}
		}
		erase_iter_if_empty(map, bitmap, iter);
	}
	return result;
}

void rpcudp_client::_send_ack(const udpaddr& dest,
	size_t group_id, size_t slice_id)
{
#ifdef _LOG_RPCUDP_RUNNING
	mylog.log(log_debug,
		"put ack group_id:", group_id, "-slice_id:", slice_id);
#endif

	auto lock = ISU_AUTO_LOCK(_nagle_acks_lock);

	if (_manager->_udpmode&DISABLE_ACK_NAGLE)
	{
		archive_packages packages(0,
			archive_packages::dynamic);

		packages.archive_in_here(true, SLICE_WITH_ACKS,
			group_id, 1, ACK_SINGAL, slice_id);

		lock.unlock();

		auto pair = packages.memory();

		const_memory_block blk(pair.first.get(), pair.second);

		int ret = _manager->_sendto_impl(blk, 0, dest, sizeof(udpaddr));
		if (ret == SOCKET_ERROR)
		{
			throw udpclient_exit();
		}
	} 
	else
	{
		_nagle_acks[group_id].add(slice_id, nullptr);
	}
}

void rpcudp_client::_cleanup_combination(size_t group_id)
{
	_cleanup_combination_impl(_coms.find(group_id));
}

void rpcudp_client::_cleanup_combination_impl(const combin_iterator& iter)
{
	if (iter != _coms.end())
	{
		_coms.erase(iter);
	}
}

void rpcudp_client::_try_cleanup_group(size_t group_id)
{
	_try_cleanup_group_impl(_groups.find(group_id));
}

void rpcudp_client::_try_cleanup_group_impl(const group_iterator& iter)
{
	if (_groups.end() != iter)
	{
		auto& bitmap = iter->second.bitmap;
		size_t value = iter->second.slicer_core.size();

		//bug-slice完成前就比较怎么办？
		if (
			bitmap.got() == iter->second.slicer_core.size()
			&&
			iter->second.state == group_waiting_ack
			)
		{
#ifdef _USER_DEBUG
			/*
			auto pair=objs.get_object<rpcudp_client>("o1");
			auto* client = pair.second;
			auto lock = ISU_AUTO_LOCK(client->_coms_acks_lock);
			if (client->_recved_groups.find(iter->first)
				== client->_recved_groups.end())
			{
				if (*client->_recved_groups.begin() == 0)
				{
					int val = 0;
				}
				mylog.log(log_error, "recved unrecved package");
			}*/
#endif
			_groups.erase(iter);
		}
	}
}

rpcudp_client::combin_iterator
	rpcudp_client::_get_combination(const udp_header_ex& ex)
{//要先确认是否已经全部接受到了
	auto iter = _coms.find(ex.group_id);
	if (iter == _coms.end())
	{
		iter = _coms.insert(std::make_pair(ex.group_id,
			combination(ex.slihead))).first;
	}
	else
	{
		auto recved_iter = _recved_groups.find(ex.group_id);
		if (recved_iter != _recved_groups.end())
			return _coms.end();
	}
	return iter;
}