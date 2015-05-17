#ifndef ISU_RPC_CS_HPP
#define ISU_RPC_CS_HPP

#include <memory>

#include <WinSock2.h>
#include <rpcdef.hpp>
#include <rpclock.hpp>
#include <archive.hpp>
#include <atomic>
#include <functional>
#include <boost/noncopyable.hpp>
#include <rpcudp.hpp>

#include <rpc_group_client.hpp>
#include <rpc_group_server.hpp>
typedef boost::noncopyable mynocopyable;

#define RPC_SEND_BY_UDP 0x01
#define RPC_SEND_BY_TCP 0x10

//һЩ�������ٴ����ظ��Ķ���
#define RPC_GENERIC_SYNC_ARG rpc_group_client* grp,const sockaddr_in* addrptr,\
	func_ident num,Arg... arg

#define RPC_GENERIC_ASYNC_ARG rpc_group_client* grp,const sockaddr_in* addrptr,\
	rpc_callback fn,sysptr_t callback_arg,\
	func_ident num,Arg... arg

#define RPC_GEN_BASIC \
	 rpc_head head=_make_rpc_head(num);\
	 auto& group = *(grp == nullptr ? &_default_group : grp); \
	 auto& addr=*addrptr;
//

class rpc_client;

class rpc_client_tk
	:mynocopyable
{
public:
	//socket����,client�ĵ�ַ,����rpc_client��Ϊmanager
	rpc_client_tk(client_mode mode, const sockaddr_in& client_addr,
		const sockaddr_in& server_addr, rpc_client* mgr = nullptr);

	 struct rpc_request
	 {
		 rpc_request_msg msg;
		 const_memory_block result;
	 };

	template<class... Arg>
	void rpc_generic(rpc_group_client* group, const sockaddr_in& addr,
		multirpc_callback_real callback, sysptr_t ptr, Arg... arg)
	{
		RPC_GEN_BASIC;
		auto& info = static_cast<multirpc_info&>(
			_ready_rpc(head, true));
		head.info = &info;
		head.id.id = 0;
		auto blk = group.push_mission(head, arg...);
		info.callback = callback;
		info.parment = ptr;
		_send_rpc(group, head.id, addr);
	}

	template<class... Arg>
	const_memory_block rpc_generic(RPC_GENERIC_SYNC_ARG)
	{
		RPC_GEN_BASIC;
		auto& info = static_cast<sync_rpcinfo&>(
			_ready_rpc(head, false));
		head.info = &info;
		head.id.id |= ID_ISSYNC;
		auto blk = group.push_mission(head, arg...); 
		bool send_by = _send_rpc(group, head.id, addr);
		info.flag |= send_by ? RPC_SEND_BY_UDP : RPC_SEND_BY_TCP;
		rpc_request request = _wait_sync_request(head, &info,
			send_by);
//		delete &info;
		return request.result;
	}

	template<class... Arg>
	void rpc_generic_async(RPC_GENERIC_ASYNC_ARG)
	{
 		RPC_GEN_BASIC;
		auto& info = static_cast<async_rpcinfo&>(
			_ready_rpc(head, true));
		head.info = &info;
		head.id.id = 0;
		auto blk = group.push_mission(head, arg...);
		info.callback = fn;
		info.parment = callback_arg;
		_send_rpc(group, head.id, addr);
	}


	//client �߳�,�����ṩ����ʼ������ȷ����ʼ�����
	void operator()(void* initlock = nullptr);
	
	//
	int socktype() const;
	sockaddr_in address() const;
	ulong64 call_count() const;
	rpc_client* mgr() const;

private:
	//��Ա
	rpc_client* _mgr;

	client_mode _mode;
	sockaddr_in _default_server;//tcp,udp��Ĭ�Ϸ�����

	rpcudp _udpsock;
	socket_type _tcpsock;

	rpc_group_client _default_group;//������û��ָ�����ʱ�������
	//������Զ�����rpc���ø�������

	typedef std::atomic_uint_fast64_t asize_t;

#ifndef DISABLE_RPC_CALL_COUNT
	asize_t _rpc_count;
#endif

	rpcid _make_rpcid(func_ident);//���ñ�źͺ�����ŵĻ����
	ulong64 _alloc_callid();//���ñ��

	void _init(client_mode, const sockaddr_in&, const sockaddr_in&, rpc_client*);
	void _initsock(const sockaddr_in&,const sockaddr_in&);

	rpc_head _make_rpc_head(func_ident num);

	static bool _is_async_call(const rpc_head&);
#define SRPC_ARG const sockaddr_in& server,const_memory_block blk

	bool _send_rpc(rpc_group_client&, rpcid id, const sockaddr_in&);//������tcp����udp�������ݰ�
	
	rpcinfo& _ready_rpc(const rpc_head&, bool is_async = false);
	size_t _rpc_end(const rpc_head&,
		const_memory_block, const sockaddr_in* addr,bool is_async = false);

	rpc_request _wait_sync_request(const rpc_head&,
		sync_rpcinfo* info,bool send_by_udp);//�첽rpc�޴����ͺ���

	void _send_udprpc(rpcid id, SRPC_ARG);
	void _send_tcprpc(SRPC_ARG);
	void rpc_client_tk::_rpc_process(const sockaddr_in* addr,
		const_memory_block argbuf);
	//utility ��Լ�����õ�
	size_t _big_packet();

	static bool _is_multicast(const sockaddr_in&);
	static bool _is_same(const sockaddr_in& rhs, const sockaddr_in& lhs);
};

