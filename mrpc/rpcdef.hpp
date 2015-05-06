#ifndef ISU_RPC_DEF_HPP
#define ISU_RPC_DEF_HPP

#include <xstddef>
#include <archive.hpp>
#include <boost/noncopyable.hpp>
#if defined(_WIN32)||defined(_WIN64)
#define _PLATFORM_WINDOWS
#else
#define _PLATFORM_UNKNOW
#endif

typedef std::size_t size_t;
typedef unsigned int func_ident;
typedef unsigned long long rpcid;
typedef rpcid ulong64;
typedef unsigned int rpcver;
typedef void* sysptr_t;
typedef int socket_type;
typedef boost::noncopyable mynocopyable;

enum rpc_request_msg{ rpc_error, rpc_success, rpc_async, rpc_source_error };
enum client_mode{ udp = 0x01, tcp = 0x10, udp_and_tcp = 0x11 };//仅udp,tcp作为通信方式,tcp和udp混合通信
enum rpc_pack{ rpc_return = 0, rpc_ack, rpc_return_with_ack };
const size_t RPC_SERVER_PORT = 8083;
const size_t DEFAULT_RPC_BUFSIZE = 64;

struct rpc_head
{
	rpcver ver;
	rpcid id;
};

struct rpc_s_result
{
	void* buffer;
	size_t size;
};

struct rpcid_un
{
public:
	typedef std::size_t size_t;
	rpcid_un(rpcid id)
	{
		from_rpcid(id);
	}
	rpcid_un(size_t num, size_t id)
		:_number(num), _ms_id(id)
	{
	}

	size_t number() const;
	size_t ms_id() const;
	rpcid to_rpcid() const;
	void from_rpcid(rpcid);
private:
	size_t _ms_id;
	size_t _number;
};

typedef void(*rpc_callback)(sysptr_t);
typedef int(*rpc_server_call)(const rpc_s_result&);

func_ident rpc_number_generic();


template<class T>
struct result_type
{
	typedef int type;
};

template<class... Arg>
struct result_type < void(Arg...) >
{
	typedef void type;
};

template<class T, class... Arg>
struct result_type < T(Arg...) >
{
	typedef T type;
};

template<class... Arg>
rpc_s_result split_pack(const void* buffer, size_t bufsize, Arg&... arg)
{
	rpc_s_result ret;
	size_t size = archived_size(arg...);
	rarchive(buffer, bufsize, arg...);
	ret.size = bufsize - size;
	ret.buffer = new char[ret.size];
	memcpy(ret.buffer,
		reinterpret_cast<const char*>(buffer) +size, ret.size);
	return ret;
}

#define RPC_NUMBER_NAME(func) func_number

#define RPC_NUMBER_DEF(func)\
	func_ident RPC_NUMBER_NAME(func)()\
	{\
		static const func_ident ret=rpc_number_generic();\
		return ret;\
	}
#define RPC_REGISTER_SERVER_STUB(func,stub_name)\
	static auto ident_generic=[=]() ->func_ident {\
	func_ident ident=RPC_NUMBER_NAME(func)();\
	return ident;\
	};\
	static func_ident ident=ident_generic()

#define RPC_SERVER_STUB(func,stub_name)\
	auto stub_name(const rpc_s_result& arg)\
	->result_type<decltype(func)>::type\
	{\
		RPC_REGISTER_SERVER_STUB(func,stub_name);\
		auto val=rpc_rarchive_call(arg.buffer,arg.size,func);\
		return val;\
	}\
	static int func##var=([&]() ->int\
	{\
		rpc_server::register_rpc(RPC_NUMBER_NAME(func)(),stub_name);\
		return 0;\
	})();

#define RPC_CLIENT_STUB(func,stub_name,client)\
	template<class... Arg>\
	auto stub_name(Arg... arg)\
		->decltype(func(arg...))\
	{\
		rpc_s_result ret=\
			client.rpc_generic(nullptr,RPC_NUMBER_NAME(func)(),arg...);\
		typedef decltype(func(arg...)) result_type;\
		result_type func_result;\
		rarchive(ret.buffer,ret.size,func_result);\
		return func_result;\
	}


void logit(const char* str);
#endif