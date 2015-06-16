#ifndef ISU_RPCUDP_HPP
#define ISU_RPCUDP_HPP

//该UDP特性
//超时重传，防丢包
//允许UDP包无序的状态下防丢包
//给rpc_client_tk和rpc_server专用

#include <set>
#include <list>
#include <vector>
#include <hash_map>
#include <hash_set>

#ifdef _USER_DEBUG
#include <random>
#endif

#include <platform_config.hpp>
#include <slice.hpp>
#include <archive.hpp>
#include <ackbitmap.hpp>
#include <double_ulong32.hpp>
#include <multiplex_timer.hpp>
#include <sliding_window.hpp>
#include <magic_modules.hpp>
#include <shared_memory_block.hpp>
#include <io_complete_port.hpp>

#include <rpcdef.hpp>
#include <rpctimer.hpp>
#include <rpclock.hpp>
#include <rpc_utility.hpp>

#include <rpcudp_def.hpp>
#include <rpcudp_client.hpp>
#include <rpcudp_detail.hpp>

struct _retrans_trunk;

class rpcudp
{
public:
	friend struct _retrans_trunk;
	friend class rpcudp_client;
	typedef int raw_socket;
	typedef sockaddr_in udpaddr;
	typedef double_ulong32 ulong64;

	typedef size_t millisecond;
	typedef multiplex_timer rpctimer;
	typedef rpctimer::timer_handle timer_handle;

	typedef shared_memory_block<char> shared_memory;
	typedef shared_memory_block<const char> const_shared_memory;

	typedef RPCUDP_DEFAULT_ALLOCATOR(char) udp_allocator;
	typedef dynamic_memory_block<char, udp_allocator> dynamic_memory_block;

	typedef rpcudp self_type;

	/*
		sock is system socket.make sure no other 
			rpcudp object attach in here.
		mode:rpcudp how to work.see marco in top
	*/
	static const size_t default_work_mode = 
		ENABLE_SENDTO_NAGLE | WORK_IN_SLOW_NETOWRK;

	rpcudp();
	rpcudp(socket_type sock, size_t mode = default_work_mode);

	~rpcudp();

	/*
		接受数据
		flag仅能对底层udp socket产生影响
		addr:用于接收发送者地址
		addrlen:用于接收发送者地址的长度
		timeout:recvfrom的超时时间

		return:如果有任何错误或超时,则shared_memory为空
		否则指向一块内存
	*/
	shared_memory recvfrom(int flag, udpaddr& addr,
		int& addrlen, millisecond timeout);

	shared_memory recvfrom(int flag, udpaddr&, int& addrlen);

	/*
		发送一块数据
		即使底层缓冲区不够也会发送,这回造成数据分片.
		buf:数据开头
		bufsize:要发送的字节量,不能为0
		flag:仅对底层udp socket有效
		addr:目标的地址

		return:不为SOCKET_ERROR即为成功
		sendto除非用户指定,否则永不因为对方未收到数据而阻塞
	*/
	int sendto(const void* buf, size_t bufsize,
		int flag, const udpaddr& addr);

	/*
		和上面一样,出了blk是buf和bufsize的集合体
	*/
	int sendto(const_memory_block blk, int flag, const udpaddr& addr);

	/*
		一样,不过提供了shared_memory参数
		这可以减少数据copy到内部缓冲区,但必须保证memory不会被其他地方修改
	*/
	int sendto(const_shared_memory memory, int flag, const udpaddr& addr);

#ifdef _USER_DEBUG
	//debug用
	void wait_all_ack();
	void set_rate_of_packet_loss(double);//设置丢包率
#endif

	/*
		底层socket
	*/
	raw_socket socket() const;
	/*
		除了closesocket以外的所有析构操作
	*/
	socket_type deatch();
	/*
		就是析构函数
	*/
	void close();
	/*
		设置要绑定的socket,会清理原来的socket的所有参数
		并放弃未收到的数据包,所有recvfrom中的线程都会被打断
	*/
	void setsocket(socket_type sock);

private:
	typedef udp_spinlock spinlock_t;
	typedef rpctimer::timer_handle timer_handle;
	typedef std::pair<size_t, shared_memory> serlized_modules;
	
