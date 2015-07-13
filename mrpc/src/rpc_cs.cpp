
#include <assert.h>

#include <boost/thread/tss.hpp>

#include <rpc_cs.hpp>
#include <mrpc_utility.hpp>
#include <rpc_group_middleware.hpp>

boost::thread_specific_ptr<remote_produce_parasite> ifx_parasite;

rpc_client_tk::rpc_client_tk(rpc_client& mgr,const udpaddr& local,
	transfer_mode mode, remote_produce_group* default_group /* = nullptr */)
	:
		_manager(&mgr),
		_recver(nullptr), 
		_address(local)
{

#ifdef _LOG_RPC_RUNNING
	mylog.log(log_debug, "rpc client tk init");
#endif

	_init(mgr, local, mode, default_group);
	_init_socket();
}

rpc_client_tk::~rpc_client_tk()
{
#ifdef _LOG_RPC_RUNNING
	mylog.log(log_debug, "rpcu client tk destroy");
#endif
	stop();
	delete _default_group;
}

void rpc_client_tk::run(active_event* event /* = nullptr */)
{
	assert(_recver == nullptr);
	_recver = new std::thread([&](active_event* event)
	{
		_recver_thread(event);
	}, event);
}

void rpc_client_tk::stop()
{
	closesocket(_tcpsock);
	_tcpsock = SOCKET_ERROR;

	_udpsock.close();
	
	//tcpsock和udpsock被关闭以后 _recver一定是joinable的
		
	if (_recver)
	{
		if (!_recver->joinable())
		{
			mylog.log(log_error, "rpc client tk "\
				"can't exit because recver not joinable");
			return;
		}
		_recver->join();
		delete _recver;
		_recver = nullptr;
	}
}

const udpaddr& rpc_client_tk::address() const
{
	return _address;
}

remote_produce_group* 
	rpc_client_tk::set_default_group(remote_produce_group* group)
{
	auto* ret = _default_group;
	_default_group = group;
	return ret;
}

//impl
void rpc_client_tk::_init(rpc_client& mgr, const udpaddr& local,
	transfer_mode mode, remote_produce_group* group)
{
	_address = local;
	_mode = mode;
	if (group == nullptr)
	{
		_default_group = new remote_produce_group;
	}
	else
	{
		_default_group = group;
	}
}

void rpc_client_tk::_start_rpc(bool async, remote_produce_group& group,
	const udpaddr& server, remote_procall* rpc)
{//WARNING:async
#ifdef _LOG_RPC_RUNNING
	mylog.log(log_debug, "start rpc is_async", async);
#endif
	if (async)
	{
		rpc->start(server);
		_send(group, server);
	}
	else
	{
		_send(group, server);
		rpc->start(server);
		delete rpc;
	}
}

rpc_head rpc_client_tk::_make_rpc_head(bool async, func_ident ident)
{
	static size_t allocer = 0;
	rpc_head ret;
	ret.id = (make_ulong64(ident, 0) | (async ? 0 : ID_ISSYNC) | ++allocer);
	return ret;
}

rpc_client_tk::tuple 
	rpc_client_tk::_ready_rpc(CALL_ARG,remote_procall* rpc)
{
	remote_produce_group& grp = _get_group(group);

	auto& parasite = _manager->get_parasite();
	if (async)
	{
		parasite.regist_remote_mission(rpc);
	}
	else
	{
		parasite.regist_remote_mission_without_lock(rpc);
	}
	rpc_trunk trunk;
	trunk.ptr = &parasite;
	trunk.rpc = rpc;

	auto head = _make_rpc_head(async, ident);
	head.trunk = trunk;
	rpc->ready();
	return tuple(head);
}

void rpc_client_tk::_call_impl(CALL_ARG,
	remote_procall* rpc, std::function<void(const tuple&)> crack)
{//crack用于执行type相关的代码-用来减少字节生成的
	auto& grp = _get_group(group);
	auto tuple = _ready_rpc(CALL_ARG_USE, rpc);
	crack(tuple);
	_start_rpc(async, grp, *server, rpc);
}

