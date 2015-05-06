#ifndef ISU_RPC_CS_HPP
#define ISU_RPC_CS_HPP

#include <memory>
#include <hash_map>

#include <WinSock2.h>
#include <vector>
#include <rpcdef.hpp>
#include <rpclock.hpp>
#include <archive.hpp>
#include <atomic>
#include <functional>
#include <boost/noncopyable.hpp>
#include <rpcudp.hpp>

typedef rpclock lock_type;
typedef boost::noncopyable mynocopyable;

class rpc_client;

class rpc_group_client;
class rpc_group_server
{
public:
	rpc_group_server();
	//由于只能在运行时知道..所以传入个固定参数没啥意义
	rpc_group_server(void* buffer, size_t size);

	template<class Func>
	void callall(Func callback);
};
enum group_comm{blob_back,single_back};

//blob_back:等所有的rpc server call完成后回来
//single_back:当某一个^

class rpc_group_client
{
public:
	rpc_group_client();
	~rpc_group_client();
	//某个rpc调用的参数archived_size是常数则指定，否则为0,mission_count用于建议
	//rpc_group该分配多少扣空间
	rpc_group_client(size_t group_id, size_t length = 0, size_t mission_count = 0);
	template<class... Arg>
	void push_mission(rpcid id, Arg&... arg);
	size_t size() const;
	size_t memory_size() const;
	bool ready_to_send() const;//表明可以发送了
private:
	//格式
	//_groupid,_length,_memory
	//_memory结构
	//rpc mission 结构1,<2,<3
	std::vector<unsigned char> _memory;//用于自动分配的
	size_t _group_id;
	size_t _count;//现在的mission计数
	size_t _mission_count;//预定义的计数
	size_t _length;//若参数长度为常数则不为0,DEBUG模式下会检查是否有意义

	template<class... Arg>
	size_t _write(Arg&... arg);//内存不足时会自动分配

};

#define RPC_GENERIC_SYNC_ARG rpc_group_client* group,const sockaddr_in& addr,\
	func_ident num,Arg... arg

#define RPC_GENERIC_ASYNC_ARG rpc_group_client* group,const sockaddr_in& addr,\
	rpc_callback fn,sysptr_t callback_arg,\
	func_ident num,Arg... arg

class rpc_client_tk
	:mynocopyable
{
public:
	//socket类型,client的地址,允许rpc_client作为manager
	rpc_client_tk(client_mode mode, const sockaddr_in& client_addr,
		const sockaddr_in& server_addr, rpc_client* mgr = nullptr);

#define RPC_GEN_BASIC \
	 rpc_head head=_make_rpc_head(num);\
	 auto pair=_archive(head,arg...);\

	 struct rpc_request
	 {
		 rpc_request_msg msg;
		 rpc_s_result result;
	 };

	template<class... Arg>
	rpc_s_result rpc_generic(RPC_GENERIC_SYNC_ARG)
	{
		RPC_GEN_BASIC;
		auto& arg = _model_ready_sync_rpc(head, arg...);
		using namespace std;
		arg.buffer = reinterpret_cast<const char*>(pair.buffer);
		arg.size = pair.size;
		arg.addr = addr;

		auto request = _wait_sync_request(head,
			_send_rpc(head.id, addr, pair.buffer, pair.size));

		return request.result;
	}

	template<class... Arg>
	void rpc_generic_async(RPC_GENERIC_ASYNC_ARG)
	{
		RPC_GEN_BASIC;
		_ready_async_rpc(head, fn, callback_arg);
		_send_rpc(addr, pair.buffer, pair.size);
	}


	//client 线程,可以提供个初始化锁以确定初始化完成
	void operator()(void* initlock = nullptr);
	
	//
	int socktype() const;
	sockaddr_in address() const;
	size_t call_count() const;
	size_t async_waiting() const;
	size_t block_watting() const;
	rpc_client* mgr() const;

private:
	typedef rpc_s_result archive_result;

	//弹性模块
	
	template<class... Arg>
	archive_result _model_ready_sync_rpc(const rpc_head&,
		Arg... arg);//rpc调用所需要的参数即可

	template<class... Arg>
	archive_result _model_ready_async_rpc(const rpc_head&,
		Arg... arg, rpc_callback);//rpc调用所需要的参数即可

	//model_split_rcpback的任务:确定是那个rpcback并调用目标
	void _model_split_rpcback(rpc_s_result);
	//
	rpc_client* _mgr;

	client_mode _mode;
	sockaddr_in _default_server;//tcp,udp的默认服务器
	rpcudp _udpsock;
	socket_type _tcpsock;
	rpc_group_client _default_group;//当用于没有指定组的时候用这个
	//该组永远不会把rpc调用给扣下来
	typedef std::atomic_size_t asize_t;

	asize_t _rpc_count;//从0~asize_t的最大值，当超过最大值时从0开始
	asize_t _cycle_of_rpc_count;//MSG:名字有点长,这是用来几率rpc_count循环了几次

	rpcid _make_rpcid(func_ident);//调用编号和函数编号的混合体
	size_t _alloc_callid();//调用编号

	rwlock _async_rpc_lock;
	std::hash_map<rpcid, rpc_callback> _async_rpcs;//记录异步rpc编号,tcp独占模式下无效

	rwlock _sync_rpc_lock;

	struct sync_arg
	{
		sync_arg();
		sync_arg(const void* buffer, size_t size);

		const char* buffer;
		size_t size;
		rwlock* lock;
		rpc_s_result result;
		size_t req;
		sockaddr_in addr;
	};
	std::hash_map<rpcid, sync_arg*> _sync_rpcs;//记录同步的编号,tcp独占下无效

	void _init(client_mode, const sockaddr_in&, const sockaddr_in&, rpc_client*);
	void _initsock(const sockaddr_in&,const sockaddr_in&);

	rpc_head _make_rpc_head(func_ident num);

	template<class... Arg>
	archive_result _archive(const rpc_head& head, Arg... arg)
	{
		return _ser_cast(archive(head, arg...));
	}

	static bool _is_async_call(const rpc_head&);
#define SRPC_ARG const sockaddr_in& server,const void* buf,size_t bufsize
	bool _send_rpc(rpcid id, SRPC_ARG);//返回用tcp还是udp发送数据包

	sync_arg& _ready_sync_rpc(const rpc_head&);
	void _sync_rpc_end(const rpc_head&, const rpc_s_result&);

	void _ready_async_rpc(const rpc_head&,rpc_callback,sysptr_t);
	void _async_rpc_end(const rpc_head&, const rpc_s_result&);

	void _send_udprpc(rpcid id, SRPC_ARG);

	rpc_request _wait_sync_request(const rpc_head&,bool send_by_udp);//异步rpc无此类型函数
	rpc_s_result _finished_udprpc(rpcid);//内部还有延时erase功能

	rpctimer _timer;
	void _register_udpack(const rpc_head&,ulong64 timeout=300);


	void _send_tcprpc(SRPC_ARG);
	//utility 节约代码用的
	archive_result _ser_cast(const std::pair<void*, size_t>&);
	size_t _big_packet();
	static bool _is_multicast(const sockaddr_in&);
	static bool _is_same(const sockaddr_in& rhs, const sockaddr_in& lhs);
	std::pair<rpc_head, rpc_s_result> _rarchive(const void* buf, size_t bufsize);
	static void _adjbuf_impl(void*, size_t);
	template<class T,class... Arg>
	static void _adjbuf_impl(void*& buf, size_t& bufsize, const T& val, Arg... arg)
	{
		buf = reinterpret_cast<char*>(buf) +sizeof(val);
		bufsize -= sizeof(val);
		_adjbuf_impl(buf, bufsize, arg...);
	}

	template<class... Arg>
	static rpc_s_result _adjbuf(void* buf,size_t bufsize, Arg... arg)
	{
		_adjbuf_impl(buf, bufsize, arg...);
		rpc_s_result ret;
		ret.buffer = buf;
		ret.size = bufsize;
		return ret;
	}

	void _rpctimer_callback(size_t timer_id, sysptr_t arg);
	static bool _have_ack(const sync_arg&);
	void _resend(rpcid);

	void _rpc_return_process(const rpc_head&, 
		rpc_request_msg msg, rpc_s_result argbuf);
	void _rpc_ack_process(const rpc_head&);
};

