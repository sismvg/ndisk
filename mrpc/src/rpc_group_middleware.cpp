
#include <rpcdef.hpp>
#include <rpc_group.hpp>
#include <rpc_group_middleware.hpp>

remote_produce_middleware::remote_produce_middleware()
	:_block(nullptr, 0), _keep(0)
{}

remote_produce_middleware::
	remote_produce_middleware(const_memory_block memory)
	:_block(memory)
{
	_keep = rarchive(memory.buffer, memory.size, _head);
}

void remote_produce_middleware::for_each(middleware_callback callback)
{
	const_memory_block args = _block;
	advance_in(args, _keep);

	auto group = create_group(uid());
	group->set_trunk(_head.trunk);
	for (size_t index = 0; index != _head.mission_count; ++index)
	{
		size_t adv = 0;
		if (_head.reailty_marble != 0)
		{
			adv = _head.reailty_marble;
		}
		else
		{
			advance_in(args, rarchive(args.buffer, args.size, adv));
		}	
		argument_container arg(args.buffer, adv);
		callback((*group), arg);
		advance_in(args, adv);
	}
}

active_event* remote_produce_middleware::get_event() const
{
	return _head.trunk.event_ptr;
}

seralize_uid remote_produce_middleware::uid() const
{
	return _head.uid;
}

//static
std::shared_ptr<remote_produce_group>
	remote_produce_middleware::create_group(seralize_uid uid)
{
	auto iter = _group_map().find(uid);
	auto& locke = _group_map_lock();
	locke.read_lock();

	auto ret = iter == _group_map().end() ?
		nullptr : iter->second(uid);

	locke.read_unlock();
	return ret;
}

bool remote_produce_middleware::
	register_group(seralize_uid uid, register_call call)
{
	auto& locke = _group_map_lock();

	locke.write_lock();
	auto pair = _group_map().insert(std::make_pair(uid, call));
	locke.write_unlock();

	return pair.second;
}

rwlock& remote_produce_middleware::_group_map_lock()
{
	static rwlock ret;
	return ret;
}

std::hash_map<seralize_uid, remote_produce_middleware::register_call>&
	remote_produce_middleware::_group_map()
{
	static std::hash_map<
		seralize_uid,
		remote_produce_middleware::register_call> ret;
	return ret;
}

/*
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
*/