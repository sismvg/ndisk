#ifndef ISU_RPC_LOG_HPP
#define ISU_RPC_LOG_HPP

#include <string>
#include <vector>

#include <rpclock.hpp>

class rpclog
{
public:
	enum logmsg_level{
		log_debug, log_warning, log_error,
		log_fault
	};
//	template<class... Arg>
//	void log(logmsg_level, const Arg&... arg);

	template<class Stream>
	void dire_to(Stream&);

	template<class String>
	void log(logmsg_level lev, const String& str)
	{
		auto wlock = ISU_AUTO_WLOCK(_lock);
		_logs.push_back(std::make_pair(lev, str));
	}
	void log(logmsg_level lev, const char* str);

	typedef
		std::vector < std::pair<logmsg_level, std::string> >
		::const_iterator const_iterator;

	const_iterator begin() const;
	const_iterator end() const;
	typedef std::size_t size_t;
	size_t size() const;
	bool empty() const;
	void clear();
	void lock_for_iter();
	void unlock_for_iter();
private:
	rwlock _lock;
	std::vector<std::pair<logmsg_level,std::string>> _logs;
};
#endif