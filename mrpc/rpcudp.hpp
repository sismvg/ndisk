#ifndef ISU_RPCUDP_HPP
#define ISU_RPCUDP_HPP

//��UDP����
//��ʱ�ش���������
//����UDP�������״̬�·�����
//��rpc_client_tk��rpc_serverר��

#include <rpcdef.hpp>

#include <hash_map>
#include <rpctimer.hpp>
#include <rpclock.hpp>

class rpcudp
{//���������û�̬ʵ�ֵģ����Կ����е��˷��ڴ�.
public:
	rpcudp();
	rpcudp(socket_type sock);
	~rpcudp();

	typedef size_t mtime_t;
	int recvfrom(void* buf, size_t bufsize, int flag,
		sockaddr_in& addr, int& addrlen, mtime_t timeout = default_timeout);

	int sendto(rpcid id, const void* buf, size_t bufsize, int flag,
		const sockaddr_in& addr, int addrlen, mtime_t timeout = default_timeout);
	socket_type socket() const;
	void setsocket(socket_type sock);
private:
	socket_type _udpsock;
	rpctimer _timer;

	typedef rpctimer::timer_handle timer_handle;
	typedef rpctimer::mytime_t mytime_t;
	static const mytime_t default_timeout = 300;

	struct mybuf
	{
		mybuf();
		mybuf(const void* buf, size_t sz);
		const void* buffer;
		size_t size;
		sockaddr_in addr;
		timer_handle timer_id;
	};

	struct packet_head
	{
		size_t pack_type;
		rpcid ack_id;
		static const size_t pack_ack = 0, main_pack = 1;
		static const size_t main_pack_and_ack = 2;
		static const size_t ack_requery = 3;
	};

	rwlock _lock;
	
	std::map<rpcid, mybuf> _packets;//�����˻�δȷ�ϵİ�

	void _resend_packet(rpcid id, mybuf& buf);
	void _send_ack(const packet_head&, const sockaddr_in& addr, int addrlen);

	void _accept_ack(const packet_head&);
	void _ack_callback(timer_handle timer_id, sysptr_t arg);

	typedef std::pair<const char*, size_t> buftype;
	void _waitlist_of_ack(rpcid, buftype, const sockaddr_in& recver);

	buftype _make_buf(rpcid id, const void* buf, size_t size,
		size_t pack_type = packet_head::main_pack);

	//utility
	int _sendto_impl(socket_type sock, const void* buf, size_t size,
		int flag, const sockaddr_in& addr, int addrlen);
	int _recvfrom_impl(socket_type sock, void* buf, size_t size,
		int flag, sockaddr_in& addr, int& addrlen);

};
#endif