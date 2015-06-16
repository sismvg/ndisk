#ifndef ISU_RPCUDP_SEND_MODULES_HPP
#define ISU_RPCUDP_SEND_MODULES_HPP

#include <rpcudp.hpp>

const double rpcudp::_max_user_head_rate = 0.3;

SEND_MODULE_MAKE(_rtt_calc)
{
	size_t rtt_start = 0;
	pac.archive_in_here(true, rtt_start);
	return SLICE_WITH_RTT;
}

//���ʹ��ڵĵ���
SEND_MODULE_MAKE(_window_adjustment)
{
	return NOP_MAGIC;
}

//�Ӵ�ACK
SEND_MODULE_MAKE(_ask_ack)
{
	auto lock = ISU_AUTO_LOCK(client._nagle_acks_lock);
	if (!client._nagle_acks.empty())
	{
		size_t group = 0;

		client._nagle_acks_lock.lock();
		if (!client._nagle_acks.empty())
		{
			auto count_proxy = pac.push_value(0);

			auto ret = client._ask_ack_impl(max_bits, pac);

			(*count_proxy) = ret.group_count;

			client._nagle_acks_lock.unlock();
			return ret.ack_count > 0 ? SLICE_WITH_ACKS : NOP_MAGIC;
		}
	}
	return NOP_MAGIC;
}
  
//��ȡ�����Ӵ��ķ�Ƭ,��Щ��Ƭ�������û�ͷ
//������Щ��Ƭ������max_user_head_rate������,��Ϊ����Ҳ��������
SEND_MODULE_MAKE(_get_incidentally_slice)
{
	return NOP_MAGIC;
}

//����ʵ�ֵ��鷳,����û������Ҫ��Ҳ��Ū��һ��module
void rpcudp::_register_send_modules()
{
	_sender_modules.insert_modules(
		SLICE_WITH_RTT, MEMBER_BIND_IN(_rtt_calc));

	_sender_modules.insert_modules(
		SLICE_WITH_WINDOW, MEMBER_BIND_IN(_window_adjustment));

	_sender_modules.insert_modules(
		SLICE_WITH_ACKS, MEMBER_BIND_IN(_ask_ack));

	_sender_modules.insert_modules(
		SLICE_DATA_WITH_MORE_SLICE, MEMBER_BIND_IN(_get_incidentally_slice));
}

void rpcudp::_clean_ack(timer_handle, sysptr_t ptr)
{
	_clean_ack_impl(nullptr, ptr, _get_max_ackpacks());
}

void rpcudp::_clean_ack_impl(timer_handle, sysptr_t ptr, size_t max)
{
	auto* client = reinterpret_cast<rpcudp_client*>(ptr);

	//ack����ʽ-magic_type|data|~
	size_t ack_count = 0;
	client->_nagle_acks_lock.lock();

	if (!client->_nagle_acks.empty())
	{
		archive_packages pac(0, archive_packages::dynamic);
		//�����0��ʾҪ���Ͷ��ٸ���
		pac.push_value(SLICE_WITH_ACKS);//magicͷ��
		auto count_proxy = pac.push_value(0);

		auto ret = client->_ask_ack_impl(max, pac);
		if (ret.ack_count > 0)
		{
			(*count_proxy) = ret.group_count;

			auto pair = pac.memory();
			const_memory_block blk(pair.first.get(), pair.second);
			client->_nagle_acks_lock.unlock();
			_sendto_impl(blk, 0, client->_address, sizeof(udpaddr));
			return;
		}
	}
	client->_nagle_acks_lock.unlock();
}
#endif