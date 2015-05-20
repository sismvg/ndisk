#ifndef ISU_RPCUDP_HPP
#define ISU_RPCUDP_HPP

//该UDP特性
//超时重传，防丢包
//允许UDP包无序的状态下防丢包
//给rpc_client_tk和rpc_server专用

#include <rpcdef.hpp>

#include <list>
#include <vector>
#include <memory>
#include <hash_map>
#include <rpctimer.hpp>
#include <rpclock.hpp>
#include <hash_set>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/thread/mutex.hpp>

inline bool operator<(const sockaddr_in& lhs, const sockaddr_in& rhs)
{
	return lhs.sin_addr.S_un.S_addr < rhs.sin_addr.S_un.S_addr;
}


inline size_t hash_value(const sockaddr_in& addr)
{
	return std::hash_value(addr.sin_addr.S_un.S_addr);
}
namespace isu
{
	template<class Iter, class Size, class OutIter>
	Iter copy_limit(Iter begin, Iter end,
		Size limit, OutIter out)
	{
		Size count = 0;
		for (; begin != end&&count < limit; ++count, ++begin, ++out)
		{
			*out = *begin;
		}
		return begin;
	}
}

//ack包,数据包,数据包带上ack,要求本机给出某个包的ack
#define IS_ACK_PACK 0x01
#define IS_MAIN_PACK 0x10
#define IS_MAIN_PACK_WITH_ACK 0x100
#define IS_ACK_QUERY 0x1000

//ack flag
#define UDP_ACK_SINGAL 0x10000
#define UDP_ACK_MULTI 0x100000
#define UDP_ACK_COMMPRESSED 0x01000000
//recv flag
#define WHEN_RECV_ACK_BREAK 0x10000000

//udpmode
#define RECV_ACK_BY_SELF 0x1
#define RECV_ACK_BY_UDP 0x10
#define DISABLE_ACK_NAGLE 0x100
#define ENABLE_SENDTO_NAGLE 0x1000

struct packid_type
{
	packid_type(size_t gpid = 0, size_t slid = 0)
		:_group_id(gpid), _slice_id(slid)
	{}

	packid_type(const ulong64 val)
	{
		_group_id = val >> 32;
		_slice_id = val & 0x00000000;
	}

	inline operator ulong64() const
	{
		return to_ulong64();
	}
	inline ulong64 to_ulong64() const
	{
		ulong64 ret = _group_id;
		ret <<= 32;
		ret |= _slice_id;
		return ret;
	}
	size_t _group_id;
	size_t _slice_id;
};
/*
inline ulong64 hash_value(const packid_type& _Keyval)
{	// hash _Keyval to size_t value one-to-one
	return (_Keyval.to_ulong64() ^ _HASH_SEED);
}*/

inline bool operator<(const packid_type& lhs, const packid_type& rhs)
{
	return lhs.to_ulong64() < rhs.to_ulong64();
}

inline bool operator==(const packid_type& lhs, const packid_type& rhs)
{
	return lhs.to_ulong64() == rhs.to_ulong64();
}

inline bool operator>(const packid_type& lhs, const packid_type& rhs)
{
	return lhs.to_ulong64() > rhs.to_ulong64();
}

inline bool operator==(const sockaddr_in& lhs, const sockaddr_in& rhs)
{
	return lhs.sin_port == rhs.sin_port
		&&lhs.sin_addr.S_un.S_addr == rhs.sin_addr.S_un.S_addr;
}
struct slice_vector
{
public:
	size_t group_id;
	size_t accepted;
	const_memory_block memorys;
	struct slice
	{
		size_t slice_id;
		memory_block blk;
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

//规则
//所有接口加锁状态未知
//所有实现加锁状态未知
//所有实现_impl允许在函数末尾添加一个或多个bool参数以控制具名锁
class rpcudp
{//由于是在用户态实现的，所以可能有点浪费内存.
public:

	typedef sockaddr_in udpaddr;

	rpcudp();
	rpcudp(socket_type sock, size_t mode = RECV_ACK_BY_SELF);
	~rpcudp();

	typedef size_t mtime_t;

	int recvfrom(void* buf, size_t bufsize, int flag,
		udpaddr& addr, int& addrlen, mtime_t timeout = default_timeout);
	int recvfrom(memory_block, int flag,
		udpaddr& addr, mtime_t timeout = default_timeout);

	//洪水模式
	memory_block recvfrom(int flag, udpaddr& addr, int& addrlen,
		mtime_t timeout = default_timeout);
	void recvack();//专门用来接收ack,在此收到的其他类型的包都会被丢掉
	//

	int sendto(const void* buf, size_t bufsize, int flag,
		const udpaddr& addr, int addrlen);
	int sendto(const_memory_block blk, int flag, const udpaddr& addr);

	//debug用
	void wait_all_ack();
	void set_rate_of_packet_loss(double);//设置丢包率
	//

