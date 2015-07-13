#ifndef ISU_RPC_SERVER_HPP
#define ISU_RPC_SERVER_HPP

#include <hash_map>

#include <udt.hpp>
#include <rpcdef.hpp>
#include <rpc_call.hpp>
#include <rpclock.hpp>
#include <rpc_group.hpp>

typedef size_t(*rpc_server_call)(const_memory_block, \
	const rpc_head&, rpc_request_msg, remote_produce_group&);

class rpc_server
{
public:
	typedef udt rpcudp;
	typedef int socket_type;
	/*
		mode:How to trans data.
		local:server address.
		server_thread_limit:For support C/S model.
	*/
	rpc_server(transfer_mode mode, const udpaddr& local,
		size_t server_thread_limit = 0);
	~rpc_server();

	/*
		Start server and force start thread_open thread
	*/
	void run(size_t thread_open = 1);

	/*
		Stop all server thread
	*/
	void stop();

	/*
		register a remote produce call.
		id:rpc ident
		call:do rarchive and archive
	*/
	static void register_rpc(func_ident id, rpc_server_call call);
private:
	static std::hash_map<func_ident, rpc_server_call>& rpc_local();

	transfer_mode _mode;
	size_t _server_thread_limit;//服务器线程数量极限,暂时无用
	udpaddr _local;

	rwlock _socks_lock;

	typedef socket_type raw_socket;

	std::vector<raw_socket> _socks;
	std::vector<std::thread*> _threads;

	void _tcp_server(active_event*);
	void _udp_server(active_event*);

	void _tcp_client_process(socket_type client, const udpaddr&);

	void _udp_client_process(rpcudp& udpsock,
		const udpaddr&, const_memory_block);

	void _client_process(rpcudp& sock, const udpaddr&,
		const_memory_block blk, bool send_by_udp);

	void _send(rpcudp& sock, const_memory_block,
		const udpaddr& addr, bool send_by_udp);
	
	//utility

	socket_type _initsock(int socktype);

	socket_type _rpc_accept(socket_type server,
		udpaddr& addr, int& addr_length);

};

#endif