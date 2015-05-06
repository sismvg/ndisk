
#include <rpclog.hpp>

void rpclog::log(logmsg_level lev, const char* str)
{
	log(lev, std::string(str));
}

rpclog::const_iterator rpclog::begin() const
{
	return _logs.begin();
}

rpclog::const_iterator rpclog::end() const
{
	return _logs.end();
}

rpclog::size_t rpclog::size() const
{
	return _logs.size();
}

bool rpclog::empty() const
{
	return _logs.empty();
}

void rpclog::clear()
{
	auto wlock = ISU_AUTO_WLOCK(_lock);
	_logs.clear();
}

void rpclog::lock_for_iter()
{
	_lock.write_lock();
}

void rpclog::unlock_for_iter()
{
	_lock.write_unlock();
}
