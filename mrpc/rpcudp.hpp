#ifndef ISU_RPCUDP_HPP
#define ISU_RPCUDP_HPP

//��UDP����
//��ʱ�ش���������
//����UDP�������״̬�·�����
//��rpc_client_tk��rpc_serverר��

#include <rpcdef.hpp>

#include <list>
#include <vector>
#include <memory>
#include <queue>
#include <hash_map>
#include <rpctimer.hpp>
#include <rpclock.hpp>
#include <hash_set>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/thread/mutex.hpp>
//ack��,���ݰ�,���ݰ�����ack,Ҫ�󱾻�����ĳ������ack
#define IS_ACK_PACK 0x01
#define IS_MAIN_PACK 0x10
#define IS_MAIN_PACK_WITH_ACK 0x100
#define IS_ACK_QUERY 0x1000

#define UDP_ACK_SINGAL 0x10000
#define UDP_ACK_MULTI 0x100000
#define UDP_ACK_COMMPRESSED 0x01000000
//flag
#define WHEN_RECV_ACK_BREAK 0x10000000

struct slice_vector
{
public:
	size_t group_id;
	size_t accepted;
	const_memory_block memorys;
	struct slice
	{
		size_t slice_id;
		const_memory_block blk;
	};
	std::vector<slice> slices;
public:

	slice_vector();
	slice_vector(const_memory_block blk);

	typedef std::vector<slice>::iterator iterator;
	iterator begin();
	iterator end();
	size_t size() const;
};

//����
//���нӿڼ���״̬δ֪
//����ʵ�ּ���״̬δ֪
//����ʵ��_impl�����ں���ĩβ���һ������bool�����Կ��ƾ�����
class rpcudp
{//���������û�̬ʵ�ֵģ����Կ����е��˷��ڴ�.
public:

	//��ˮģʽ��ͣ��ģʽ��ͣ��ģʽ������vector��Ϊ����
	enum udpmode{flood_mode,stwait_mode,stwait_vector_mode};

	rpcudp();
	rpcudp(socket_type sock, udpmode mode = stwait_mode);
	~rpcudp();

	//��udp��֤�����޴γ����²�������������
	typedef size_t mtime_t;

	int recvfrom(void* buf, size_t bufsize, int flag,
		sockaddr_in& addr, int& addrlen, mtime_t timeout = default_timeout);
	int recvfrom(memory_block, int flag,
		sockaddr_in& addr, mtime_t timeout = default_timeout);

	//��ˮģʽ
	memory_block recvfrom(int flag, sockaddr_in& addr, int& addrlen,
		mtime_t timeout = default_timeout);
	void recvack();//ר����������ack,�ڴ��յ����������͵İ����ᱻ����
	//

	int sendto(const void* buf, size_t bufsize, int flag,
		const sockaddr_in& addr, int addrlen);
	int sendto(const_memory_block blk, int flag, const sockaddr_in& addr);

	socket_type socket() const;
	void setsocket(socket_type sock);
	typedef boost::mutex rwlock;

private:
	typedef rpctimer::mytime_t mytime_t;

	static const size_t mtu = 1500;
	static const mytime_t default_timeout = 300;
	static const memory_block _nop_block;
	static const const_memory_block _const_nop_block;
	typedef char bit;

	static std::shared_ptr<bit> _alloc_memory(size_t size);
	static memory_address _alloc_memory_raw(size_t size);

	static size_t _get_mtu();

	typedef rpctimer::timer_handle timer_handle;

	socket_type _udpsock;
	udpmode _mode;
	rpctimer _timer;
	void _check_ack(timer_handle, sysptr_t);

	typedef slice_vector::iterator slice_iterator;
	struct resend_argument;
	memory_block _try_chose_block();
	resend_argument* _regist_slice(size_t group_id, const sockaddr_in& addr,
		const slice_vector::slice&);

	std::hash_set<ulong64> _registed_slice;

	struct slice_head
	{
		size_t slice_type;
		size_t total_slice;
		size_t group_id;
		size_t slice_id;
		size_t total_size;
	};

	struct recved_record
	{
		size_t accepted;
		memory_block memory;
	};

	struct resend_argument;
	typedef std::list<resend_argument*>::iterator mission_handle;

	boost::interprocess::interprocess_semaphore _sem;
	rwlock _send_list_lock;
	std::list<resend_argument*> _send_list;
	struct resend_argument
	{
		const_memory_block block;
		ulong64 id;
		sockaddr_in addr;
		timer_handle timer_id;
		size_t max_retry;
	};
	
	//һЩ���ݺ���
	void _to_send(size_t group_id, const sockaddr_in& addr,
		slice_iterator begin, slice_iterator end);

	typedef ulong64 packid_type;
	static packid_type _makeid(size_t group_id, size_t slice_id);
	//
	rwlock _recved_dgrm_lock;
	std::hash_set<packid_type> _recved_pack;
	std::hash_map<packid_type, recved_record> _recved_dgrm;
	typedef std::hash_map<packid_type,recved_record>::iterator hash_map_iter;
	hash_map_iter _write_to(
		const slice_head&, const_memory_block blk);

	void _send_ack(slice_head, const sockaddr_in&,int len);
	bool _accept_ack(const slice_head&, const_memory_block);
	std::atomic_size_t _group_id_allocer;
	size_t _alloc_group_id();
	slice_vector _slice(const_memory_block);
	size_t _get_window_size(const sockaddr_in&);
	size_t _get_window_size();
	size_t _wait_ack(const sockaddr_in&, packid_type id);
	bool _filter_ack(memory_block&);
	memory_block _get_range(const slice_head&,
		size_t max_size, memory_block from);
	bool _accept_ack_impl(const slice_head&,size_t slice_id);
	std::pair<bool, memory_block>
		_package_process(const sockaddr_in&,int len, const_memory_block);
	rwlock _recved_pack_lock;
	//��¼�յ����İ�
	
	//��ˮģʽ��
	//�Ǻ�ˮģʽ
	rwlock _recved_lock;
	std::hash_map<packid_type,mission_handle> _recved;
	
	rwlock _findished_lock;
	std::list<recved_record> _finished;
	class sendout_record;
	size_t _dynamic_retry_count(const sendout_record&);
	size_t _dynamic_timeout();
	//utility
	int _sendto_impl(socket_type sock, const_memory_block blk,
		int flag, const sockaddr_in& addr, int addrlen);
	int _recvfrom_impl(socket_type sock, memory_block blk,
		int flag, sockaddr_in& addr, int& addrlen);

};
#endif