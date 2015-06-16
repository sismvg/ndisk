#ifndef ISU_RPCUDP_HPP
#define ISU_RPCUDP_HPP

//��UDP����
//��ʱ�ش���������
//����UDP�������״̬�·�����
//��rpc_client_tk��rpc_serverר��

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
		��������
		flag���ܶԵײ�udp socket����Ӱ��
		addr:���ڽ��շ����ߵ�ַ
		addrlen:���ڽ��շ����ߵ�ַ�ĳ���
		timeout:recvfrom�ĳ�ʱʱ��

		return:������κδ����ʱ,��shared_memoryΪ��
		����ָ��һ���ڴ�
	*/
	shared_memory recvfrom(int flag, udpaddr& addr,
		int& addrlen, millisecond timeout);

	shared_memory recvfrom(int flag, udpaddr&, int& addrlen);

	/*
		����һ������
		��ʹ�ײ㻺��������Ҳ�ᷢ��,���������ݷ�Ƭ.
		buf:���ݿ�ͷ
		bufsize:Ҫ���͵��ֽ���,����Ϊ0
		flag:���Եײ�udp socket��Ч
		addr:Ŀ��ĵ�ַ

		return:��ΪSOCKET_ERROR��Ϊ�ɹ�
		sendto�����û�ָ��,����������Ϊ�Է�δ�յ����ݶ�����
	*/
	int sendto(const void* buf, size_t bufsize,
		int flag, const udpaddr& addr);

	/*
		������һ��,����blk��buf��bufsize�ļ�����
	*/
	int sendto(const_memory_block blk, int flag, const udpaddr& addr);

	/*
		һ��,�����ṩ��shared_memory����
		����Լ�������copy���ڲ�������,�����뱣֤memory���ᱻ�����ط��޸�
	*/
	int sendto(const_shared_memory memory, int flag, const udpaddr& addr);

#ifdef _USER_DEBUG
	//debug��
	void wait_all_ack();
	void set_rate_of_packet_loss(double);//���ö�����
#endif

	/*
		�ײ�socket
	*/
	raw_socket socket() const;
	/*
		����closesocket�����������������
	*/
	socket_type deatch();
	/*
		������������
	*/
	void close();
	/*
		����Ҫ�󶨵�socket,������ԭ����socket�����в���
		������δ�յ������ݰ�,����recvfrom�е��̶߳��ᱻ���
	*/
	void setsocket(socket_type sock);

private:
	typedef udp_spinlock spinlock_t;
	typedef rpctimer::timer_handle timer_handle;
	typedef std::pair<size_t, shared_memory> serlized_modules;
	
	//��ʽ��ʶ,��������,�������л���Ŀ�ĵ�
	//ͷ�����������ٸ��ֽ�
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
		��֤��Щģ��ȫ�����ǿ������
	*/
	recver_modules_manager _recver_modules;//�⿪�����Ƕ������
	sender_modules_manager _sender_modules;//���ڹ��췢�����ݰ���ģ��

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
	//��������Ҫ��

	enum recver_message{
		complete_package=1,
		recver_exit
	};

	enum processor_message{
		get_slice=1,
		send_slice=2,
		processor_exit
	};

	//����_clients��ͬ��
	spinlock_t _spinlock;

	rpcudp_client& _get_client(const udpaddr&);
	std::map<udpaddr, rpcudp_client*> _clients;

	size_t _alloc_group_id();
	std::atomic_size_t _group_id_allocer;

	//������ʱ��
	void _retrans(timer_handle, sysptr_t client_ptr);
	void _clean_ack(timer_handle, sysptr_t client_ptr);
	void _clean_ack_impl(timer_handle, sysptr_t, size_t max);
#ifdef _WINDOWS
	//����recvfrom�е��̵߳�����,���������Ƿ���exit_msg
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
		����ģ��ģ��
	*/

	//�û�ͷ��߿���ռ�ðٷ�֮���ķ�Ƭ��С
	static const double _max_user_head_rate;

	//ACK����Զ������Ҫ����,����������rtt��window����

	//���ڼ���rtt,Ҳ�����ӶԷ���rtt����
	SEND_MODULE_MAKE(_rtt_calc);

	//���ʹ��ڵĵ���
	SEND_MODULE_MAKE(_window_adjustment);

	//�Ӵ�ACK
	SEND_MODULE_MAKE(_ask_ack);

	//��ȡ�����Ӵ��ķ�Ƭ,��Щ��Ƭ�������û�ͷ
	//������Щ��Ƭ������max_user_head_rate������,��Ϊ����Ҳ��������
	SEND_MODULE_MAKE(_get_incidentally_slice);

	//����ʵ�ֵ��鷳,����û������Ҫ��Ҳ��Ū��һ��module
	void _register_send_modules();

	//------------------------------------------------------

	//����ģ��

	//���ܶԷ���rtt����
	RECV_MODULE_MAKE(_unpack_rtt_calc);

	//���ܴ��ڵ���
	RECV_MODULE_MAKE(_unpack_window_adjustment);

	//�⿪ack
	RECV_MODULE_MAKE(_unpack_ack);

	//�⿪�Ӵ���Ƭ
	RECV_MODULE_MAKE(_unpack_incidentally_slice);

	//����
	RECV_MODULE_MAKE(_unpack_main_pack);

	void _register_recv_modules();

	//-----------------------------------------------------

	//��̬��������
	size_t _get_window_size();
	size_t _get_window_size(const udpaddr&);

	//���������������ٶ�,��λΪbit
	static size_t _local_speed_for_wlan();
	//��������ľ������ļ����ٶ�
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
	//ͳ����������
	void _debug_count_performance();
	//һ�������˶���bit������
	std::atomic_uint_fast64_t _total_bits;
	//һ���ش�le���ٴ�
	std::atomic_size_t _retry_count;
	//�����˶����ֽ�
	std::atomic_uint_fast64_t _recved_total_bits;
	//�ڴ�ռ��,������release���ֽ�
	std::atomic_uint_fast64_t _memory_count;

	private:
		//֧�ֶ�������
		std::uniform_int<> _debug_random_loss;
		std::random_device _debug_dev;
		std::mt19937 _debug_mt;
#endif
};
#endif