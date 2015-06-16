
#include <archive.hpp>

#include <rpcdef.hpp>
#include <rpclock.hpp>

#include <boost/crc.hpp>

size_t rpc_crc(const void* ptr, size_t length)
{
	boost::crc_16_type dor;
	dor.process_bytes(ptr, length);
	return dor.checksum();
}

size_t rpc_crc(const_memory_block blk)
{
	return rpc_crc(blk.buffer, blk.size);
}

rpcid::operator ulong64() const
{
	return to_ulong64();
}

ulong64 rpcid::to_ulong64() const
{
	return make_ulong64(_funcid, _id);
}

bool rpcid::is_sync_call() const
{
	return (exparment()&ID_ISSYNC) != 0;
}

size_t rpcid::funcid() const
{
	return _funcid;
}

size_t rpcid::exparment() const
{
	return _id;
}

//remote_produce

remote_procall::remote_procall(size_t flag)
	:_flag(flag)
{
}

size_t remote_procall::flag() const
{
	return _flag;
}

bool remote_procall::is_set(size_t val) const
{
	return (_flag&val) == 0;
}

void remote_procall::set(size_t val)
{
	_flag |= val;
}

void remote_procall::unset(size_t val)
{
	_flag &= val;
}
//sync
sync_remote_procall::sync_remote_procall(active_event& event,
	sync_callback callback, size_t flag)
	:remote_procall(flag), _callback(callback), _event(&event)
{

}

void sync_remote_procall::ready()
{
	//mylog.log(log_debug, "ready");
	_event->singal_wait();
//	mylog.log(log_debug, "ready end");
}

void sync_remote_procall::start(const udpaddr& addr)
{
	auto* ptr = this;
	//mylog.log(log_debug, "start wait,this:",this);
	_event->singal_wait();
	_event->singal_post();
//	mylog.log(log_debug, "wait end");
	
}

size_t sync_remote_procall::operator()
	(const udpaddr& addr, const argument_container& args)
{
	_callback(args);
	//mylog.log(log_debug, "active");
	_event->singal_post();
//	mylog.log(log_debug, "active fin");
	return 0;
}

//async
async_remote_procall::async_remote_procall(
	async_callback callback,sysptr_t ptr, size_t flag)
	:remote_procall(flag), _callback(callback), _argument(ptr)
{

}

void async_remote_procall::ready()
{

}

void async_remote_procall::start(const udpaddr& addr)
{
}

size_t async_remote_procall::operator()
	(const udpaddr& addr, const argument_container& args)
{
	_callback(_argument, args);
	return 0;
}

//multi
multicast_remote_procall::multicast_remote_procall(
	size_t wait_until, active_event& event,
	multicast_callback callback, sysptr_t argument, size_t flag)
	:remote_procall(flag), _callback(callback), _event(&event),
	_argument(argument), _wait_until(wait_until)
{
	_event->reset(wait_until);
}

void multicast_remote_procall::ready()
{
	_event->ready_wait();
}

void multicast_remote_procall::start(const udpaddr& addr)
{
	_event->wait();
}

size_t multicast_remote_procall::operator()
	(const udpaddr& addr, const argument_container& args)
{
	_callback(addr, 0, _argument, args);
	_event->active();
	if (_event->actived() == _event->active_limit())
	{
		_event->reset(1);
	}
	return 0;
}

size_t archived_size(const const_memory_block& blk)
{
	return blk.size;
}

size_t rarchive(const_memory_block blk, memory_block& data)
{
	return rarchive_from(blk, data);
}

size_t rarchive_from(const_memory_block blk, memory_block& data)
{
	size_t size = 0;
	size_t adv = rarchive_from(blk.buffer, blk.size, size);
	advance_in(blk, adv);
	memcpy(data.buffer, blk.buffer, size);
	return adv + size;
}

memory_block archive(const const_memory_block& blk)
{
	memory_block ret;
	ret.size = blk.size + archived_size<size_t>();
	ret.buffer = new char[ret.size];
	archive_to(ret.buffer, ret.size, blk);
	return ret;
}

size_t archive_to(memory_address addr,
	size_t size, const const_memory_block& blk)
{
	vector_air ph;
	archive_to(addr, size, ph,
		blk.size, reinterpret_cast<const char*>(blk.buffer));
	return blk.size;
}

rpclog mylog;

rpcid::rpcid()
	:_funcid(0), _id(0)
{
}

rpcid::rpcid(const ulong64& val)
{
	_funcid = val >> sizeof(size_t) * 8;
	ulong64 mask = (~0);
	_id = val&mask;
}

size_t rpc_number_generic()
{
	static size_t number = 0;
	return ++number;
}