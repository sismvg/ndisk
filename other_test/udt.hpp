#ifndef ISU_UDT_HPP
#define ISU_UDT_HPP

#include <slice.hpp>
#include <trans_core.hpp>
#include <multiplex_timer.hpp>
//事实上这个类只是个带分片重组功能的wapper

struct udt_recv_result
{//下面的都已经抽象了,也没必要再加啥了
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
		创建一个udp socket并绑定地址
		limit_thread指定了可以最多调用recv的线程数量(I/O完成端口)
		bad_send指定了在包发送失败时该如何处理
	*/
	udt()
		:_groups_lock(BOOST_DETAIL_SPINLOCK_INIT)
	{}
	udt(const udpaddr&, size_t limit_thread = 1);
	udt(const udpaddr&, bad_send_functional bad_send,
		size_t limit_thread = 1);

	/*
		接受数据,错误代码与数据全部放入udt_recv_result中
	*/
	udt_recv_result recv();

	/*
		接受数据,但由放入由用户指定的内存当中
		并且只能接受单个分片.因为无法确定发送者。
		也就没法确定buffer的长度,这回导致溢出
	*/
	io_result recv(bit_type* buffer, size_t size, core_sockaddr&);

	/*
		发送一块数据,send是异步的
		io_result标记了内核socket和用户态socket的状态
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
		关闭socket
	*/
	void close();
	
	/*
		系统socket
	*/
	int native() const;
	/*
		
	*/
	void setsocket(const core_sockaddr&);
	size_t alloc_group_id();
private:
	send_callback _send_callback;
	//当一个组长时间收不到分片,则用这个来提醒
	//	multiplex_timer _group_timer;

	//trans_core_head-group_id-start
	struct _group_ident
	{
		group_id_type group_id;
		size_t slice_start;
		size_t data_size;
	};

	struct _recv_group
	{//由于保证不会重复包.所以不需要bitmap啥的记录是否收到过
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