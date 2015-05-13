#ifndef ISU_RPC_GROUP_MIDDLEWARE_HPP
#define ISU_RPC_GROUP_MIDDLEWARE_HPP

#include <archive.hpp>
#include <functional>
#include <rpclock.hpp>

struct rpc_head;

class rpc_group_client;
class rpc_group_middleware
{
public:

	rpc_group_middleware();
	rpc_group_middleware(const_memory_block);
	friend class rpc_group_client;
	//第一个传入rpchead,第二个传入数据
	void split_group_item(
		std::function<size_t(const rpc_head&, const_memory_block)> fn);

	size_t mission_count() const;
	size_t item_length() const;//当为0时条目长度不一定
	void fin();
public:

	initlock* _lock;
	size_t _flag;
	group_impl _impl;
	const_memory_block _blk;

	friend size_t rarchive_from(const void* buf,
		size_t size, rpc_group_middleware& grp);
	friend size_t rarchive(const void* buf,
		size_t size, rpc_group_middleware& grp);
};

size_t rarchive_from(const void* buf,
	size_t size, rpc_group_middleware& grp);

size_t rarchive(const void* buf, size_t size,
	rpc_group_middleware& grp);
#endif