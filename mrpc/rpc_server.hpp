#ifndef ISU_RPC_SERVER_HPP
#define ISU_RPC_SERVER_HPP

#include <hash_map>

#include <rpcdef.hpp>

#include <WinSock2.h>

#include <rpc_call.hpp>
#include <rpclock.hpp>
#include <rpctimer.hpp>
#include <rpcudp.hpp>
typedef client_mode server_mode;
class rpc_server
{
public:

	rpc_server(server_mode mode,
		const sockaddr_in& addr, size_t backlog = 0, size_t service_thread_limit = 0);
	~rpc_server();

	void start(size_t thread_open=1);
	//������һ���Դ����ǳ�����̣߳���I/O�ȸ���û�������̴߳����������˵�ʱ��
	//�ͻῪ�µ��߳�
	void stop();

	static void register_rpc(func_ident id, rpc_server_call call);

private:
	static std::hash_map<func_ident, rpc_server_call>& rpc_local();

	server_mode _mode;
	size_t _backlog;
	size_t _server_thread_limit;//�������߳���������,��ʱ����
	//û��socket,��Ϊ����ʱ�Ŵ�����
	sockaddr_in _server_addr;

//	rpctimer _timer;
	void _tcp_server(initlock* = nullptr);

	typedef int sockaddr_key;
	static sockaddr_key _make_sockaddr_key(const sockaddr_in&);
	std::hash_map<sockaddr_key, socket_type> _connected;//ֻ����tcp,udp���ģʽ����Ч
	void _udp_server(initlock* =nullptr);
	struct rpc_arg;
	void _register_ack(const rpc_head&, const rpc_arg&);
	rwlock _lock;
	void _rpc_ack_callback(size_t id, sysptr_t arg);
	bool _check_ack(rpcid head);
	struct rpc_arg
	{
		const void* buffer;
		size_t size;
		size_t req;
		sockaddr_in addr;
		socket_type sock;
	};

	std::hash_map<rpcid, rpc_arg> _map;
	void _tcp_client_process(rpcudp& client,
		const sockaddr_in& addr, int addrlen);

	void _udp_client_process(rpcudp& udpsock,
		const sockaddr_in& addr, int addrlen,
		const void* buffer, size_t bufsize);

	void _client_process(rpcudp& sock,
		const sockaddr_in& addr, int addrlen,
		const void* buffer, size_t bufsize, bool send_by_udp);

	void _send(rpcid id,rpcudp& sock, const rpc_s_result&,
		const sockaddr_in& addr, int addrlen, bool send_by_udp);
	
	//utility
	template<class Func>
	auto _async_call(Func fn)
		->decltype(fn())
	{
		return fn();
	}

	socket_type _initsock(int socktype);
	socket_type _rpc_accept(rpcudp& server,
		sockaddr_in& addr, int& addr_length);

	static size_t _big_packet();
};

#endif