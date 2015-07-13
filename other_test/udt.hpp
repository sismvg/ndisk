#ifndef ISU_UDT_HPP
#define ISU_UDT_HPP

#include <slice.hpp>
#include <trans_core.hpp>
#include <multiplex_timer.hpp>
//��ʵ�������ֻ�Ǹ�����Ƭ���鹦�ܵ�wapper

struct udt_recv_result
{//����Ķ��Ѿ�������,Ҳû��Ҫ�ټ�ɶ��
	io_result io_state;
	typedef shared_memory_block<char> shared_memory;
	shared_memory memory;
	core_sockaddr from;
};

class udt
	:boost::noncopyable
{
public:
	typedef shared_memory_block<const char> const_shared_memory;
	typedef shared_memory_block<char> shared_memory;
	typedef char bit_type;
	typedef size_t group_id_type;
	typedef trans_core::io_mission_handle io_mission_handle;
	typedef std::function<void(group_id_type, const core_sockaddr&,
		const const_memory_block&)> bad_send_functional;

	/*
		����һ��udp socket���󶨵�ַ
		limit_threadָ���˿���������recv���߳�����(I/O��ɶ˿�)
		bad_sendָ�����ڰ�����ʧ��ʱ����δ���
	*/
	udt()
		:_groups_lock(BOOST_DETAIL_SPINLOCK_INIT)
	{}
	udt(const udpaddr&, size_t limit_thread = 1);
	udt(const udpaddr&, bad_send_functional bad_send,
		size_t limit_thread = 1);

	/*
		��������,�������������ȫ������udt_recv_result��
	*/
	udt_recv_result recv();

	/*
		��������,���ɷ������û�ָ�����ڴ浱��
		����ֻ�ܽ��ܵ�����Ƭ.��Ϊ�޷�ȷ�������ߡ�
		Ҳ��û��ȷ��buffer�ĳ���,��ص������
	*/
	io_result recv(bit_type* buffer, size_t size, core_sockaddr&);

	/*
		����һ������,send���첽��
		io_result������ں�socket���û�̬socket��״̬
	*/
	io_result send(const core_sockaddr&,
		const const_shared_memory& data,
		size_t size = 0, const retrans_control* ctrl_ptr = nullptr);

	io_result send(const core_sockaddr&, size_t id,
		const const_shared_memory& data, 
		size_t size = 0, const retrans_control* ctrl_ptr = nullptr);

	io_result send_block(const core_sockaddr& dest, size_t id,
		const_memory_block block, 
		size_t size = 0, const retrans_control* ctrl_ptr = nullptr);

	typedef bad_send_functional send_callback;
	void set_send_callback(send_callback);
	/*
		�ر�socket
	*/
	void close();
	
	/*
		ϵͳsocket
	*/
	int native() const;
	/*
		
	*/
	void setsocket(const core_sockaddr&);
	size_t alloc_group_id();
private:
	send_callback _send_callback;
	//��һ���鳤ʱ���ղ�����Ƭ,�������������
	//	multiplex_timer _group_timer;

	//trans_core_head-group_id-start
	struct _group_ident
	{
		group_id_type group_id;
		size_t slice_start;
		size_t data_size;
	};

	struct _recv_group
	{//���ڱ�֤�����ظ���.���Բ���Ҫbitmapɶ�ļ�¼�Ƿ��յ���
		combination _combin;
	};

	typedef boost::detail::spinlock spinlock_t;

	spinlock_t _groups_lock;
	std::map<group_id_type, _recv_group> _recv_groups;

	spinlock_t _sending_groups_lock;
	std::map<group_id_type, std::pair<size_t,size_t>> _sending_groups;

	trans_core _core;
	bad_send_functional _bad_send;

	void _udt_bad_send(const core_sockaddr& dest,
		const const_shared_memory& memory, io_mission_handle id);
	shared_memory _insert(const _group_ident& ident, 
		const core_sockaddr& from,
		const const_memory_block& data);

	std::atomic<group_id_type> _group_id_allocer;
	group_id_type _alloc_group_id();

};
#endif