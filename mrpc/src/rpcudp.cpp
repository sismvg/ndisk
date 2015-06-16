
#include <rpcudp.hpp>
#include <random>

#include <boost/lexical_cast.hpp>
#include <boost/thread/tss.hpp>

#include <rpcudp_client.hpp>

#define MYCALLBACK(name) [&](timer_handle id,sysptr_t ptr)\
	{\
		name(id,ptr);\
	}

#ifdef _USER_DEBUG

#define RPCUDP_INIT : \
	_udpmode(0),_group_id_allocer(0),\
_process_thread(nullptr), _recv_thread_handle(nullptr),_recver_count(0),\
_udpsock(SOCKET_ERROR),_timer(MEMBER_BIND_IN(_retrans),\
rpcudp_client::_retrans_trunk_release),_process_port(4),\
_ack_timer(MEMBER_BIND_IN(_clean_ack)),_recved_total_bits(0),\
_total_bits(0),_retry_count(0),\
_debug_mt(_debug_dev()),_debug_random_loss(0,100)//百分之1的丢包率

#else

//recvport和process_port允许4个和1个线程同时处理消息
#define RPCUDP_INIT \
: _udpmode(0),_group_id_allocer(0),\
_process_thread(nullptr), _recv_thread_handle(nullptr),_recver_count(0),\
_udpsock(SOCKET_ERROR),_timer(MEMBER_BIND_IN(_retrans),\
udp_client_sender::_retrans_trunk_release),\
_ack_timer(MEMBER_BIND_IN(_clean_ack))
	

#endif

rpcudp::rpcudp()
RPCUDP_INIT
{
	_init();
}

rpcudp::rpcudp(socket_type sock, size_t mode)
RPCUDP_INIT
{
	_udpmode = mode;
	_udpsock = sock;
	_init();
}

rpcudp::~rpcudp()
{
	close();
}

#ifdef _USER_DEBUG
void rpcudp::wait_all_ack()
{
}

#endif

//接口
void rpcudp::close()
{
#ifdef _LOG_RPCUDP_RUNNING
	mylog.log(log_debug, "rpcudp deconstruct");
#endif

	complete_result exit_msg(
		io_complete_port::complete_port_exit, 0, nullptr);

	_recvport.close();
	_process_port.close();
#ifdef _WINDOWS
	if (_udpsock != SOCKET_ERROR)
		closesocket(_udpsock);

	kernel_handle handles[2] =
	{
		_recv_thread_handle, _process_thread
	};

	WaitForMultipleObjects(2, handles, TRUE, INFINITE);
	CloseHandle(_recv_thread_handle);
	CloseHandle(_process_thread);

	for (auto iter = _clients.begin(); iter != _clients.end(); ++iter)
	{
		delete iter->second;
	}

	_ack_timer.stop();
	_timer.stop();
#else

#endif
}

rpcudp::shared_memory rpcudp::recvfrom(int flag, udpaddr& addr, int& addrlen)
{
	return recvfrom(flag, addr, addrlen, -1);
}

rpcudp::shared_memory rpcudp::recvfrom(int flag, udpaddr& addr,
	int& addrlen, millisecond timeout /* = default_timeout */)
{
	shared_memory ret(memory_block(nullptr, 0));
	++_recver_count;
	complete_result arg = _recvport.get(timeout);
	if (arg.key != 0)
	{
		//TODO:do something
		return ret;
	}
	else if (arg.user_key == complete_package)
	{
		auto* ptr = reinterpret_cast<_complete_argument*>(arg.argument);

		addr = ptr->addr;
		addrlen = sizeof(addr);
		ret = ptr->memory;

		delete ptr;
	}
	else if (arg.user_key == recver_exit)
	{
	}
	return ret;
}

int rpcudp::sendto(const_shared_memory memory, int flag, const udpaddr& addr)
{
	const_memory_block blk = memory;
	return sendto(blk, flag, addr);
}

int rpcudp::sendto(const_memory_block blk, int flag, const udpaddr& addr)
{
	return sendto(blk.buffer, blk.size, flag, addr);
}

int rpcudp::sendto(const void* buf, size_t bufsize,
	int flag, const udpaddr& addr)
{
	_spinlock.lock();
	auto& client = _get_client(addr);
	_spinlock.unlock();

	const_shared_memory memory(
		deep_copy(const_memory_block(buf, bufsize)));
	return client.sendto(memory, addr);
}

