#ifndef ISU_RPC_GROUP_MIDDLEWARE_HPP
#define ISU_RPC_GROUP_MIDDLEWARE_HPP

#include <hash_map>
#include <memory>
#include <functional>

#include <archive.hpp>
#include <rpclock.hpp>
#include <rpc_group_def.hpp>

class remote_produce_group;
//head,trunk,size-args

struct rpc_head;
enum rpc_request_msg;
class remote_produce_middleware
{
public:
	//I don't want do that..but you know.. typedef,struct,class blah...
	typedef const_memory_block argument_container;
	/*
		Just a archive wapper.
			head archive must be no platform care binary code
		memory must be living before destory.
	*/
	remote_produce_middleware();
	remote_produce_middleware(const_memory_block memory);

	friend class remote_produce_group;

	typedef std::function < size_t(
		remote_produce_group& group,
			const argument_container&) > middleware_callback;

	/*
		Use that, you can don't care impl for callback;
	*/
	void for_each(middleware_callback);

	/*
		Infact here need return a trunk
	*/
	active_event* get_event() const;
	/*
		Current Group seralize_uid;
	*/
	seralize_uid uid() const;
public:
	typedef std::function <std::shared_ptr<remote_produce_group>
		(seralize_uid) > register_call;
	
	/*
		Factory mode,for dynamic create group and no care impl.
	*/
	static std::shared_ptr < remote_produce_group >
		create_group(seralize_uid);
	/*
		Register a new group.
	*/
	static bool register_group(seralize_uid, register_call);

private:

	static rwlock& _group_map_lock();
	static std::hash_map<seralize_uid, register_call>& _group_map();

	size_t _keep;
	group_head _head;
	const_memory_block _block;
};

/*	group_impl _impl;
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
	*/
#endif