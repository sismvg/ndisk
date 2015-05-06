
#include "../rpc_cs.hpp"

#include <assert.h>

#include <thread>
#include <mrpc_utility.hpp>
#include <iostream>

rpc_client_tk::sync_arg::sync_arg()
	:size(0), req(0), lock(nullptr)
{}

rpc_client_tk::rpc_client_tk(client_mode mode, const sockaddr_in& addr,
	const sockaddr_in& server_addr, rpc_client* mgr)
	:
	_timer([&](size_t id,sysptr_t arg)
	{
	//	_rpctimer_callback(id, arg);
	})
{
	_init(mode, addr, server_addr, mgr);
}

sockaddr_in rpc_client_tk::address() const
{//TODO:tcp udp mix没有实现
	sockaddr_in addr;
	return addr;
}

size_t rpc_client_tk::call_count() const
{
	return _rpc_count + (_cycle_of_rpc_count + 1)*size_t(-1);
}

#define AOSYNC_IMPL(name) \
	auto* mythis = const_cast<rpc_client_tk*>(this);\
	auto rlock = ISU_AUTO_RLOCK(mythis->name##_lock);\
	auto ret = name##s.size();\
	return ret

size_t rpc_client_tk::async_waiting() const
{
	AOSYNC_IMPL(_async_rpc);
}

size_t rpc_client_tk::block_watting() const
{
	AOSYNC_IMPL(_sync_rpc);
}

rpc_client* rpc_client_tk::mgr() const
{
	return _mgr;
}
//impl

rpc_head rpc_client_tk::_make_rpc_head(func_ident num)
{
	rpc_head ret;
	ret.id = _make_rpcid(num);
	ret.ver = 0;
	return ret;
}

void rpc_client_tk::_register_udpack(const rpc_head& head,ulong64 timeout)
{
}

void rpc_client_tk::_rpctimer_callback(size_t timer_id, sysptr_t arg)
{
}

bool rpc_client_tk::_have_ack(const sync_arg& arg)
{
	return arg.req > 0;
}

void rpc_client_tk::_resend(rpcid id)
{
	//必须保证已经上过锁了
	auto iter = _sync_rpcs.find(id);
	if (iter != _sync_rpcs.end())
	{
		const auto& arg = *iter->second;
		_send_rpc(id, arg.addr, arg.buffer, arg.size);
	}
}

rpc_client_tk::rpc_request 
	rpc_client_tk::_wait_sync_request(const rpc_head& head,bool send_by_udp)
{
	rpc_request ret;
	if (send_by_udp)
	{
		auto* lock = _mgr->getlock();
		lock->write_lock();
		ret.result = _finished_udprpc(head.id);
		lock->write_unlock();
	}
	else
	{
		char buf[DEFAULT_RPC_BUFSIZE];
		int recvsize = recv(_tcpsock, buf, DEFAULT_RPC_BUFSIZE, 0);
		//TODO:代码精简
		rpc_head head;
		rpc_request_msg msg;
		rarchive(buf, recvsize, head, msg);
		auto pair = _adjbuf(buf, recvsize, head, msg);
		if (msg == rpc_success)
		{
			auto* tmp = new char[pair.size];
			memcpy(tmp, pair.buffer, pair.size);
			pair.buffer = tmp;
			ret.result = pair;
		}
	}
	ret.msg = rpc_success;
	return ret;
}

rpc_s_result rpc_client_tk::_finished_udprpc(rpcid id)
{
	auto rlock = ISU_AUTO_RLOCK(_sync_rpc_lock);

	auto iter = _sync_rpcs.find(id);
	assert(iter != _sync_rpcs.end());
	auto result = iter->second->result;
	rlock.unlock();
	return result;
}

void rpc_client_tk::_init(client_mode mode, const sockaddr_in& addr,
	const sockaddr_in& server_addr, rpc_client* mgr)
{
	_mgr = mgr;
	_mode = mode;
	_default_server = server_addr;
	_initsock(addr, server_addr);
}

void rpc_client_tk::_initsock(const sockaddr_in& addr,
	const sockaddr_in& server_addr)
{
	socket_type sock = 0;
	if (_mode&tcp)
	{
		sock = _tcpsock = socket(AF_INET, SOCK_STREAM, 0);
	}
	if (_mode&udp)
	{
		_udpsock.setsocket(socket(AF_INET, SOCK_DGRAM, 0));
		sock = _udpsock.socket();
	}
	bind(sock, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
	if (_mode&tcp)
	{
		connect(_tcpsock,
			reinterpret_cast<const sockaddr*>(&server_addr), sizeof(server_addr));
	}
}

rpc_client_tk::sync_arg& 
	rpc_client_tk::_ready_sync_rpc(const rpc_head& head)
{
	auto wlock = ISU_AUTO_WLOCK(_sync_rpc_lock);

	auto* lock = _mgr->getlock();
	sync_arg* arg = new sync_arg;
	arg->lock = lock;
	auto iter = _sync_rpcs.insert(std::make_pair(head.id, arg));
	sync_arg& result = *_sync_rpcs[head.id];
	lock->write_lock();
	return result;
}

void rpc_client_tk::_sync_rpc_end(const rpc_head& head, const rpc_s_result& argbuf)
{
	auto wlock = ISU_AUTO_WLOCK(_sync_rpc_lock);
	auto iter = _sync_rpcs.find(head.id);
	if (iter == _sync_rpcs.end())
	{
		//TODO:错误处理
		logit("出错了");
		return;
	}
	auto& arg = *iter->second;
	arg.result = argbuf;
	using namespace std;
	auto* lock = arg.lock;
	wlock.unlock();//MSG:这里是用来防止下面的write_lock解锁导致的线程切换问题
	lock->write_unlock();
}

void rpc_client_tk::_async_rpc_end(const rpc_head& head, const rpc_s_result& pack)
{
	//TODO:异步rpc是否需要保存rpc函数的返回值(这样的话会导致需要函数对象(32bit))
	auto rlock = ISU_AUTO_RLOCK(_async_rpc_lock);
	auto iter = _async_rpcs.find(head.id);
	if (iter == _async_rpcs.end())
	{//TODO:错误处理
		return;
	}
	auto fn = iter->second;
	rlock.unlock();
	//TODO:可以让fn异步
	fn(nullptr);
}

void rpc_client_tk::_adjbuf_impl(void* buf, size_t bufsize)
{}

void rpc_client_tk::_ready_async_rpc(const rpc_head& head, 
	rpc_callback fn,sysptr_t ptr)
{
	auto wlock = ISU_AUTO_WLOCK(_async_rpc_lock);
	_async_rpcs.insert(std::make_pair(head.id, fn));
}

rpcid rpc_client_tk::_make_rpcid(unsigned int number)
{
	rpcid_un un(number, _alloc_callid());
	auto id= un.to_rpcid();
	return id;
}

size_t rpc_client_tk::_alloc_callid()
{
	++_rpc_count;
	if (_rpc_count == size_t(-1))
	{
		_rpc_count = 0;
		++_cycle_of_rpc_count;
	}
	return _rpc_count;
}

void rpc_client_tk::_send_udprpc(rpcid id, SRPC_ARG)
{
	_udpsock.sendto(id, buf, bufsize, 0, server, sizeof(server));
}

void rpc_client_tk::_send_tcprpc(SRPC_ARG)
{
	if (_mode&tcp&&_is_multicast(server))
		throw std::runtime_error("");
	send(_tcpsock, reinterpret_cast<const char*>(buf), bufsize, 0);
}

bool rpc_client_tk::_send_rpc(rpcid id,SRPC_ARG)
{
	int addr_length = sizeof(server);

	auto _tmp_mode = _mode;

#ifdef _DEBUG
	//当mix模式下的时候tcp和udp都只能指向同一个地址
	if (_is_same(server, _default_server))
		throw std::runtime_error("");
#endif

	if (bufsize >= _big_packet())
		_tmp_mode = tcp;
	if (_tmp_mode == tcp)
	{
		_send_tcprpc(server, buf, bufsize);
	}
	else if (_tmp_mode == udp)
	{
		_send_udprpc(id, server, buf, bufsize);
	}
	return _tmp_mode == udp;
}

void rpc_client_tk::_rpc_return_process(const rpc_head& head,
	rpc_request_msg msg, rpc_s_result argbuf)
{
	if (msg == rpc_success)
	{
		//分成异步call和同步call两种
		if (_is_async_call(head))
		{
			_async_rpc_end(head, argbuf);
		}
		else
		{
			auto* tmp = new char[argbuf.size];//TODO:妥协而已
			memcpy(tmp, argbuf.buffer, argbuf.size);
			argbuf.buffer = tmp;//MSG:Nothing error
			auto* mytmp = new rpc_s_result(argbuf);
			_sync_rpc_end(head, *mytmp);
		}
	}
}

void rpc_client_tk::_rpc_ack_process(const rpc_head& head)
{
	auto rlock = ISU_AUTO_RLOCK(_sync_rpc_lock);
	auto iter = _sync_rpcs.find(head.id);
	auto& arg = iter->second;
	arg->req = 1;
}

void rpc_client_tk::operator()(void* lock)
{//WARNING:会参与到mgr的初始化中
	initlock* mylock = reinterpret_cast<initlock*>(lock);

	char argbuf[DEFAULT_RPC_BUFSIZE];
	unsigned int bufsize = DEFAULT_RPC_BUFSIZE;

	sockaddr_in sender;
	int sender_length = sizeof(sender);
	mylock->fin();
	while (true)
	{
		int dgrmsize = _udpsock.recvfrom(argbuf, bufsize, 0,
			sender, sender_length);
		auto cdoe = WSAGetLastError();
		if (dgrmsize != SOCKET_ERROR)
		{
			rpc_head head;
			rpc_request_msg msg = rpc_error;
			rpc_s_result rpc_back;

			rpc_back = split_pack(argbuf, bufsize, head, msg);
			_rpc_return_process(head, msg, rpc_back);
		}
	}
}

rpc_client_tk::archive_result 
	rpc_client_tk::_ser_cast(const std::pair<void*, size_t>& arg)
{
	archive_result ret;
	ret.buffer = arg.first;
	ret.size = arg.second;
	return ret;
}

size_t rpc_client_tk::_big_packet()
{
	return 1726;
}

bool rpc_client_tk::_is_multicast(const sockaddr_in& addr)
{
	return false;
}

bool rpc_client_tk::_is_same(const sockaddr_in& rhs, const sockaddr_in& lhs)
{
	return memcmp(&rhs, &lhs, sizeof(rhs)) == 1;
}

std::pair<rpc_head, rpc_s_result> 
	rpc_client_tk::_rarchive(const void* buf, size_t bufsize)
{
	rpc_head head;
	rarchive(buf, bufsize, head);
	return std::make_pair(head, _adjbuf(const_cast<void*>(buf), bufsize, head));
}

bool rpc_client_tk::_is_async_call(const rpc_head& head)
{
	return head.ver == 1;
}

//rpc_client

rpc_client::rpc_client(client_mode mode,
	const sockaddr_in& server, size_t client_limit)
{
	_init(mode, server, client_limit);
}

rwlock* rpc_client::getlock()
{
	auto rlock = ISU_AUTO_RLOCK(_lock);
	auto iter = _locks.find(GetCurrentThreadId());
	rwlock* ret = nullptr;
	if (iter == _locks.end())
	{
		rlock.unlock();
		auto wlock = ISU_AUTO_RLOCK(_lock);
		ret = &_locks[GetCurrentThreadId()];
	}
	else
	{
		ret = &iter->second;
	}
	return ret;
}

void rpc_client::add_client(rpc_client_tk* client)
{
	//TODO:线程安全问题
	if (_src.size() >= _limit_of_client)
		throw std::runtime_error("");
	_src.push_back(client);
}

const sockaddr_in& rpc_client::default_server() const
{
	return _default_server;
}

void rpc_client::set_default_server(const sockaddr_in& addr)
{
	//TODO:这里处理比较麻烦
}

const size_t rpc_client::client_count() const
{
	return _src.size();
}

//impl
void rpc_client::_init(client_mode mode, const sockaddr_in& addr, size_t limit)
{
	_mode = mode;
	_default_server = addr;
	_limit_of_client = limit == 0 ? cpu_core_count() : limit;
}

void rpc_client::_async(rpc_client_tk& client,initlock* mylock)
{
	std::thread thr([=,&client](initlock* lock)
	{
		client(lock);
	},mylock);
	thr.detach();
}

const sockaddr_in& rpc_client::_address(const sockaddr_in* addr)
{
	return addr == nullptr ? _default_server : *addr;
}

rpc_client_tk& rpc_client::_get_client()
{
	return *_src[_next_index()];
}

size_t rpc_client::_next_index()
{
	if (_alloc_loop >= _src.size())
		_alloc_loop = 0;
	return _alloc_loop++;
}

void rpc_client::_init_clients_impl(
	std::function<rpc_client_tk*(const sockaddr_in&, rpc_client*)> client_maker)
	//MSG:such bad
{
	sockaddr_in addr;
	addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	addr.sin_family = AF_INET;

	initlock lock(_limit_of_client);
	lock.ready_wait();
	for (auto index = 0; index != _limit_of_client; ++index)
	{
		addr.sin_port = htons(RPC_SERVER_PORT + index + 1);
		_src.push_back(client_maker(addr, this));
		_async(*_src.back(), &lock);
	}
	lock.wait();
}