class rpc_client
	:mynocopyable
{
public:
	typedef unsigned long thread_id;

	//server��sock���ͱ����socktypeһ��
	rpc_client(client_mode mode,
		const sockaddr_in& server, size_t client_limit = 0);

#define RGEN(name) _get_client().name(grp,&_address(addrptr),num,arg...)
	template<class... Arg>
	const_memory_block rpc_generic(RPC_GENERIC_SYNC_ARG)
	{
		return RGEN(rpc_generic);
	}

	template<class... Arg>
	void rpc_generic_async(RPC_GENERIC_ASYNC_ARG)
	{
		_get_client().rpc_generic_async(
			grp, &_address(addrptr), fn, callback_arg, num, arg...);
	}
#undef RGEN

	template<class ClientType>
	void init_clients()
	{
		_init_clients_impl([&](const sockaddr_in& addr, rpc_client* mgr)
			->rpc_client_tk*
		{
			auto* ret = new ClientType(_mode, addr, _default_server, mgr);
			return ret;
		});
	}

	void add_client(rpc_client_tk* client);//WARNING:����ʵ�����,client�����ܱ�delete���ͷŵ���Դ
	//���������ṩ
	const sockaddr_in& default_server() const;
	void set_default_server(const sockaddr_in&);
	const size_t client_count() const;
private:

	friend class rpc_client_tk;
	rwlock* getlock();

private:
	client_mode _mode;

	sockaddr_in _default_server;//rpc��������ַ,tcp�±�����Ч,udp�¿��û���rpc����ʱָ��

	typedef std::atomic_size_t asize_t;

	//TODO:_next_index and _get_client����д�ĸ�ϸһ�㣬�Դ�������ƽ�⸺��
	size_t _next_index();
	asize_t _alloc_loop;
	size_t _limit_of_client;

	rpc_client_tk& _get_client();
	//WARNING:_src��_locks�����ڵ�һ��rpc����ǰ��ʼ�����,������ɺ��ܱ��޸�
	std::vector<rpc_client_tk*> _src;
	rwlock _lock;
	std::map<thread_id, rwlock> _locks;//udp�²�������,��������rpc�����߳�

	void _init(client_mode mode, const sockaddr_in& addr, size_t limit);
	void _async(rpc_client_tk&, initlock*);//udp�ͻ��ģʽ��initlock������Ч

	//utility :����������ٴ����ظ��ȵĶ�������
	const sockaddr_in& _address(const sockaddr_in* addr);
	void _init_clients_impl(
		std::function<rpc_client_tk*(const sockaddr_in&, rpc_client*)> client_maker);

};

#endif