	//格式标识,所属链接,参数序列化的目的地
	//头部最多允许多少个字节
#define SEND_MODULE_MAKE(name) size_t \
	rpcudp::name(size_t magic,rpcudp_client& client,\
	archive_packages& pac,size_t max_bits)

#define RECV_MODULE_MAKE(name) bool rpcudp::name(size_t magic,\
	rpcudp_client& client,\
	rpcudp_detail::__slice_head_attacher& attacher,\
	packages_analysis& sis)

	typedef magic_modules < 
		size_t,
		bool, 
		size_t,
		rpcudp_client&,//arguments
		rpcudp_detail::__slice_head_attacher&,
		packages_analysis& > recver_modules_manager;

	typedef magic_modules < 
		size_t,//magic_type
		size_t,//result_type
		size_t,//modules contant
		rpcudp_client&,//arguments
		archive_packages&, size_t > sender_modules_manager;

	/*
		保证这些模组全部都是可重入的
	*/
	recver_modules_manager _recver_modules;//解开下面那东西造的
	sender_modules_manager _sender_modules;//用于构造发送数据包的模块

	struct _complete_argument
	{
		_complete_argument(const udpaddr& address,
			const shared_memory& data)
			:addr(address), memory(data), client(nullptr)
		{}

		shared_memory memory;
		udpaddr addr;
		rpcudp_client* client;
	};

	void _init();

	typedef void* kernel_handle;
	typedef int raw_socket;

	raw_socket _udpsock;
	std::atomic_size_t _udpmode;
	rpctimer _timer, _ack_timer;
	//接受所需要的

	enum recver_message{
		complete_package=1,
		recver_exit
	};

	enum processor_message{
		get_slice=1,
		send_slice=2,
		processor_exit
	};

	//负责_clients的同步
	spinlock_t _spinlock;

	rpcudp_client& _get_client(const udpaddr&);
	std::map<udpaddr, rpcudp_client*> _clients;

	size_t _alloc_group_id();
	std::atomic_size_t _group_id_allocer;

	//两个定时器
	void _retrans(timer_handle, sysptr_t client_ptr);
	void _clean_ack(timer_handle, sysptr_t client_ptr);
	void _clean_ack_impl(timer_handle, sysptr_t, size_t max);
#ifdef _WINDOWS
	//正在recvfrom中的线程的数量,用于析构是发送exit_msg
	std::atomic_size_t _recver_count;

	io_complete_port _recvport, _process_port;

	kernel_handle _process_thread, _recv_thread_handle;
#endif

	static SYSTEM_THREAD_RET
		SYSTEM_THREAD_CALLBACK _recv_thread(sysptr_t);

	void _recv_thread_impl(sysptr_t ptr);

	//---------------------------------------------------

	static SYSTEM_THREAD_RET
		SYSTEM_THREAD_CALLBACK _processor_thread(sysptr_t);

	void _processor_thread_impl(sysptr_t);

	void _package_process(const udpaddr&, int len,
		shared_memory, size_t memory_size);

	/*
		发送模块模组
	*/

	//用户头最高可以占用百分之几的分片大小
	static const double _max_user_head_rate;

	//ACK包永远不带主要数据,但是允许有rtt和window调整

	//用于计算rtt,也负责会从对方的rtt请求
	SEND_MODULE_MAKE(_rtt_calc);

	//发送窗口的调整
	SEND_MODULE_MAKE(_window_adjustment);

	//捎带ACK
	SEND_MODULE_MAKE(_ask_ack);

	//获取允许被捎带的分片,这些分片不会有用户头
	//并且这些分片并不受max_user_head_rate的限制,以为本来也是主数据
	SEND_MODULE_MAKE(_get_incidentally_slice);

	//由于实现的麻烦,这里没法把主要包也能弄成一个module
	void _register_send_modules();

	//------------------------------------------------------

	//接受模组

	//接受对方的rtt请求
	RECV_MODULE_MAKE(_unpack_rtt_calc);

	//接受窗口调整
	RECV_MODULE_MAKE(_unpack_window_adjustment);

	//解开ack
	RECV_MODULE_MAKE(_unpack_ack);

	//解开捎带分片
	RECV_MODULE_MAKE(_unpack_incidentally_slice);

	//主包
	RECV_MODULE_MAKE(_unpack_main_pack);

	void _register_recv_modules();

	//-----------------------------------------------------

	//动态辅助参数
	size_t _get_window_size();
	size_t _get_window_size(const udpaddr&);

	//本机对外网极限速度,单位为bit
	static size_t _local_speed_for_wlan();
	//本机参与的局域网的极限速度
	static size_t _local_network_speed();

	//----------------------------------------------------

	//utility
	int _sendto_impl(const_memory_block blk,
		int flag, const udpaddr& addr, int addrlen);

	int _recvfrom_impl(void* buffer,size_t size,
		int flag, udpaddr& addr, int& addrlen);

	static const size_t mtu = 1500;
	static const millisecond default_timeout = 300;
	static const memory_block _nop_block;
	static const const_memory_block _const_nop_block;

	static size_t _get_max_ackpacks();
	static shared_memory _alloc_memory(size_t size);
	static memory_address _alloc_memory_raw(size_t size);
	static size_t _get_mtu();

#ifdef _USER_DEBUG
	public:
	//统计性能数据
	void _debug_count_performance();
	//一共发送了多少bit的数据
	std::atomic_uint_fast64_t _total_bits;
	//一共重传le多少次
	std::atomic_size_t _retry_count;
	//接受了多少字节
	std::atomic_uint_fast64_t _recved_total_bits;
	//内存占用,包括了release的字节
	std::atomic_uint_fast64_t _memory_count;

	private:
		//支持丢包控制
		std::uniform_int<> _debug_random_loss;
		std::random_device _debug_dev;
		std::mt19937 _debug_mt;
#endif
};
#endif