
#include <trans_core.hpp>

#include <thread>
#include <atomic>

#include <rpcudp_def.hpp>
#include <socket.hpp>

#include <multiplex_timer.hpp>
#include <io_complete_port.hpp>

#include <archive.hpp>
#include <mpl_helper.hpp>

#include <boost/smart_ptr/detail/spinlock.hpp>
#include <dynamic_bitmap.hpp>

#ifdef _USER_DEBUG
#include <rpcdef.hpp>
#endif

unsigned long long system_freq()
{
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	return freq.QuadPart;
}

template<class Func>
unsigned long long log_time(Func fn)
{
	LARGE_INTEGER start;
	BOOL ret = QueryPerformanceCounter(&start);

	fn();

	LARGE_INTEGER end;
	QueryPerformanceCounter(&end);
	//���պ������
	return (end.QuadPart - start.QuadPart) * 1000;
}

std::atomic_size_t retry = 0;

size_t trans_core::maxinum_slice_size()
{
	return maxinum_slice_data_size;
}

size_t trans_core::trans_core_reserve()
{
	return trans_core_reserve_size;
}

/*
	cancel io��֧��
	�ಥ��֧��
	�ಥxid�������޸�
*/

//��ôдֻ�������Զ��������
#define _START_TRY_BLOCK try{
#define _END_TRY_BLOCK(expr) }catch(expr)

#define SHARED_MEMORY_PAIR(mem) mem.get(),mem.size()

#define SHARED_MEMORY_DEALLOC [size](const char* ptr)\
	{delete ptr;}

