#ifndef ISU_RPCUDP_CLIENT_HPP
#define ISU_RPCUDP_CLIENT_HPP

#include <set>

#include <rpcudp_client_detail.hpp>

#include <boost/thread/mutex.hpp>

#include <multiplex_timer.hpp>
#include <memory_helper.hpp>
#include <slice.hpp>
#include <rpcudp_def.hpp>
#include <sliding_window.hpp>

class rpcudp;

class rpcudp_client
	:boost::noncopyable
{
public:
	friend class rpcudp;
	friend struct _retrans_trunk;
	typedef std::size_t size_t;
	typedef shared_memory_block<char> shared_memory;
	typedef shared_memory_block<const char> const_shared_memory;
	/*
		rpcudp主控,必须保证其生存周期
		address:要链接的地址,必须保证身存周期
		default_window:默认发送窗口大小,为0发包调整
		rtt:往返时间,为0则表示发包调整
		cleanup_timeout:clean_ack的超时时间
	*/
	rpcudp_client(rpcudp& manager,
		const udpaddr& address, size_t default_window,
		size_t rtt, size_t cleanup_timeout);
	~rpcudp_client();

	int sendto(const_shared_memory& memory,const udpaddr& dest);
	bool recved_from(const udpaddr& src, const_shared_memory memory);
private:
	typedef udp_spinlock spinlock_t;
	typedef std::atomic_size_t atomic_size_t;

	rpcudp* _manager;
	const udpaddr _address;

	//获取重传的超时时间
	size_t _get_timeout_of_retrans();
	atomic_size_t _rtt;
	double _sliding_factor;

	//获取nagled_ack清理超时时间
	size_t _get_timeout_of_cleanup();
	atomic_size_t _timeout_of_cleanup;
	//发送所需
	sliding_window _window;
	typedef size_t group_ident;

	//发送或等待ack中的组,不再此组中的id,不是已经接受完毕就是还没有创建
	spinlock_t _groups_lock;
	std::map<group_ident, _group> _groups;

	//接受模块

	//缓存ack.-list和bitset两个可选新结构
	spinlock_t _nagle_acks_lock;
	std::map<group_ident, ackbitmap> _nagle_acks;

	//包缓存
	spinlock_t _coms_acks_lock;
	std::map<group_ident, combination> _coms;

	spinlock_t _recved_group_lock;
	std::set<group_ident> _recved_groups;

	std::map<size_t, std::set<size_t>> acks, _nagle_push;
#ifdef _USER_DEBUG
	//重传总次数
	std::atomic_size_t _retry_count;

	//卡在窗口等待上的时间
	std::atomic_uint64_t _blocking_time, _total_bits;
#endif

	//发送

	typedef multiplex_timer::timer_handle timer_handle;
	typedef dynamic_memory_block<char> dynamic_memory_block;
	typedef std::map<group_ident, _group>::iterator group_iterator;

	timer_handle _cleanup_timer;

	void _init();
	void _do_send_to(const group_iterator& iter,
		size_t slice_size, int flag);

	//无lock
	void _try_cleanup_group(size_t group_id);
	void _try_cleanup_group_impl(const group_iterator&);

	dynamic_memory_block _alloc_udpbuf(size_t size);

	size_t _get_mtu();

	static void _retrans_trunk_release(sysptr_t);

	static void _copy_slice_to(dynamic_memory_block& blk,
		const slicer::value_type&);
	//接受

	typedef std::map < group_ident, combination >
		::iterator combin_iterator;

	combin_iterator _get_combination(const udp_header_ex&);

	void _send_ack(const udpaddr& dest,
		size_t group_id, size_t slice_id);

	ackbitmap::get_result
		_ask_ack_impl(size_t max, archive_packages&);

	size_t _process_package_impl(const udpaddr&, const_memory_block);

	void _cleanup_combination(size_t groupd_id);
	void _cleanup_combination_impl(const combin_iterator&);
};
#endif