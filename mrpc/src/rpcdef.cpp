
#include <rpcdef.hpp>
#include <rpclock.hpp>
#include <iostream>
#include <archive.hpp>

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
	:funcid(0),id(0)
{
	id |= ID_ISSYNC;
}

rpcid::rpcid(const ulong64& val)
{
	rarchive_from(&val,sizeof(ulong64),*this);
}

rpcid::operator ulong64() const
{
	ulong64 ret = 0;
	rarchive_from(this, sizeof(rpcid), ret);
	return ret;
}

size_t rpc_number_generic()
{
	static size_t number = 0;
	return ++number;
}
rwlock loglock;
void logit(const char* str)
{
	auto wlock = ISU_AUTO_WLOCK(loglock);
	auto id = GetCurrentThreadId();
//	std::cerr << "Ïß³Ìid:" << id << "MSG:" << str << std::endl;
}