class trans_core_impl
	:public boost::noncopyable
{//һ�Զ����Ӵ�������.����������,���ؽṹ�л��а�ͷ��
public:
	friend class trans_core;

	typedef core_sockaddr ipaddr;

#ifdef _WINDOWS
	typedef HANDLE kernel_handle;
#else
	typedef FD kernel_handle;
#endif

	typedef shared_memory_block<char> shared_memory;
	typedef shared_memory_block<const char> const_shared_memory;

	typedef multiplex_timer timer_queue;
	typedef timer_queue::timer_handle timer_handle;

	typedef char bit_type;
	//
	typedef std::size_t size_t;
	typedef size_t size_type;
	typedef std::size_t ulong32;
	typedef unsigned short ushort16;
	typedef unsigned long long ulong64;

	typedef void* io_mission_handle;

	typedef std::function < void(const core_sockaddr& dest,
		const const_shared_memory& memory,
		io_mission_handle id) > bad_send_functional;

	trans_core_impl(const ipaddr& local, bad_send_functional bad_send,
		size_t recv_thread_count=1,size_t window = 0)
		:
			_sr_blance(default_window_size), 
			_group_lock(BOOST_DETAIL_SPINLOCK_INIT),
			_timeout_timer(MEMBER_BIND_IN(_timer_callback),_timer_argument_release),
			_group_allocer(0),
			_address(local),
			_socket(SOCK_DGRAM,0,local),
			_recver_port(recv_thread_count),
			_work_flag(WORK_DEFAULT),
			_bad_send(bad_send)
#ifdef _USER_DEBUG
			,
			_debug_retry_count(0),
			_debug_window_time(0),
			_debug_sendto_time(0),
			_debug_recvfrom_time(0)
#endif
	{
		if (_socket.socket() == SOCKET_ERROR)
		{
			mylog.log(log_error,
				"create socket error:", WSAGetLastError());
			return;
		}

		_recver = std::thread([&]()
		{
			_recver_thread();
		});

	}

	~trans_core_impl()
	{//����֤���I/O����
		if (_socket.socket() != SOCKET_ERROR)
			close();
	}

	void set_work_flag(size_t flag)
	{
		_work_flag |= flag;
	}

	void close()
	{
#ifdef _USER_DEBUG
		mylog.log(log_debug, "--window-time-:", _debug_window_time / system_freq());
		mylog.log(log_debug, "--sendto-time-:", _debug_sendto_time / system_freq());
		mylog.log(log_debug, "--recvfrom-time-:", _debug_recvfrom_time / system_freq());
#endif
		if (_socket.socket() == SOCKET_ERROR)
			return;
		_socket.close();
		_recver.join();

		//��������������
		_recver_port.close();

		//���ֹͣ��Ϊ�˷�ֹrecver_thread��cancel_timer��
		_timeout_timer.stop();
	}

	struct __trans_trunk
	{
		timer_handle local_handle;
	};

	struct _trans_group
	{
		ulong32 magic;
		package_id_type id;
		ushort16 rtt, window;
		__trans_trunk trunk;
	};

	//Ϊͷ��Ԥ�������ݳ���
	static const size_t trans_core_impl_reserve = sizeof(_trans_group);

	struct _retrans_trunk
	{
		_retrans_trunk(const _trans_group& group,
			const shared_memory& buf, const udpaddr& addr)
			:id(group.id), buffer(buf), address(addr), retry_limit(0)
		{
		}

		package_id_type id;

		std::atomic_size_t retry_count;
		
		shared_memory buffer;
		udpaddr address;
		size_type retry_limit;
	};

	/*
		ȡ��һ��io����.��handle��Ч���Ѿ���cancelʱ����false
		active_io_failedָ�����Ƿ����bad_send
	*/
	bool cancel_io(const io_mission_handle& handle, bool active_io_failed = true)
	{
		try
		{
			const timer_handle timer = reinterpret_cast<timer_handle>(handle);
			sysptr_t arg = _timeout_timer.get_argument(timer);
			_exit_timer(*reinterpret_cast<_retrans_trunk*>(arg),
				timer, active_io_failed);
		}
		catch (...)
		{
			return false;
		}
		return true;
	}

	/*
		���÷�������Ҫ����Ϣ
		����һ������mission_handle,��������cancelһ��I/O
	*/
	io_result assembile_send(const shared_memory& memory,
		const udpaddr& addr,const retrans_control* ctrl_ptr=nullptr)
	{//trunkֻ����trunk+���ֱ������
		//�����ܱ�֤���������ⶫ����ƽ̨��ص�

		retrans_control ctrl;

		if (ctrl_ptr)
			ctrl = *ctrl_ptr;

		memory_block block(SHARED_MEMORY_PAIR(memory));

		package_id_type id = _alloc_package_id();

		auto lock = ISU_AUTO_LOCK(_group_lock);
		_packages.set(id, true);
		lock.unlock();

		__trans_trunk trunk;
		_trans_group group = _make_group(id, trunk);

		_retrans_trunk* trunk2 = new _retrans_trunk(group, memory, addr);
		trunk2->retry_limit = ctrl.retry_limit;

		timer_handle handle =
			_timeout_timer.ready_timer(trunk2, ctrl.first_timeout);
		group.trunk.local_handle = handle;

		archive_to(block.buffer, block.size, group);

		return io_result(trunk.local_handle, 0, 0);
	}

	io_result send_assembiled(const shared_memory& memory,const ipaddr& addr)
	{
		_enter_trans(addr);

		auto* head = reinterpret_cast<_trans_group*>(memory.get());

		_timeout_timer.start_timer(head->trunk.local_handle);

		_sendto_impl(native(), memory, 0, addr);
		return io_result(head->trunk.local_handle, 0, WSAGetLastError());
	}

	//���ͷ�����Ԥ��trans_core_reserve���ֽڵ�ͷ��
	io_result send(const shared_memory& memory,
		const udpaddr& addr,const retrans_control* ptr)
	{
		assembile_send(memory, addr, ptr);
		return send_assembiled(memory, addr);
	}

	void _enter_trans(const ipaddr& addr)
	{
#ifdef _USER_DEBUG
		_debug_window_time+=log_time([&]()
		{
#endif
			while (_sr_blance == 0)
			{
				boost::detail::yield(1);
			}
			--_sr_blance;
#ifdef _USER_DEBUG
		});
#endif
	}

	recv_result recv()
	{
		return get_completet_io();
	}

	core_socket::system_socket native() const
	{
		return _socket.socket();
	}

	recv_result get_completet_io()
	{
		//��ȡһ�����I/O���������Ƿ�ɹ�
		auto arg = _recver_port.get();
		if (arg.key != 0 || arg.user_key != 0)
		{
			return recv_result(io_result(nullptr, arg.key, arg.user_key),
				core_sockaddr(), EXIT_COMPLETE, memory_block(nullptr, 0), 0);
		}

		auto* ptr = reinterpret_cast<recv_result*>(arg.argument);
		recv_result result = *ptr;
		delete ptr;

		return result;
	}

private:
	static const size_t default_window_size =773;

	//tcp����200,udp���ڴ������ý��ͱ��ؾ�����
	//����Ҫ����һ��
	static const size_t default_timeout = 64;

	//һ��id�߽�,���ڼ���Ƿ��ж���İ�����
	static const package_id_type id_boundary = 4096;

	//1472��ȥ��udpͷ���MTU
	static const size_t maxinum_package_size = 1472 - trans_core_impl_reserve;

	typedef boost::detail::spinlock spinlock_t;

	spinlock_t _group_lock;

	std::atomic<package_id_type> _group_allocer;
	std::atomic_size_t _sr_blance;
	std::atomic_size_t _work_flag;

	core_sockaddr _address;
	core_socket _socket;
	
	dynamic_bitmap _packages;
	dynamic_bitmap _received;
	io_complete_port _recver_port;

	bad_send_functional _bad_send;

	std::thread _recver;
	multiplex_timer _timeout_timer;
	DEFAULT_ALLOCATOR _memory_allocer;

#ifdef _USER_DEBUG
	std::atomic_size_t _debug_retry_count;
	ulong64 _debug_recvfrom_time;
	ulong64 _debug_sendto_time;
	ulong64 _debug_window_time;
#endif

	package_id_type _alloc_package_id()
	{
		return _group_allocer++;
	}

	void _stun_group(_retrans_trunk* trunk,io_mission_handle handle)
	{
		if (_bad_send)
			_bad_send(trunk->address,
				trunk->buffer, handle);
	}

	void _exit_timer(const _retrans_trunk& trunk,
		timer_handle handle, bool active_io_failed = true)
	{
		_packages.set(trunk.id, false);
		//���ܷŵ�lock��,�����п���recv_thread ����set false�Ժ�,�⿪group_lcok
		//Ȼ��ɾ����ʱ��,�����ɾ����ʱ��Ҳ��lock��ͻᷢ��ɾ�����ε�����
		this->_timeout_timer.cancel_timer_when_callback_exit(handle);

		//post faild�͵���bad_sendֻ��ѡһ��
		if (_work_flag&POST_SEND_COMPLETE)
		{
			io_result state(handle, send_faild, WSAGetLastError());
			_post(state, trunk.address, FAILED_SEND_COMPLETE, trunk.buffer);
		}
		else if (active_io_failed&&_bad_send)
		{
			_bad_send(trunk.address, trunk.buffer, handle);
		}
	}

	void _post(size_type key, size_type error_code)
	{
		_recver_port.post(complete_result(key, error_code, nullptr));
	}

	template<class T>
	void _post(const io_result& state,
		const core_sockaddr& from, size_type key, const T& memory,size_t size=0)
	{
		auto* ptr = new complete_io_result(state, from, key, memory, size);
		_recver_port.post(complete_result(0, 0, ptr));
	}

	void _timer_callback(timer_handle handle,sysptr_t ptr)
	{
		_retrans_trunk* trunk = reinterpret_cast<_retrans_trunk*>(ptr);
#ifdef _USER_DEBUG_LOG
		mylog.log(log_debug, "retry");
#endif
		if (++trunk->retry_count > trunk->retry_limit)
		{
			this->_stun_group(trunk, handle);

			//��Ȼ��̫����,Ҫ�ǰ���պ���cancel�����˾��鷳��
			auto lock = ISU_AUTO_LOCK(_group_lock);

			_exit_timer(*trunk, handle);
			lock.unlock();
			return;
		}
		int ret = _sendto_impl(_socket.socket(), trunk->buffer,
			0, trunk->address);
		if (ret == SOCKET_ERROR)
		{
			_exit_timer(*trunk, handle);
		}
		size_t ms = _timeout_timer.get_expire_interval(handle);
		_timeout_timer.advance_expire(handle, ms);
	}

	static void _timer_argument_release(sysptr_t ptr)
	{
		_retrans_trunk* trunk =
			reinterpret_cast<_retrans_trunk*>(ptr);
		delete trunk;
	}

	_trans_group _make_group(package_id_type id, const __trans_trunk& trunk)
	{
		_trans_group result;  
		result.magic = SLICE_WITH_DATA | SLICE_WITH_WINDOW;
		result.id = id;
		result.trunk = trunk;
		result.window = 0;
		return result;
	}

	size_t _get_buffer_size() const
	{
		//���mtu-udpͷ��
		return 1472;
	}

	void _try_release_buffer()
	{}

	void _recver_thread()
	{
		core_sockaddr from;
		int from_length = 0;

		size_t size = _get_buffer_size();
		int error_code = 0;

		_START_TRY_BLOCK
		shared_memory buffer(new bit_type[size], size);

		package_id_type biggest_id = id_boundary;
		while (true)
		{
			int ret = _recvfrom_impl(native(), buffer.get(),
				size, 0, from);
		
			if (ret == SOCKET_ERROR)
			{
				error_code = WSAGetLastError();
#ifdef _USER_DEBUG_LOG
				mylog.log(log_debug, "got last error:", error_code);
#endif
#ifdef _WINDOWS
				if (error_code == ENOTSOCK||error_code==WSAEINTR)
				{
					break;
				}
				else if (error_code == ENOMEM)
				{
					//�ͷŶ���Ļ�����
					_try_release_buffer();
				}
				else if (error_code==ENOBUFS)
				{
					break;
				}
#ifndef _WINDOWS
				else if (error_code != WSAECONNRESET)
				{
					break;
				}
#endif
				//��ΪԶ�̼����ǿ�ȹر�һ�����ӵ�ʱ��
				//��鿴�Ƿ�Ϊ����ip,Ȼ��ָ���������recv
				error_code = 0;
				continue;
#else
#endif
			}
			const_memory_block block(buffer);

			const auto& head = *reinterpret_cast<
				const _trans_group*>(block.buffer);

			auto local_handle = head.trunk.local_handle;

			if (head.magic&SLICE_WITH_ACKS)
			{
				auto lock = ISU_AUTO_LOCK(_group_lock);
				if (_packages.have(head.id) )
				{
					_packages.just_set(head.id, false);
					++_sr_blance;
					lock.unlock();

					sysptr_t arg = _timeout_timer.get_argument(
						head.trunk.local_handle);

					if (_work_flag&POST_SEND_COMPLETE)
					{
						auto ptr = reinterpret_cast<_retrans_trunk*>(arg);
						_post(io_result(local_handle),
							from, SEND_COMPLETE, ptr->buffer, ret);
					}

					_timeout_timer.cancel_timer(head.trunk.local_handle); 
				}
			}
			else
			{		
				if (head.id > biggest_id * 2)
				{//����̫����,�����Ƕ����
					continue;
				}
				biggest_id = (std::max)(biggest_id, head.id);
				_send_ack(from, head);
				if (!_received.test_and_set(head.id))
				{
					_post(io_result(local_handle, 0, error_code),
						from, RECV_COMPLETE, _get_shared_memory(buffer, ret),ret);
				}
			}

		}
		_END_TRY_BLOCK(std::bad_alloc&)
		{
			_post(memory_not_enough, error_code);
			return;
		_END_TRY_BLOCK(...)//�����ĵ�һ���ַ�����},������һ��catch���ܼ�}
		{
			_post(have_cpp_exception, error_code);
			return;
		}
		//����
		WSASetLastError(0);
		SetLastError(0);
		_post(recv_thread_exit, error_code);
	}

	shared_memory _get_shared_memory(shared_memory& memory, size_t data_size)
	{
		size_t size = memory.size();

		if (data_size < size / 2)
		{
			void* buffer = new char[data_size];
			memcpy(buffer, memory.get(), data_size);
			size = data_size;
			return shared_memory(memory_block(buffer, data_size));
		}
		shared_memory result = memory;

		memory = shared_memory(memory_block(new char[size], size));
		return result;
	}

	void _send_ack(const ipaddr& address, _trans_group group)
	{
		group.magic = SLICE_WITH_ACKS;
		_sendto_impl(native(), make_block(group),
			0, address);
	}

	int _sendto_impl(int sock, const_memory_block blk,
		int flag, const ipaddr& addr)
	{
		int ret = 0;
#ifdef _USER_DEBUG
		_debug_sendto_time+=log_time([&]()
		{
#endif
			ret = ::sendto(sock,
				reinterpret_cast<const char*>(blk.buffer), blk.size,
				flag, CORE_SOCKADDR_PAIR(addr));
#ifdef _USER_DEBUG
		});
#endif
		return ret;
	}

	int _recvfrom_impl(int sock, void* buffer,
		size_t size, int flag, ipaddr& addr)
	{
		int ret = 0;
#ifdef _USER_DEBUG
		_debug_recvfrom_time += log_time([&]()
		{
#endif
			int length = addr.address_size();

			ret = ::recvfrom(sock,
				reinterpret_cast<char*>(buffer), size,
				flag, addr.socket_addr(), &length);
#ifdef _USER_DEBUG
		});
#endif
		return ret;
	}
};

size_t trans_core::maxinum_slice_data_size =
	trans_core_impl::maxinum_package_size;

size_t trans_core::trans_core_reserve_size =
	trans_core_impl::trans_core_impl_reserve;

trans_core::trans_core(const udpaddr& addr,size_t limit_thread)
{
	_impl.reset(new trans_core_impl(addr,
		trans_core_impl::bad_send_functional(), limit_thread));
}

trans_core::trans_core(const udpaddr& addr,
	bad_send_functional bad_send,size_t limit_thread)
{
	_impl.reset(new trans_core_impl(addr, bad_send, limit_thread));
}

io_result trans_core::send(const shared_memory& memory,
	const udpaddr& addr,const retrans_control* ptr)
{
	return _impl->send(memory, addr, ptr);
}

io_result trans_core::assemble_send(const shared_memory& memory,
	const udpaddr& dest, const retrans_control* ctrl_ptr)
{
	return _impl->assembile_send(memory, dest, ctrl_ptr);
}

io_result trans_core::send_assembled(const shared_memory& memory, const udpaddr& dest)
{
	return _impl->send_assembiled(memory, dest);
}

void trans_core::set_flag(size_t flag)
{
	_impl->set_work_flag(flag);
}

recv_result trans_core::recv()
{
	return _impl->recv();
}

void trans_core::close()
{
	if (_impl)
		_impl->close();
}

bool trans_core::cancel_io(const io_mission_handle& handle, bool active_io_faild)
{
	return _impl->cancel_io(handle, active_io_faild);
}

core_socket::system_socket trans_core::native() const

{
	return _impl->native();
}

complete_io_result::complete_io_result()
{}

complete_io_result::complete_io_result(const io_result& error,
	const core_sockaddr& from,size_t io_type,
	const shared_memory& memory,size_t size)
	:_io_error(error), 
	_memory(memory), _io_type(io_type),
	_from(from), _size(size)
{
}

complete_io_result::io_mission_handle complete_io_result::id() const
{
	return get_io_result().id();
}

size_t complete_io_result::io_type() const
{
	return _io_type;
}
const io_result& complete_io_result::get_io_result() const
{
	return _io_error;
}

const core_sockaddr& complete_io_result::from() const
{
	return _from;
}

complete_io_result::iterator
	complete_io_result::begin()
{
	return _begin() + trans_core_impl::trans_core_impl_reserve;
}

complete_io_result::const_iterator
	complete_io_result::begin() const
{
	return const_cast<self_type*>(this)->begin();
}

complete_io_result::iterator
	complete_io_result::end()
{
	return _begin() + _size;
}

complete_io_result::const_iterator
	complete_io_result::end() const
{
	return const_cast<self_type*>(this)->end();
}

complete_io_result::iterator
	complete_io_result::_begin()
{
	return _memory.get();
}

complete_io_result::const_iterator
	complete_io_result::_begin() const
{
	return const_cast<self_type*>(this)->_begin();
}