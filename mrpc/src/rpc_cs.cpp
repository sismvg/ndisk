
#include "../rpc_cs.hpp"

#include <assert.h>

#include <thread>
#include <mrpc_utility.hpp>
#include <iostream>

rpc_client_tk::rpc_client_tk(client_mode mode, const sockaddr_in& addr,
	const sockaddr_in& server_addr, rpc_client* mgr)
	:_rpc_count(0), _default_group(0,1,
	GRPACK_IS_SINGAL_PACK|GRPACK_SYNCPACK_FORCE_SIZE)
{
	_init(mode, addr, server_addr, mgr);
}

sockaddr_in rpc_client_tk::address() const
{//TODO:tcp udp mix没有实现
	sockaddr_in addr;
	return addr;
}

ulong64 rpc_client_tk::call_count() const
{
	return _rpc_count;
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
	ret.info = nullptr;
	return ret;
}

rpc_client_tk::rpc_request 
rpc_client_tk::_wait_sync_request(const rpc_head& head,
	sync_rpcinfo* info, bool send_by_udp)
{
	rpc_request ret;
	if (send_by_udp)
	{
		auto* lock = info->locke;
		lock->write_lock();
		ret.result = info->result;
		lock->write_unlock();
	}
	else
	{
		char buf[DEFAULT_RPC_BUFSIZE];
		int recvsize = recv(_tcpsock, buf, DEFAULT_RPC_BUFSIZE, 0);
		int va = WSAGetLastError();
		const_memory_block blk;
		blk.buffer = buf;
		blk.size = recvsize;
		_rpc_process(blk);
		ret.result = info->result;
	}
	ret.msg = rpc_success;
	return ret;
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
	int val = WSAGetLastError();
	if (_mode&tcp)
	{
		connect(_tcpsock,
			reinterpret_cast<const sockaddr*>(&server_addr), sizeof(server_addr));
		val = WSAGetLastError();
		int p = 10;
	}
}

rpcinfo& rpc_client_tk::_ready_rpc(const rpc_head&, bool is_async /* = false */)
{
	rpcinfo* ret = nullptr;
	if (is_async)
	{
		auto* ptr = new async_rpcinfo;
		ret = ptr;
	}
	else
	{
		auto* ptr = new sync_rpcinfo;
		ptr->locke = _mgr->getlock();
		ret = ptr;
		ptr->locke->write_lock();
	}
	ret->flag = 0;
	return *ret;
}


rpcid rpc_client_tk::_make_rpcid(unsigned int number)
{
	rpcid ret;
	ret.funcid = number;
	ret.id = _alloc_callid() | ID_ISSYNC;
	return ret;
}

ulong64 rpc_client_tk::_alloc_callid()
{
	return ++_rpc_count;
}

void rpc_client_tk::_send_udprpc(rpcid id, SRPC_ARG)
{
	_udpsock.sendto(id, blk.buffer, blk.size,
		0, server, sizeof(server));
}

void rpc_client_tk::_send_tcprpc(SRPC_ARG)
{
	if (_mode&tcp&&_is_multicast(server))
		throw std::invalid_argument("send tcprpc not"\
		" support multicast address");
	send(_tcpsock, 
		reinterpret_cast<const char*>(blk.buffer), blk.size, 0);
	int val = WSAGetLastError();
}

bool rpc_client_tk::_send_rpc(rpc_group_client& group, rpcid id, SRPC_ARG)
{
	if (group.standby())
	{
		int addr_length = sizeof(server);

		auto _tmp_mode = _mode;

#ifdef _DEBUG
		//当mix模式下的时候tcp和udp都只能指向同一个地址
		if (_is_same(server, _default_server))
			throw std::invalid_argument("When rpc client mode is tcp"\
			",the server address parment must be same with default_server");
#endif

		//if (blk.size >= _big_packet())
			//_tmp_mode = tcp;
		if (_tmp_mode & tcp)
		{
			_send_tcprpc(server, blk);
		}
		else if (_tmp_mode & udp)
		{
			_send_udprpc(id, server, blk);
		}
		group.reset();
		return _tmp_mode & udp;
	}
	return false;
}

void rpc_client_tk::_rpc_process(const_memory_block argbuf)
{
	rpc_group_middleware group_info(argbuf);
	size_t index = 0;
	group_info.split_group_item(
		[&](const rpc_head& head,const_memory_block blk)
		->size_t
	{
		size_t ret = 0;
		rpc_request_msg msg;
		advance_in(blk, rarchive(blk.buffer, blk.size, msg));
		if (msg == rpc_success)
		{
			ret= _rpc_end(head, blk, _is_async_call(head));
		}
		using namespace std;
		group_info.fin();
		return ret + sizeof(msg);
	});
}

size_t rpc_client_tk::_rpc_end(const rpc_head& head, 
	const_memory_block argbuf, bool is_async /* = false */)
{
	rpcinfo* info = nullptr;
	size_t ret = 0;
	if (!is_async)
	{
		sync_rpcinfo* _1 = reinterpret_cast<sync_rpcinfo*>(head.info);
		_1->result.buffer = new char[argbuf.size];
		_1->result.size = argbuf.size;
		memcpy(const_cast<void*>(_1->result.buffer), argbuf.buffer, argbuf.size);
		_1->locke->write_unlock();
		return 4;
	}
	else
	{
		auto* _1 = static_cast<async_rpcinfo*>(head.info);
		_1->callback(_1->parment);
		ret = 4;
	}
	return ret;
}

void rpc_client_tk::operator()(void* lock)
{//WARNING:会参与到mgr的初始化中
	initlock* mylock = reinterpret_cast<initlock*>(lock);

	char argbuf[DEFAULT_RPC_BUFSIZE];
	unsigned int bufsize = DEFAULT_RPC_BUFSIZE;

	sockaddr_in sender;
	int sender_length = sizeof(sender);
	mylock->fin();
	if (_mode&tcp)
	{
		while (true)
		{
			char buf[DEFAULT_RPC_BUFSIZE];
			int recvsize = recv(_tcpsock, buf, DEFAULT_RPC_BUFSIZE, 0);
			int va = WSAGetLastError();
			const_memory_block blk;
			blk.buffer = buf;
			blk.size = recvsize;
			_rpc_process(blk);
		}
	}
	else
	{
		while (true)
		{
			int dgrmsize = _udpsock.recvfrom(argbuf, bufsize, 0,
				sender, sender_length);
			auto cdoe = WSAGetLastError();
			if (dgrmsize != SOCKET_ERROR)
			{
				const_memory_block rpc_back;
				rpc_back.buffer = argbuf;
				rpc_back.size = dgrmsize;
				//TODO:对大型包可以异步
				_rpc_process(rpc_back);
			}
		}
	}
}

size_t rpc_client_tk::_big_packet()
{
	return 1450;
}

bool rpc_client_tk::_is_multicast(const sockaddr_in& addr)
{
	return false;
}

bool rpc_client_tk::_is_same(const sockaddr_in& rhs, const sockaddr_in& lhs)
{
	return memcmp(&rhs, &lhs, sizeof(rhs)) == 1;
}

bool rpc_client_tk::_is_async_call(const rpc_head& head)
{
	return !(head.id.id&ID_ISSYNC);
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