remote_produce_group& 
	rpc_client_tk::_get_group(remote_produce_group* group)
{
	return *(group ? group : _default_group);
}

bool rpc_client_tk::_send(remote_produce_group& group, 
	const udpaddr& server, bool force_udp /* = false */)
{
	bool ret = false;
	if (group.is_standby())
	{
		//mylog.log(log_debug, "is standy");
		if (force_udp || _mode == transfer_by_udp)
		{
#ifdef _LOG_RPC_RUNNING
			mylog.log(log_debug, "start send");
#endif
			_send_by_udp(server, group);
			ret = true;
		}
		else
		{
			_send_by_tcp(server, group);
		}
		group.reset();
	}
	return ret;
}

int rpc_client_tk::_send_by_tcp(const udpaddr& server, 
	remote_produce_group& group)
{//WARNING:only support small misssion
	auto blk = group.to_memory();
	int ret = send(_tcpsock, 
		reinterpret_cast<const char*>(blk.buffer), blk.size, 0);
	delete blk.buffer;
	if (ret == SOCKET_ERROR)
	{
		mylog.log(log_error, "send by tcp have socket_error:",
			get_last_socket_error());
	}
	return ret;
}

int rpc_client_tk::_send_by_udp(const udpaddr& server, 
	remote_produce_group& group)
{
	int sock = _udpsock.native();
	auto blk = group.to_memory();
	io_result error;// = _udpsock.send_block(core_sockaddr(server), blk, 0);
	int sock2 = _udpsock.native();
	if (!error.in_vaild())
	{
		mylog.log(log_error, "send by udp have socket_error,",
			error.system_socket_error());
	}
	return error.system_socket_error();
}

void rpc_client_tk::_process(const udpaddr& server,
	const_memory_block memory)
{
#ifdef _LOG_RPC_RUNNING
	mylog.log(log_debug, "rpc process mission");
#endif
	remote_produce_middleware ware(memory);
	ware.for_each([&](remote_produce_group& group,
			const argument_container& raw_args)->size_t
	{
		auto args = raw_args;
		rpc_head head;
		rpc_request_msg msg;
		advance_in(args,
			rarchive(args.buffer, args.size, head, msg));
		if (msg == rpc_success)
		{
			auto rpc = head.trunk.rpc;
			auto& parsite =
				*reinterpret_cast<remote_produce_parasite*>(head.trunk.ptr);
			int val = rpc->is_set(0);
			if (_is_async(head))
			{
				parsite.unregist_remote_mission(rpc,head.id.exparment());
			}
			else
			{
				parsite.unregist_remote_mission_without_lock
					(rpc, head.id.exparment());
			}
			(*rpc)(server, args);
			if (!head.id.is_sync_call())
			{
				delete rpc;
				if (ware.get_event())
					ware.get_event()->active();
			}
		}
		else
		{
			mylog.log(log_error, "Have rpc error");
			throw std::runtime_error("");
		}
		return 0;
	});
}

void rpc_client_tk::_init_socket()
{
	raw_socket sock = 0;
	if (_mode&transfer_by_tcp)
	{
		sock = _tcpsock = socket(AF_INET, SOCK_STREAM, 0);
		bind(sock, reinterpret_cast<
			const sockaddr*>(&_address), sizeof(_address));
	}
	if (_mode&transfer_by_udp)
	{
		_udpsock.setsocket(_address);
	}
}

void rpc_client_tk::_recver_thread(active_event* event)
{
	if (event)
	{
		event->active();
	}
	if (_mode==transfer_by_udp)
	{
		while (true)
		{
			udt_recv_result data = _udpsock.recv();

			const io_result& error = data.io_state;

			if (!error.in_vaild()&&error.system_socket_error()!=ENOTSOCK)
			{
				mylog.log(log_debug,
					"recver_thread exit,code:", error.system_socket_error());
				break;
			}

			const_memory_block blk = data.memory;
			if (blk.buffer == nullptr)
			{
				mylog.log(log_error, "block buffer is nullptr");
				break;
			}
			_process(data.from, blk);
		}
	}
}

