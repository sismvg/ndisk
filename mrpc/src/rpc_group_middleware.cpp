
#include <rpcdef.hpp>
#include <rpc_group_middleware.hpp>
#include <iostream>

rpc_group_middleware::rpc_group_middleware()
{

}

rpc_group_middleware::rpc_group_middleware(const_memory_block blk)
{
	_lock = nullptr;
	rarchive(blk.buffer, blk.size, *this);
}

void rpc_group_middleware::fin()
{
	if (_lock)
		_lock->fin();
}

void rpc_group_middleware::split_group_item(
	std::function<size_t(const rpc_head&, const_memory_block)> fn)
{

	size_t total_count = mission_count();
	size_t same_length_count = _impl.that_length_count;

	size_t adv = 0;
	auto block = _blk;
	auto val = crc16l(reinterpret_cast<const char*>(block.buffer), block.size);
	int val2 = crc16l(reinterpret_cast<const char*>(block.buffer), block.size);
	char flag = _impl.miscount.group_type;

	int is_singal_length = flag&GRPACK_SYNCPACK_FORCE_SIZE;
	int is_singal_pack = flag&GRPACK_IS_SINGAL_PACK;
	int is_sync_pack = !(flag&GRPACK_ASYNC);
	while (total_count--)
	{
		using namespace std;
		rpc_head head;
		head.id.id = 0;
		if (!(flag&GRPACK_ASYNC))
			head.id.id |= ID_ISSYNC;
		size_t adv1 = rarchive(block.buffer, block.size, head);
		//std::cout << adv1 << std::endl;
		advance_in(block, adv1);
		adv = fn(head, block);
		if (same_length_count)
		{
			same_length_count--;
			adv = _impl.mislength;
		}
		else if (is_singal_length||is_sync_pack)
		{//ǿ�ư�size��ͬ����size�������Զ���size,��ͬ����ͬ����
				//������ȫ������
			//adv = head.id.id;//MAGIC:�����id.idָ���ǰ��Ĵ�С
		}
		if (!(_impl.miscount.group_type&is_singal_pack))
		{
			advance_in(block, adv);
		}
	}
}

size_t rpc_group_middleware::mission_count() const
{
	return _impl.miscount.size;
}

size_t rpc_group_middleware::item_length() const
{
	return _impl.mislength;
}

//��ͬͷ:group type
//�첽��:
//client��:��ͷ,�첽���ͬ����Ϣ��,�첽��ͷ��rpcidֻ��func_ident����
//server��:^
//ͬ����:
//client��:��ͷ,�ɰ�ͷָ�����Ƿ�ӵ����Ϣ��,ǰmislength_count
//����ֻ��func_ident����,ʣ��rpcid����32λ���ǰ���С
//server��:^
//���ͷ
//һ��8λ�� �����Ҵ�С����һ�ηֱ���
//[������][�Ƿ�ӵ����Ϣ��][ͬ�����Ƿ�ǿ��ÿ����ӵ��size]�������
//[�����Ƿ�Ϊ����][���Ƿ�ѹ��][3λ����,Ϊ1]
//ʵ��
size_t rarchive_from(const void* buf,
	size_t size, rpc_group_middleware& grp)
{
	size_t adv = sizeof(char);
	char flag = '\0';
	rarchive_from(buf, size, flag);//���ﲻ�����ָ��,��Ϊimpl�����������
	//advance_in(buf, size, adv);
	grp._lock = nullptr;
	if (!(flag&GRPACK_IS_SINGAL_PACK))
	{
		int ptr = 0;
		if (flag&GRPACK_ASYNC||flag&GRPACK_HAVE_MSG_LOCK)
		{
			adv = rarchive(buf, size,
				grp._impl, grp._lock);
			advance_in(buf, size, adv);
		}
		else
		{
			int val = 0;
			advance_in(buf, size, rarchive_from(buf, size, grp._impl));
		}
	}
	else
	{
		auto& impl = grp._impl;
		impl.miscount.group_type = flag;
		impl.miscount.size = 1;
		impl.mislength = 0;
		impl.that_length_count = 0;
		advance_in(buf, size, sizeof(char));
	}
	grp._blk.buffer = buf;
	grp._blk.size = size;
	grp._flag = 0;
	return adv;
}

size_t rarchive(const void* buf, size_t size,
	rpc_group_middleware& grp)
{
	return rarchive_from(buf, size, grp);
}