	socket_type socket() const;
	socket_type deatch();//会导致出了closesocket以外的所有析构任务
	void setsocket(socket_type sock);//会执行所有析构任务,没啥大用
private:
	typedef boost::mutex rwlock;
	typedef rpctimer::mytime_t mytime_t;
	//debug用
	initlock _recved_init;
	//
	void _init();
	static const size_t mtu = 1500;
	static const mytime_t default_timeout = 300;
	static const memory_block _nop_block;
	static const const_memory_block _const_nop_block;

	typedef char bit;
	static size_t _get_max_ackpacks();
	static std::shared_ptr<bit> _alloc_memory(size_t size);
	static memory_address _alloc_memory_raw(size_t size);
	static size_t _get_mtu();

	typedef rpctimer::timer_handle timer_handle;

	size_t _udpmode;
	socket_type _udpsock;
	rpctimer _timer, _ack_timer;

	typedef std::list<packid_type> acks;
	typedef sockaddr_in udpaddr;

	struct slice_head
	{
		size_t slice_type;
		size_t slice_count;
		size_t packsize;
		int crc;
		int slice_crc;
		size_t start;
		size_t size;
		packid_type packid;
	};

	//ack,等待发送的
	rwlock _acklock;
	std::hash_map<udpaddr, acks> _ack_list;
	std::vector<packid_type> _reqack(const udpaddr&);
	std::vector<packid_type> _reqack_impl(const udpaddr&, size_t max);
	std::vector<packid_type> _reqack_impl(acks&, size_t max);
	void _timed_clear_ack(timer_handle handle, sysptr_t nop);
	void _send_ack(slice_head, const udpaddr&);
	
	rwlock _ack_waitlist_lock;
	std::hash_map<udpaddr, std::hash_set<packid_type>> _ack_waitlist;//等待接受的ack

	size_t _wait_ack(const udpaddr&, packid_type id);
	size_t _accept_ack(slice_head&,
		const udpaddr& addr, const_memory_block& data);

	size_t _accept_ack_impl(const udpaddr&, packid_type id);
	void _check_ack(timer_handle, sysptr_t resend_arg);
	size_t _filter_ack(const udpaddr&, memory_block& blk);//过滤并接受ack
	//缓存数据包
	struct recved_record
	{//-1表示全部收到
		recved_record()
			:_accepted(0), _data(nullptr,0)
		{}

		recved_record(memory_block blk, bool entrust_delete = false)
			:_accepted(0), _data(blk), _mydelete(entrust_delete)
		{}

		~recved_record()
		{
			if (_mydelete)
				delete _data.buffer;
		}

		inline void imaccept()
		{
			++_accepted;
		}
		inline void set_accpeted(size_t val)
		{
			_accepted_val.insert(val);
		}
		inline memory_block set_fin()
		{
			_mydelete = false;
			_accepted = -1;
			return _data;
		}
		inline bool is_fin() const
		{
			return _accepted == -1;
		}
		inline memory_block data()
		{
			return _data;
		}
		inline size_t accepted() const
		{
			return _accepted;
		}
		inline bool is_accepted(size_t id) const
		{
			auto iter = _accepted_val.find(id);
			return iter != _accepted_val.end();
		}
	private:
		bool _mydelete;
		size_t _accepted;
		memory_block _data;
		std::hash_set<size_t> _accepted_val;
	};

	rwlock _recved_lock;
	std::hash_map < udpaddr,
		std::map < packid_type, recved_record >> _recved;
	//超时重传
	struct resend_argument
	{
		const_memory_block data;
		packid_type packid;
		udpaddr addr;
		timer_handle handle;
		size_t retry_limit;
	};
	resend_argument* _make_resend_argment(
		size_t group_id, const udpaddr& addr, const slice_vector::slice& sli);
	void _release_resend_argument(resend_argument*);
	//传输
	boost::interprocess::interprocess_semaphore _window;
	std::atomic_size_t _group_id_allocer;

	size_t _alloc_group_id();
	typedef slice_vector::iterator slice_iterator;
	void _to_send(int flag, size_t group_id, const udpaddr& addr,
		slice_iterator begin, slice_iterator end);

	typedef std::map<packid_type,
		recved_record>::iterator hash_map_iter;

	hash_map_iter _write_to(const slice_head&,
		const udpaddr& addr, const_memory_block blk);
	slice_vector _slice(const_memory_block, size_t keep,int crc);//keep为ack保留
	memory_block _get_data_range(const slice_head& head,
		size_t max_size, memory_block from);//分片头:最高分配多少数据:从哪里分配
	void _recvack(const udpaddr* addr);
	struct package_process_ret
	{
		size_t accepted_ack;
		memory_block data;
	};

	package_process_ret _package_process(
		const udpaddr&, int len, const_memory_block);

	size_t _get_window_size();
	size_t _get_window_size(const udpaddr&);
	size_t _dynamic_timeout();

	//utility
	int _sendto_impl(socket_type sock, const_memory_block blk,
		int flag, const udpaddr& addr, int addrlen);
	int _recvfrom_impl(socket_type sock, memory_block blk,
		int flag, udpaddr& addr, int& addrlen);
};
#endif