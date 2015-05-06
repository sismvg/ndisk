
#include <archive.hpp>

#include <rpc_server.hpp>
#include <iostream>
#include <thread>
#include <mrpc_utility.hpp>
#include <rpclock.hpp>
#include <rpcudp.hpp>

rpc_server::rpc_server(server_mode mode, const sockaddr_in& addr,
	size_t backlog, size_t service_thread_limit /* = 0 */)
	:_server_addr(addr), _mode(mode),
	_backlog(backlog), _server_thread_limit(service_thread_limit)
{
	if (_server_thread_limit == 0)
		_server_thread_limit = cpu_core_count();
	if (_backlog == 0)
		_backlog = 5;
}

rpc_server::~rpc_server()
{
	stop();
}

void rpc_server::start(size_t thread_open/* =1 */)
{
	assert(thread_open != 0);
	initlock lock(thread_open);
	lock.ready_wait();
	while (thread_open--)
	{
		std::thread thr([&](initlock* lock)
		{
			if (_mode&tcp)
				_tcp_server(lock);//TODO:要让_tcp&udp_server是异步的
			if (_mode&udp)
				_udp_server(lock);
		}, &lock);
		thr.detach();
	}
	lock.wait();
	//Sleep(100);
}

void rpc_server::stop()
{
	//TODO:实现它
} 
//impl

socket_type rpc_server::_initsock(int socktype)
{
	socket_type sock = socket(AF_INET, socktype, 0);
	bind(sock, reinterpret_cast<const sockaddr*>(&_server_addr),
		sizeof(_server_addr));
	return sock;
}

void rpc_server::_tcp_server(initlock* lock)
{
	_async_call([&]()
	{
		rpcudp tcpsock(_initsock(SOCK_STREAM));
		listen(tcpsock.socket(), _backlog);

		//
		sockaddr_in client_addr;
		int client_length = sizeof(client_addr);
		if (lock)
			lock->fin();
		rpcudp client(_rpc_accept(tcpsock, client_addr, client_length));
		
		_async_call([&]()
		{
			_tcp_client_process(client, client_addr, client_length);
		});
	});
}

void rpc_server::_udp_server(initlock* lock)
{
	_async_call([&]()
	{
		rpcudp udpsock(_initsock(SOCK_DGRAM));
		
		sockaddr_in client_addr;
		int client_length = sizeof(client_addr);

		char buffer[DEFAULT_RPC_BUFSIZE];
		int bufsize = DEFAULT_RPC_BUFSIZE;
		if (lock)
			lock->fin();
		while (true)
		{
			int dgrmsize = udpsock.recvfrom(buffer, bufsize, 0,
				client_addr, client_length);
			if (dgrmsize != SOCKET_ERROR)
			{
				_udp_client_process(udpsock, client_addr,
					client_length, buffer, dgrmsize);
			}
		}
	});
}

socket_type rpc_server::_rpc_accept(
	rpcudp& server, sockaddr_in& addr, int& addr_length)
{
	socket_type client=
		accept(server.socket(), reinterpret_cast<sockaddr*>(&addr), &addr_length);
	//TODO:可以加入到别的地方去
	return client;
}
void rpc_server::_tcp_client_process(rpcudp& client,
	const sockaddr_in& addr, int addrlen)
{
	char buffer[DEFAULT_RPC_BUFSIZE];
	char bufsize = DEFAULT_RPC_BUFSIZE;

	int dgrmsize = recv(client.socket(), buffer, bufsize, 0);
	_client_process(client, addr, addrlen, buffer, dgrmsize, false);
}

