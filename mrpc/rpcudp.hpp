#ifndef ISU_RPCUDP_HPP
#define ISU_RPCUDP_HPP

//该UDP特性
//超时重传，防丢包
//允许UDP包无序的状态下防丢包
//给rpc_client_tk和rpc_server专用

#include <rpcdef.hpp>

#include <hash_map>
#include <rpctimer.hpp>
#include <rpclock.hpp>

class rpcudp
{//由于是在用户态实现的，所以可能有点浪费内存.
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
	
	std::map<rpcid, mybuf> _packets;//保存了还未确认的包

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