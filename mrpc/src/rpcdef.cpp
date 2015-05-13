
#include <rpcdef.hpp>
#include <rpclock.hpp>
#include <iostream>

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