#ifndef ISU_RPC_LOG_HPP
#define ISU_RPC_LOG_HPP

#include <string>
#include <vector>
#include <hash_map>
#include <ostream>

#include <rpclock.hpp>

//无论什么编码,一律转换到unicode

typedef std::wstring log_string_type;
typedef unsigned long id_type;

enum logmsg_level{
	log_debug, log_warning, log_error,
	log_fault
};

namespace
{
	template<class T>
	void _make_exstr_impl(std::wostream& stream,const T& t1)
	{
		stream << t1;
	}
	template<class T, class... Arg>
	void _make_exstr_impl(std::wostream& stream, const T& t1, Arg... arg)
	{
		stream << t1;
		_make_exstr_impl(stream, arg...);
	}

	template<class... Arg>
	log_string_type _make_exstr(Arg... arg)
	{
		log_string_type ret;
		std::wostringstream stream(ret);
		stream.imbue(std::locale("chs"));
		_make_exstr_impl(stream, arg...);
		return stream.str();
	}
}
struct log_item
{
	log_item();
	log_item(logmsg_level, id_type, const log_string_type&);
	log_item(logmsg_level, id_type, const std::string&);

	logmsg_level level;
	id_type log_thread;
	log_string_type str, exstr;
private:
	void _init(logmsg_level lev, id_type, const log_string_type&);
public:
	static std::hash_map<logmsg_level, log_string_type> level_to_string;
};

class rpclog
{
public:
	rpclog();
	~rpclog();
	friend struct log_item;

	template<class String>
	void set_name(id_type thread_id, const String& name)
	{
		auto wlock = ISU_AUTO_WLOCK(_id_map_lock);
		_id_map[thread_id] = _to_unicode(name);
	}

	template<class String>
	void set_thread_name(const String& name)
	{
		id_type id = GetCurrentThreadId();
		set_name(id, name);
	}

	static log_string_type get_name(id_type thread_id)
	{
		auto wlock = ISU_AUTO_WLOCK(_id_map_lock);
		auto ret = _id_map[thread_id];
		return ret;
	}

	static log_string_type get_name()
	{
		return get_name(GetCurrentThreadId());
	}

	//必须保证stream在log死前一直有效
	void dire_to(std::wostream&);//重定向

	template<class String>
	void log(logmsg_level lev, const String& str)
	{
		auto wlock = ISU_AUTO_WLOCK(_lock);
		log_item item(lev, GetCurrentThreadId(), str);
		_logs.push_back(item);
		_write_to_stream(item);
	}

	void log(logmsg_level lev, const char* str);

	template<class String,class... Arg>
	void log(logmsg_level lev, const String& str, Arg... arg)
	{
		auto wlock = ISU_AUTO_WLOCK(_lock);
		log_item item(lev, GetCurrentThreadId(), str);
		item.exstr = _make_exstr(arg...);
		_logs.push_back(item);
		_write_to_stream(item);
	}
	typedef
		std::vector<log_item>
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
public:
	static rwlock _id_map_lock;
	static std::hash_map<id_type, log_string_type> _id_map;
private:
	std::vector<log_item> _logs;
	std::wostream* _stream;

	void _write_to_stream(const log_item&);
public:
	static log_string_type _to_unicode(const std::string&);
	static const log_string_type& _to_unicode(const log_string_type&);

};

std::wostream& operator<<(std::wostream& stream, const log_item&);
std::wistream& operator>>(std::wistream& stream, log_item&);

#endif