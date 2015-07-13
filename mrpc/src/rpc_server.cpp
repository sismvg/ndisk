
#include <thread>

#include <archive.hpp>

#include <rpc_server.hpp>
#include <active_event.hpp>
#include <mrpc_utility.hpp>

rpc_server::rpc_server(transfer_mode mode, const udpaddr& addr,
	size_t service_thread_limit)
	:
		_mode(mode), 
		_local(addr), 
		_server_thread_limit(service_thread_limit)
{
	startup_network_api();
	if (_server_thread_limit == 0)
		_server_thread_limit = cpu_core_count();
}
rpc_server::~rpc_server()
{
	stop();
	close_network_api();
}

void rpc_server::run(size_t thread_open/* =1 */)
{
	assert(thread_open != 0);
	active_event event(thread_open);
	event.ready_wait();
	while (thread_open--)
	{
		std::thread* ptr= 
			new std::thread([&](active_event* eve)
		{
			if (_mode&transfer_by_tcp)
				_tcp_server(eve);
			if (_mode&transfer_by_udp)
				_udp_server(eve);
		}, &event);
		_threads.push_back(ptr);
	}
	event.wait();
}

void rpc_server::stop()
{
	//TODO:实现它
	auto wlock = ISU_AUTO_WLOCK(_socks_lock);
	std::for_each(_socks.begin(), _socks.end(), closesocket);
	std::for_each(_threads.begin(), _threads.end(),
		[&](std::thread* ptr)
	{
		ptr->join();
		delete ptr;
	});
} 

//impl

socket_type rpc_server::_initsock(int socktype)
{
	socket_type sock = socket(AF_INET, socktype, 0);
	bind(sock, reinterpret_cast<const sockaddr*>(&_local), sizeof(_local));

	auto wlock = ISU_AUTO_WLOCK(_socks_lock);

	_socks.push_back(sock);

	wlock.unlock();

	return sock;
}

void rpc_server::_tcp_server(active_event* event)
{
	int tcpsock(_initsock(SOCK_STREAM));
	listen(tcpsock, 5);//WARNING: magic number

	udpaddr client_addr;
	int client_length = sizeof(client_addr);

	if (event)
		event->active();

	int client(_rpc_accept(tcpsock, client_addr, client_length));

	_tcp_client_process(client, client_addr);
}

void rpc_server::_udp_server(active_event* eve)
{
	udt udpsock(_local);

	if (eve)
		eve->active();

	while (true)
	{
		udt_recv_result data = udpsock.recv();

		const io_result& error = data.io_state;
		if (!error.in_vaild())
		{
			if (error.system_socket_error() == WSAENOTSOCK)
				break;
			mylog.log(log_error,
				"udp server have socket_error:", 
					error.system_socket_error());
		}

		const_memory_block blk = data.memory;
		if (blk.buffer == nullptr)
		{
			mylog.log(log_error, "unkown error,block address is nullptr");
			break;
		}
		_udp_client_process(udpsock, data.from, blk);
	}
}

socket_type rpc_server::_rpc_accept(
	socket_type server_sock, sockaddr_in& addr, int& addr_length)
{
	socket_type client=
		accept(server_sock, reinterpret_cast<sockaddr*>(&addr), &addr_length);
	//TODO:可以加入到别的地方去
	if (WSAGetLastError() != 0)
	{
		mylog.log(log_error,
			"rpc accept error:", WSAGetLastError());
	}
	return client;
}

void rpc_server::_tcp_client_process(socket_type client,const udpaddr& addr)
{
	//TODO:实现它
}

void rpc_server::_client_process(rpcudp& sock,const udpaddr& addr,
	const_memory_block blk, bool send_by_udp)
{
	static std::hash_set<size_t> keep;
	remote_produce_middleware middleware(blk);
	middleware.for_each(
		[&](remote_produce_group& group,
			argument_container blk)->size_t
	{
		rpc_head head;
		advance_in(blk, rarchive(blk.buffer, blk.size, head));
		if (keep.find(head.id.exparment()) != keep.end())
		{
			mylog.log(log_debug, "Same rpc id", head.id.exparment());
		}
		else
		{
			keep.insert(head.id.exparment());
		}
		rpc_request_msg msg;
		auto iter = rpc_local().find(head.id.funcid());

		if (iter == rpc_local().end())
		{
			mylog.log(log_error, "Not found rpc number");
			msg = rpc_error;
		}
		else
		{
			msg = rpc_success;
			auto rpc_server_stub = iter->second;
			rpc_server_stub(blk, head, msg, group);
			if (group.is_standby())
				_send(sock, group.to_memory(),
				addr, send_by_udp);
		}
		return 0;
	});
}


void rpc_server::_udp_client_process(rpcudp& udpsock,
	const udpaddr& addr, const_memory_block blk)
{//MSG:I know ,argument list is too long.
#ifdef _LOG_RPC_RUNNING
	mylog.log(log_debug, "client process");
#endif
	_client_process(udpsock, addr, blk, true);
}

void rpc_server::_send(
	rpcudp& sock, const_memory_block argbuf, 
	const sockaddr_in& addr,bool send_by_udp)
{
	if (send_by_udp)
	{
		//sock.send_block(addr, argbuf, 0);
	}
	else
	{
		//TODO:实现,动态connect and send
	}
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