#define RPC_REQUEST_ARGS head,msg
void rpc_server::_client_process(rpcudp& sock,
	const sockaddr_in& addr, int addrlen,
	const void* buffer, size_t bufsize, bool send_by_udp)
{
	rpc_head head;
	rpc_s_result argbuf;
	rarchive(buffer, bufsize, head);

	const auto* cbuf = reinterpret_cast<const char*>(buffer);
	argbuf.buffer = const_cast<char*>(cbuf + sizeof(head));//WARNING:const_cast
	argbuf.size = bufsize - sizeof(head);

	rpcid_un un(head.id);

	rpc_request_msg msg;
	auto iter = rpc_local().find(un.number());
	if (iter == rpc_local().end())
	{
		msg = rpc_error;
		//send back
	}
	else
	{
		_async_call([&]()
		{
			msg = rpc_success;
			if (false)//send_by_udp)
			{
				rpc_s_result reqbuf;
				auto pair = archive(rpc_ack, head);
				reqbuf.buffer = pair.first;
				reqbuf.size = pair.second;
				size_t type = 0;
				rarchive(pair.first,pair.second,type);
				_send(head.id, sock, reqbuf, addr, addrlen, true);
			}
			int val =
				iter->second(/*RPC_REQUEST_ARGS, */argbuf);
			rpc_s_result archived_result;
			auto pair = archive(head, msg, val);
			archived_result.buffer = pair.first;
			archived_result.size = pair.second;
			if (false)//send_by_udp)
			{
				rpc_arg arg;
				arg.buffer = pair.first;
				arg.size = pair.second;
			//.//	arg.sock = sock;
				arg.addr = addr;
				arg.req = 0;
				//_register_ack(head, arg);
			}
			_send(head.id, sock, archived_result,
				addr, addrlen, send_by_udp);
		});
	}
}

bool rpc_server::_check_ack(rpcid id)
{
	auto rlock = ISU_AUTO_RLOCK(_lock);
	auto iter = _map.find(id);
	if (iter != _map.end())
	{
		rpc_s_result argbuf;
		auto& arg = iter->second;
		if (arg.req ==0)//表示没有ack
		{
			argbuf.buffer = const_cast<void*>(arg.buffer);
			argbuf.size = arg.size;
			//_send(arg.sock, argbuf, arg.addr, sizeof(arg.addr), true);
			return false;
		}
		else
		{
			rlock.unlock();
			auto wlock = ISU_AUTO_WLOCK(_lock);
			_map.erase(iter);
			return true;
		}
	}
	return true;
}

void rpc_server::_register_ack(const rpc_head& head,const rpc_arg& arg)
{
	auto wlock = ISU_AUTO_WLOCK(_lock);
	auto iter = _map.find(head.id);
	if (iter == _map.end())
	{
		_map.insert(std::make_pair(head.id, arg));
	}
}

void rpc_server::_rpc_ack_callback(size_t id, sysptr_t arg)
{
}

void rpc_server::_udp_client_process(rpcudp& udpsock,
	const sockaddr_in& addr, int addrlen, const void* buffer, size_t bufsize)
{//MSG:I know ,argument list is too long.
	_client_process(udpsock, addr, addrlen, buffer, bufsize, true);
}

void rpc_server::_send(
	rpcid id,
	rpcudp& sock, const rpc_s_result& argbuf, 
	const sockaddr_in& addr, int addrlen, bool send_by_udp)
{
	const char* cbuf = reinterpret_cast<const char*>(argbuf.buffer);
	size_t bufsize = argbuf.size;
	const auto* myaddr = reinterpret_cast<const sockaddr*>(&addr);
	if (!send_by_udp)
	{
		send(sock.socket(), cbuf, bufsize, 0);
	}
	else
	{
		if (argbuf.size >= _big_packet() && _mode&tcp)
		{
			//auto& sock1 = _connected[_make_sockaddr_key(addr)];
		//..	send(sock1.socket(), cbuf, bufsize, 0);
		}
		else
		{
			sock.sendto(id, cbuf, bufsize, 0, addr, addrlen);
		}
	}
}

size_t rpc_server::_big_packet()
{
	return 1726;
}

rpc_server::sockaddr_key 
	rpc_server::_make_sockaddr_key(const sockaddr_in& addr)
{
	sockaddr_key key = 0;
	//TODO:实现
	return key;
}

std::hash_map<func_ident, rpc_server_call>& rpc_server::rpc_local()
{
	static std::hash_map<func_ident, rpc_server_call> _rpc_local;
	return _rpc_local;
}

void rpc_server::register_rpc(func_ident id, rpc_server_call call)
{
	rpc_local().insert(std::make_pair(id, call));
}