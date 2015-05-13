#ifndef ISU_RPC_GROUP_CLIENT_HPP
#define ISU_RPC_GROUP_CLIENT_HPP

#include <vector>
#include <archive.hpp>

#include <rpcdef.hpp>
#include <rpclock.hpp>

//分为同步包和异步包两种
//包的相同头:pointer of rpcinfo,rpcid[func_ident,1位包类型,31位调用标识]
//异步包: 
//client端:包头,数据
//server端:包头,rpc_request_msg,数据
//同步包
//client端:包头但此时rpcid的后31位标识包的大小,数据
//server端:^,rpc_request_msg,数据
//rpc组
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

class rpc_group_middleware;

class rpc_group_client
{//事实上，这东西就是个archive模块..
public:
	~rpc_group_client();
	rpc_group_client(size_t length = 0, size_t mission_count = 0,
		size_t flag = GRPACK_HAVE_MSG_LOCK,
		initlock* lock = nullptr);
	rpc_group_client(const rpc_group_middleware&);

	template<class... Arg>
	const_memory_block push_mission(const rpc_head& head,Arg&... arg)
	{
		rpcid id = head.id.id;
		bool is_sync_pack = id.id&ID_ISSYNC;

		size_t size = archived_size(head, arg...);
		if (head.id.id&ID_ISSYNC)
			id.id = size;//同步包的packet_ident没有意义
		else
			_impl.miscount.group_type |= GRPACK_ASYNC;

		if (_memory.capacity() < (_memory.size() + size))
		{//WARNING:这里指数规避不起作用
			//_memory.reserve(_memory.size() + size);
		}

		memory_block blk = archive(head, arg...);
	//	blk.buffer = reinterpret_cast<char*>(&_memory.back()) + 1;
		const char* begin = reinterpret_cast<const char*>(blk.buffer);
	//	blk.size = size;

	//	archive_to(blk.buffer, blk.size, arg...);
		_memory.insert(_memory.end(), begin, begin + blk.size);
		_update_grpinfo(size);
		return blk;
	}

	const_memory_block group_block();

	size_t size() const;
	size_t memory_size() const;

	bool standby();
	void ready_wait();
	void wait();
	void force_standby();
	void set_manually_standby();//设置手动standby以后则要用force_standby来发送数据
	void reset();
public:
	static const size_t head_size = sizeof(group_impl);

	size_t _head_size(size_t flag);
	initlock* _lock;//用于支持wait
	group_impl _impl;
	size_t _flag;
	size_t _mission_count;
	std::vector<unsigned char> _memory;//用vector是为了指数规避特性
	//utility
	void _update_grpinfo(size_t size);
};
#endif