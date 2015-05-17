
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
		{//强制包size和同步包size都带有自定义size,不同的是同步包
				//并不是全部都带
			//adv = head.id.id;//MAGIC:这里的id.id指的是包的大小
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

//相同头:group type
//异步组:
//client端:包头,异步组的同步消息锁,异步包头但rpcid只有func_ident部分
//server端:^
//同步组:
//client端:包头,由包头指定的是否拥有消息锁,前mislength_count
//个包只有func_ident部分,剩下rpcid余下32位都是包大小
//server端:^
//组包头
//一个8位组 从左到右从小到大一次分别是
//[组类型][是否拥有消息锁][同步组是否强制每个包拥有size]下面继续
//[改组是否为单包][组是否压缩][3位保留,为1]
//实现
size_t rarchive_from(const void* buf,
	size_t size, rpc_group_middleware& grp)
{
	size_t adv = sizeof(char);
	char flag = '\0';
	rarchive_from(buf, size, flag);//这里不会调整指针,因为impl里有这个数据
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
