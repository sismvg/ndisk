#ifndef ISU_ACKBITMAP_HPP
#define ISU_ACKBITMAP_HPP

#include <xstddef>
#include <map>
#include <set>
#include <functional>

#include <memory_helper.hpp>
#include <archive_packages.hpp>

#include <multiplex_timer.hpp>
#include <boost/dynamic_bitset.hpp>

#define DEFINE_BIT_FLAG_HTONL(pos)\
	(static_cast<size_t>(1)<<pos)

#define ACK_SINGAL DEFINE_BIT_FLAG_HTONL(6)
#define ACK_MULTI DEFINE_BIT_FLAG_HTONL(7)
#define ACK_COMPRESSED DEFINE_BIT_FLAG_HTONL(8)

struct acks
{
	typedef shared_memory_block<const char> const_shared_memory;
	const_shared_memory block;
};

memory_block archive(const acks&);
void rarchive(void* buffer, size_t size, const acks&);

class ackbitmap
{
public:
	typedef std::size_t size_t;
	typedef size_t index_type;
	typedef shared_memory_block<char> shared_memory;
	typedef std::map < size_t,
		std::pair < multiplex_timer::timer_handle, size_t >> impl;
	typedef impl::value_type value_type;
	typedef impl::iterator iterator;
	typedef impl::const_iterator const_iterator;
	/*
		size is how many ident memory reserve.
		you can set it for more better speed.
		no set the class work too.
	*/
	ackbitmap(size_t size = 0);
	
	/*
		get values.don't try to resolve acks.
		and it will delete that value
	*/

	struct get_result
	{
		get_result()
			:ack_count(0), group_count(1)
		{}

		get_result(size_t ack)
			:ack_count(ack), group_count(1)
		{}
		size_t ack_count;
		size_t group_count;
	};

	get_result get(size_t limit, archive_packages&);
	/*
		put acks value. it must get from function "get"
	*/

	struct put_result
	{
		put_result()
			:accepted(0), used_memory(0)
		{}

		put_result(size_t acc, size_t used)
			:accepted(acc), used_memory(used)
		{}

		size_t accepted;
		size_t used_memory;
	};

	typedef multiplex_timer::timer_handle timer_handle;
	typedef std::function<void(size_t,timer_handle&)> ack_callback;

	put_result put(const acks&, ack_callback);
	put_result put(const_memory_block);
	put_result put(const_memory_block, ack_callback);
	
	/*
		添加size个要等待的ack
	*/
	void extern_to(size_t size);

	/*
		已经收到的ack的数量
	*/
	size_t got() const;

	/*
		平台相关
	*/
	void set_handle(size_t slice_id, timer_handle);
	timer_handle get_handle(size_t slice_id);
	
	/*
		添加一个新的要等待的ack
	*/
	void add(index_type, timer_handle);

	/*
		this ident is checked?
	*/
	bool have(index_type) const;

	/*
		全部都有了
	*/
	bool accpeted_all() const;

	/*
		bitmap size
	*/
	size_t size() const;
	/*
		clear bitmap. but not release memory.
	*/
	void clear();

	/*
		迭代器区间
	*/
	iterator begin();
	iterator end();
	const_iterator begin() const;
	const_iterator end() const;
private:
	size_t _got;
	
	//boost::dynamic_bitset<unsigned long> _bitset;
	impl _set;

	size_t _set_ack(size_t key, ack_callback);
	size_t _get_ack();
};

#endif