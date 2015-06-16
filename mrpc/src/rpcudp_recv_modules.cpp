#ifndef ISU_RPCUDP_RECV_MODULES_HPP
#define ISU_RPCUDP_RECV_MODULES_HPP

#include <rpcudp.hpp>
#include <archive_packages.hpp>
//rpcudp的接受模组实现

//----------------------------头部格式----------------------//
//---------rtt----window----akcs----exslice----------------//

//接受对方的rtt请求
RECV_MODULE_MAKE(_unpack_rtt_calc)
{
	bool ret = false;
	size_t type_magic = attacher.udp->type_magic;
	if (type_magic&magic||type_magic&SLICE_WITH_RTT_RECV)
	{//分别是RTT发送,和RTT接受

		size_t value = 0;
		sis.next_block([&](size_t index, const_memory_block block)
			->size_t
		{
			return rarchive(block, value);
		});

		if (type_magic&magic)
		{
			//计算网络UTC并nagle
		}
		else if (type_magic&SLICE_WITH_RTT_RECV)
		{
			//直接设定该地址的rtt
		}
		ret = true;
	}
	return ret;
}

//接受窗口调整
RECV_MODULE_MAKE(_unpack_window_adjustment)
{
	bool ret = false;
	if (attacher.udp->type_magic&magic)
	{
		//窗口占用1个int
		int adj = 0;
		sis.next_block([&](size_t index, const_memory_block blk)
			->size_t
		{
			return rarchive(blk, adj);
		});
		ret = true;
	}
	return ret;
}

//解开ack
RECV_MODULE_MAKE(_unpack_ack)
{//ack包格式-group_id,acks
	bool ret = false;
	//type_magic和包的长度不对劲
	if (attacher.udp->type_magic&magic)
	{//包长度不对
		packages_analysis analysis = sis;
		size_t ack_package_count = -1;
		//这是dynamic的type
		while (ack_package_count == -1 || ack_package_count != 0)
		{
			size_t pre = analysis.analysised();
			analysis.next_block([&](size_t index, const_memory_block blk)
				->size_t
			{
				size_t ret = 0;
				size_t group_id = 0;
				if (ack_package_count == -1)
				{
					ret = rarchive(blk, ack_package_count, group_id);
				}
				else
				{
					ret = rarchive(blk, group_id);
				}
				assert(ack_package_count != 0);
				--ack_package_count;
				advance_in(blk, ret);
				//这里可能非常多的ack
				auto lock = ISU_AUTO_LOCK(client._groups_lock);
				auto iter = client._groups.find(group_id);
				if (iter != client._groups.end())
				{//
					auto put = iter->second.bitmap.put(blk,
						[&](size_t slice_id, timer_handle& handle)
					{
						if (handle == nullptr)
						{
#ifdef _LOG_RPCUDP_RUNNING
							mylog.log(log_error, "get wrong handle");
#endif
							throw std::runtime_error("");
						}
						client._window.post();
						_timer.cancel_timer(handle);
#ifdef _LOG_RPCUDP_RUNNING
						mylog.log(log_debug,
							"recved ack group_id:", group_id, "---", slice_id);
#endif
						handle = nullptr;
					});

					ret += put.used_memory;
					client._try_cleanup_group_impl(iter);
					//mylog.log(log_debug, "group count:",client._groups.size());
				}
#ifdef _USER_DEBUG
				else
				{
#ifdef _LOG_RPCUDP_RUNNING
					mylog.log(log_debug,
						"recv ack got recved or uncreated group");
#endif
				}
#endif
				return ret;
			});
			if (pre == analysis.analysised())
				break;
		}
	}
	return ret;
}

//解开捎带分片
RECV_MODULE_MAKE(_unpack_incidentally_slice)
{	
	bool ret = false;
	return ret;
}

//主包
RECV_MODULE_MAKE(_unpack_main_pack)
{
	bool ret = false;
	if (attacher.udp->type_magic&magic)
	{
		size_t group_id = attacher.udp_ex->group_id;

		auto& coms_lock = client._coms_acks_lock;

		coms_lock.lock();

		auto coms_iter = client._get_combination(*attacher.udp_ex);
		if (coms_iter == client._coms.end())
		{
#ifdef _LOG_RPCUDP_RUNNING
			mylog.log(log_error,
				"get a fake package");
#endif
			ret = false;
		}
		else
		{
			sis.next_block([&](size_t, const_memory_block blk)
				->size_t
			{
				auto& com = coms_iter->second;

				slice_head head;
				advance_in(blk, rarchive(blk, head));
				slice sli(blk);

				com.insert(//这里也应该但是..
					std::make_pair(head, sli));

				if (com.accepted_all())//insert出错
				{
					auto* ptr = new _complete_argument(client._address,
						com.data().first);

					client._recved_groups.insert(group_id);
					client._cleanup_combination_impl(coms_iter);

					coms_lock.unlock();//系统调用一律unlock
					_recvport.post(complete_result(0, complete_package, ptr));
					coms_lock.lock();
				}	
				ret = true;
				return blk.size;
			});
		}
		coms_lock.unlock();
	}
	return ret;
}

void rpcudp::_register_recv_modules()
{
	_recver_modules.insert_modules(
		SLICE_WITH_RTT, MEMBER_BIND_IN(_unpack_rtt_calc));

	_recver_modules.insert_modules(
		SLICE_WITH_WINDOW, MEMBER_BIND_IN(_unpack_window_adjustment));

	_recver_modules.insert_modules(
		SLICE_WITH_ACKS, MEMBER_BIND_IN(_unpack_ack));

	_recver_modules.insert_modules(
		SLICE_DATA_WITH_MORE_SLICE, MEMBER_BIND_IN(_unpack_incidentally_slice));

	_recver_modules.insert_modules(
		SLICE_WITH_DATA, MEMBER_BIND_IN(_unpack_main_pack));
}

#endif