rpcudp::raw_socket rpcudp::socket() const
{
	return _udpsock;
}

void rpcudp::setsocket(raw_socket sock)
{
	if (_udpsock != SOCKET_ERROR)
		closesocket(_udpsock);

	_udpsock = sock;

	int val = 0;
	int ret = setsockopt(_udpsock, SOL_SOCKET, SO_RCVBUF,
		reinterpret_cast<const char*>(&val), sizeof(int));

	if (ret == SOCKET_ERROR)
	{
#ifdef _LOG_RPCUDP_RUNNING
		mylog.log(log_error,
			"set socket has error:", WSAGetLastError());
#endif
	}
}


SYSTEM_THREAD_RET SYSTEM_THREAD_CALLBACK 
	rpcudp::_recv_thread(sysptr_t ptr)
{
	reinterpret_cast<self_type*>(ptr)->_recv_thread_impl(ptr);
	SYSTEM_THREAD_RET_CODE(0);
}

void rpcudp::_recv_thread_impl(sysptr_t)
{
	size_t bufsize = _get_mtu();

	udpaddr addr;
	int addrlen = sizeof(addr);
	int code = 0;

	char* tmp = new char[bufsize];
	char* buffer = tmp;
	while (true)
	{
		void* ptr = _alloc_memory_raw(bufsize);

		code = _recvfrom_impl(buffer, bufsize, 0, addr, addrlen);
		if (code == SOCKET_ERROR)
		{//TODO:加上udpsock失效不提示
#ifdef _LOG_RPCUDP_RUNNING
			mylog.log(log_error,
				"rpcudp recvfrom have socket error code:", WSAGetLastError());
#endif
			break;
		}


		shared_memory recvbuf;
		//减少ack包带来的内存损耗
	/*	if (code < static_cast<size_t>(bufsize / 2))
		{
			char* small_buffer = new char[code];
			memcpy(small_buffer, buffer, code);
			recvbuf = shared_memory(memory_block(small_buffer, code));
			_memory_count += code;
		}
		else*/
		if (true)
		{
			recvbuf = shared_memory(memory_block(buffer, code));
			buffer = new char[bufsize];
			_memory_count += bufsize;
		}

		_package_process(addr, addrlen, recvbuf, code);
	}
	delete buffer;
}

void rpcudp::_package_process(const udpaddr& addr,
	int len, shared_memory memory, size_t memory_size)
{
	/*
		要最先发送ack
		*/
	const_memory_block blk(memory);
	blk.size = memory_size;
	packages_analysis sis(blk);
	rpcudp_detail::__slice_head_attacher attacher(sis);


	complete_result job;

	job.user_key = get_slice;
	job.key = 0;

	auto* arg = new _complete_argument(addr, memory);
	job.argument = arg;

	auto wlock = ISU_AUTO_LOCK(_spinlock);
	arg->client = &_get_client(addr);
	wlock.unlock();

	if (attacher.udp_ex != nullptr)
	{
		arg->client->_send_ack(
			addr, attacher.udp_ex->group_id,
			attacher.udp_ex->slice_id);
	}

	_process_port.post(job);
}

SYSTEM_THREAD_RET SYSTEM_THREAD_CALLBACK
rpcudp::_processor_thread(sysptr_t arg)
{
	auto* self = reinterpret_cast<self_type*>(arg);
	self->_processor_thread_impl(arg);
	SYSTEM_THREAD_RET_CODE(0);
}

void rpcudp::_processor_thread_impl(sysptr_t)
{
	complete_result job;
	while (true)
	{
		job = _process_port.get();
#ifdef _LOG_RPCUDP_RUNNING
		mylog.log(log_debug, "processor thread impl running");
#endif
		if (job.key != 0)
		{
			break;
		}
		else if (job.user_key == processor_exit)
		{
			break;
		}
		else if (job.user_key == get_slice)
		{
			auto* ptr = reinterpret_cast<_complete_argument*>(job.argument);

			bool recved_ok=ptr->client->recved_from(ptr->addr, ptr->memory);
			delete ptr;
			if (!recved_ok)
			{
				break;
			}
		}
	}
}