class rpc_client
	:mynocopyable
{
public:
	typedef unsigned long thread_id;

	//server的sock类型必须和socktype一样
	rpc_client(client_mode mode,
		const sockaddr_in& server, size_t client_limit = 0);

#define RGEN(name) _get_client().name(_address(addr),num,arg...)
	template<class... Arg>
	rpc_s_result rpc_generic(RPC_GENERIC_SYNC_ARG)
	{
		return RGEN(rpc_generic);
	}

	template<class... Arg>
	void rpc_generic_async(RPC_GENERIC_ASYNC_ARG)
	{
		return RGEN(rpc_generic_async);
	}
#undef RGEN

	template<class ClientType>
	void init_clients()
	{
		_init_clients_impl([&](const sockaddr_in& addr, rpc_client* mgr)
			->rpc_client_tk*
		{
			auto* ret = new ClientType(_mode, addr, _default_server, mgr);
			return ret;
		});
	}

	void add_client(rpc_client_tk* client);//WARNING:无论实现如何,client必须能被delete且释放掉资源
	//性能数据提供
	const sockaddr_in& default_server() const;
	void set_default_server(const sockaddr_in&);
	const size_t client_count() const;
private:

	friend class rpc_client_tk;
	rwlock* getlock();

private:
	client_mode _mode;

	sockaddr_in _default_server;//rpc服务器地址,tcp下必须有效,udp下可用户在rpc调用时指定

	typedef std::atomic_size_t asize_t;

	//TODO:_next_index and _get_client可以写的更细一点，以带来更好平衡负载
	size_t _next_index();
	asize_t _alloc_loop;
	size_t _limit_of_client;

	rpc_client_tk& _get_client();
	//WARNING:_src和_locks必须在第一次rpc调用前初始化完成,并且完成后不能被修改
	std::vector<rpc_client_tk*> _src;
	rwlock _lock;
	std::hash_map<thread_id, rwlock> _locks;//udp下才有意义,用来阻塞rpc调用线程

	void _init(client_mode mode, const sockaddr_in& addr, size_t limit);
	void _async(rpc_client_tk&, initlock*);//udp和混合模式下initlock必须有效

	//utility :杂项，用来减少代码重复度的东西罢了
	const sockaddr_in& _address(const sockaddr_in* addr);
	void _init_clients_impl(
		std::function<rpc_client_tk*(const sockaddr_in&, rpc_client*)> client_maker);

};

#endif