
#include <rpcdef.hpp>
#include <rpclock.hpp>
#include <iostream>

size_t rpcid_un::number() const
{
	return _number;
}

size_t rpcid_un::ms_id() const
{
	return _ms_id;
}

rpcid rpcid_un::to_rpcid() const
{
	rpcid id = _number;
	id <<= sizeof(_number) * 8;
	id |= _ms_id;
	return id;
}

void rpcid_un::from_rpcid(rpcid id)
{
	*this = *reinterpret_cast<rpcid_un*>(&id);
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