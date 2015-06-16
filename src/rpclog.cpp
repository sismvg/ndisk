
#include <rpclog.hpp>
#include <locale>
#include <chrono>
#include <iomanip>
#include <sstream>

rpclog::rpclog()
	:_stream(nullptr)
{}

rpclog::~rpclog()
{

}

void rpclog::dire_to(std::wostream& stream)
{
	_stream = &stream;
}

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

//
void rpclog::_write_to_stream(const log_item& item)
{
#ifndef _CLOSE_RPC_LOG
	if (_stream)
	{
		(*_stream) << item << std::endl;
	}
#endif
}


const log_string_type& rpclog::_to_unicode(const log_string_type& str)
{
	return str;
}

log_string_type rpclog::_to_unicode(const std::string& str)
{
	std::wstring_convert<std::codecvt<wchar_t, char, _Mbstatet>> cov;
	return cov.from_bytes(str);
}

//log item
log_item::log_item()
{
	log_string_type nop_string;
	_init(log_debug, 0, nop_string);
}

log_item::log_item(logmsg_level lev, id_type id, const std::string& str)
{
	_init(lev, id, rpclog::_to_unicode(str));
}

log_item::log_item(logmsg_level lev, id_type id, const log_string_type& str)
{
	_init(lev, id, str);
}

std::hash_map<id_type, log_string_type> rpclog::_id_map;
rwlock rpclog::_id_map_lock;

#define K std::hash_map<logmsg_level, log_string_type> 
K log_item::level_to_string = []()->K
{
	K ret;
	ret[log_debug] = L"debug";
	ret[log_warning] = L"warning";
	ret[log_error] = L"error";
	ret[log_fault] = L"fault";
	return ret;
}();
#undef K

std::wostream& operator<<(std::wostream& stream, const log_item& log)
{
#ifndef _CLOSE_RPC_LOG
	auto now = std::chrono::system_clock::now();

	auto tm = std::chrono::system_clock::to_time_t(now);
	auto time_string = std::put_time(std::localtime(&tm),"%H:%M:%S");

	std::string byte_string;
	std::ostringstream stm(byte_string);
	stm << time_string;
	stream << "[" << rpclog::_to_unicode(stm.str()) << "]-";
	stream << "thread_id:[" << log.log_thread << "]-";
	
	auto wlock = ISU_AUTO_WLOCK(rpclog::_id_map_lock);
	auto iter = rpclog::_id_map.find(log.log_thread);
	if (iter != rpclog::_id_map.end())
	{
		stream << "thread_name:[" << iter->second << "]-";
	}

	stream << "level:[" << log_item::level_to_string[log.level] << "]-";
	stream << "msg:[" << log.str << log.exstr << ']';
#endif
	return stream;
}

std::wistream& operator>>(std::wistream& istream, log_item& log)
{
	return istream;
}

//
void log_item::_init(logmsg_level lev, id_type ident, const log_string_type& logstr)
{
	level = lev;
	log_thread = ident;
	str = logstr;
}