bool rpc_client_tk::_is_async(const rpc_head& head)
{
	return !head.id.is_sync_call();
}

//rpc_client

rpc_client::rpc_client(transfer_mode mode,
	const sockaddr_in& server, size_t client_limit)
{
	_init(mode, server, client_limit);
}

rpc_client::~rpc_client()
{
	std::for_each(_src.begin(), _src.end(),
		[&](rpc_client_tk* word)
	{
		word->stop();
	});
	close_network_api();
}

void rpc_client::add_client(rpc_client_tk* client)
{
	//TODO:线程安全问题
	if (_src.size() >= _limit_of_client)
		throw std::runtime_error("");
	_src.push_back(client);
}

const size_t rpc_client::client_count() const
{
	return _src.size();
}

const udpaddr& rpc_client::default_server() const
{
	return _server;
}

void rpc_client::set_server(const udpaddr& addr)
{
	_server = addr;
}

remote_produce_parasite& rpc_client::get_parasite()
{
	if (ifx_parasite.get() == nullptr)
	{
		ifx_parasite.reset(new remote_produce_parasite(1));
	}
	return *ifx_parasite;
}


active_event& rpc_client::get_event()
{
	return get_parasite();
}

//impl
void rpc_client::_init(transfer_mode mode, const sockaddr_in& addr, size_t limit)
{
	startup_network_api();
	_mode = mode;
	_local = addr;
	_limit_of_client = limit == 0 ? cpu_core_count() : limit;
}

void rpc_client::_async(rpc_client_tk& client, active_event& event)
{
	auto* ptr = &event;
	std::thread thr([=, &client]()
	{
		client.run(ptr);
	});
	thr.detach();
}

const udpaddr& rpc_client::_address(const sockaddr_in* addr)
{
	return _local;
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

	active_event event(1);// _limit_of_client);
	event.ready_wait();
	for (auto index = 0; index != 1;++index)// _limit_of_client; ++index)
	{
		addr.sin_port = htons(RPC_SERVER_PORT + index + 1);
		_src.push_back(client_maker(addr, this));
		_async(*_src.back(), event);
	}
	event.wait();
}

//
remote_produce_parasite::
	remote_produce_parasite(size_t wait_until /* = 1 */)
:active_event(wait_until)
{}

remote_produce_parasite::~remote_produce_parasite()
{}

remote_produce_parasite::why_break
	remote_produce_parasite::block()
{
	wait();


	return _why;
}

active_event& remote_produce_parasite::blocker()
{
	return *this;
}

void remote_produce_parasite::set_unblock(why_break why)
{
	_why = why;
	active();
}

void remote_produce_parasite::
	regist_remote_mission(remote_procall* rpc)
{
	_spinlock.lock();
	regist_remote_mission_without_lock(rpc);
	_spinlock.unlock();
}

void remote_produce_parasite::
	regist_remote_mission_without_lock(remote_procall* rpc)
{
	auto pair = _missions.insert(rpc);
	if (!pair.second)
	{
		mylog.log(log_error, "The rpc was inserted");
	}
}

void remote_produce_parasite::
	unregist_remote_mission(remote_procall* rpc,size_t number)
{
	_spinlock.lock();
	unregist_remote_mission_without_lock(rpc, number);
	_spinlock.unlock();
}

void remote_produce_parasite::
unregist_remote_mission_without_lock(remote_procall* rpc, size_t number)
{
	static std::map<remote_procall*, std::vector<size_t>> keep;
	auto iter = _missions.find(rpc);
	if (iter == _missions.end())
	{
		mylog.log(log_debug, "Unknow rpc com", rpc);
	}
	else
	{
		auto& vec = keep[rpc];
		vec.push_back(number);
	}
	_missions.erase(iter);
}