void rpcudp::_retrans(timer_handle handle, sysptr_t ptr)
{
	//不用加锁
	auto* retrans_arg = reinterpret_cast<_retrans_trunk*>(ptr);
	retrans_arg->do_retrans(handle, _udpsock);
}

rpcudp_client& rpcudp::_get_client(const udpaddr& addr)
{
	auto iter = _clients.find(addr);
	if (iter == _clients.end())
	{
		auto pair = _clients.emplace(addr,
			new rpcudp_client(*this,
			addr, _get_window_size(addr), 0, 0));
		if (!pair.second)
		{
#ifdef _LOG_RPCUDP_RUNNING
			mylog.log(log_error, 
				"_clients insert new client failed");
#endif
		}
		iter = pair.first;
	}
	return *iter->second;
}

size_t rpcudp::_get_window_size(const sockaddr_in& addr)
{
	return _get_window_size();
}

size_t rpcudp::_get_window_size()
{
	return 773;//经验
}

//实现-杂项

size_t rpcudp::_get_max_ackpacks()
{
	return _get_mtu() / sizeof(size_t);
}

size_t rpcudp::_local_speed_for_wlan()
{
	return 1024 * 1024;
}

size_t _local_network_speed()
{
	return 1024 * 1024 * 10;
}

size_t rpcudp::_get_mtu()
{
	return 1472;
}

size_t rpcudp::_alloc_group_id()
{
	return ++_group_id_allocer;
}

memory_address rpcudp::_alloc_memory_raw(size_t size)
{
	char* ret = new char[size];
	return ret;
}

int rpcudp::_sendto_impl(const_memory_block blk,
	int flag, const sockaddr_in& addr, int addrlen)
{
	int ret = ::sendto(_udpsock,
		reinterpret_cast<const char*>(blk.buffer), blk.size,
		flag, reinterpret_cast<const sockaddr*>(&addr), addrlen);
	if (ret == SOCKET_ERROR)
	{
#ifdef _LOG_RPCUDP_RUNNING
		mylog.log(log_error,
			"sendto_impl have socket_error code:", WSAGetLastError());
#endif
	}
#ifdef _USER_DEBUG
	_total_bits += blk.size;
#endif
	return ret;
}

int rpcudp::_recvfrom_impl(void* buffer,size_t size, int flag,
	sockaddr_in& addr, int& addrlen)
{
	int ret = 0;
#ifdef _USER_DEBUG
	int debug_reback = 0;
	while (debug_reback == 0)
	{
#endif
		ret = ::recvfrom(_udpsock,
			reinterpret_cast<char*>(buffer), size,
			flag, reinterpret_cast<sockaddr*>(&addr), &addrlen);
		if (ret == SOCKET_ERROR)
		{
#ifdef _LOG_RPCUDP_RUNNING
			mylog.log(log_error,
				"recvfrom_impl have socket_error code:", WSAGetLastError());
#endif
			return ret;
		}

#ifdef _USER_DEBUG
		debug_reback = 1;
		_recved_total_bits += ret;
		if (_debug_random_loss(_debug_mt) == 0)
			debug_reback = 0;
	}
#endif

	return ret;
}

void rpcudp::_init()
{
	_register_send_modules();
	_register_recv_modules();

	if (_udpsock != SOCKET_ERROR)
	{
		int val = 0;
		int ret = setsockopt(_udpsock, SOL_SOCKET, SO_RCVBUF,
			reinterpret_cast<const char*>(&val), sizeof(int));
	}

#ifdef _WINDOWS
	_recv_thread_handle = CreateThread(NULL, 0,
		_recv_thread, reinterpret_cast<sysptr_t>(this), 0, NULL);
	//抢占,变慢了,挂掉
	_process_thread = CreateThread(NULL, 0,
		_processor_thread, reinterpret_cast<sysptr_t>(this), 0, NULL);

	_process_thread = CreateThread(NULL, 0,
		_processor_thread, reinterpret_cast<sysptr_t>(this), 0, NULL);
#else
#endif
}

#ifdef _USER_DEBUG
void rpcudp::_debug_count_performance()
{
	auto lock = ISU_AUTO_LOCK(_spinlock);
	for (auto& value : _clients)
	{
		_retry_count += value.second->_retry_count;
	}
}

#endif
const const_memory_block rpcudp::_const_nop_block(nullptr, 0);

const memory_block rpcudp::_nop_block(nullptr, 0);