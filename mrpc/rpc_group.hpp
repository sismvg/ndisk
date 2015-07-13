#ifndef ISU_RPC_GROUP_HPP
#define ISU_RPC_GROUP_HPP

#include <vector>

#include <archive.hpp>
#include <rpcdef.hpp>
#include <rpc_group_def.hpp>
#include <rpc_archive.hpp>

class remote_produce_group
{
public:
	typedef char bit;
	typedef remote_produce_group self;
	/*
		mission_count:If you know have how many mission
			will coming,you can set it.
		is_standby will reutnr false until mission_count be zero
	*/
	remote_produce_group(size_t mission_count = 1);
	~remote_produce_group();
	
	/*
		Push a new mission,return how many bit for rarchive.
			and is type care
	*/
	template<class... Arg>
	size_t push_mission(const rpc_head& head, Arg... arg)
	{
		size_t size = 0;
		_insert(&size, sizeof(size_t));
		size_t* ptr =
			reinterpret_cast<size_t*>(&(*(_memory.end() - sizeof(size_t))));
		auto blk = self::archive(head,arg...);
		(*ptr) = blk.size;
		_insert(blk.buffer, blk.size);
		++_inserted;
		post();
		return sizeof(size_t) + blk.size;
	}
	/*
		For client trunk
	*/
	void set_trunk(const group_trunk& trunk);
	/*
		Get archied mission memory
	*/
	virtual memory_block to_memory();

	/*
		If mission_count==_inserted,return true
			and you can call to_memory
	*/
	bool is_standby() const;
	/*
		How many mission inserted;
	*/
	size_t mission_count() const;

	/*
		Reset all argument
	*/
	virtual void reset(size_t mission_count = 0);
	/*
		Wait all mission done
	*/
	void wait(size_t timeout = -1);
	/*
		Have jog do it!
	*/
	void post();
	/*
		Ready wait all mission
	*/
	void ready_wait(); 
	/*
		Job done
	*/
	void done();
public:
	static const seralize_uid serid = 0;
	static bool is_registed;

	template<class... Arg>
	static argument_container archive(const Arg&... arg)
	{
		return ::archive(arg...);
	}

	template<class... Arg>
	static void archive_to(argument_container buf, const Arg&... arg)
	{
		return ::archive_to(buf.buffer, buf.size, arg...);
	}

	template<class... Arg>
	static void rarchive(
		const argument_container& args, Arg&... arg)
	{
		::rarchive(args.buffer, args.size, arg...);
	}

	template<class... Arg>
	static void rarchive_skip_const(
		const argument_container& args, Arg&... arg)
	{
		rarchive_ncnt<Arg...>::rarchive_cnt(args, arg...);
	}

private:
	/*
		Just for dynamic memory run
	*/

	void _update_head();
	void _insert(const void* buffer, size_t size);
	std::vector<bit> _memory;
	group_head _head;
	active_event* _event;
	size_t _inserted;
};
#endif