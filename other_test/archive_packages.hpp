#ifndef ISU_ARCHIVE_PACKAGES_HPP
#define ISU_ARCHIVE_PACKAGES_HPP

#include <vector>

#include <archive_def.hpp>
#include <memory_helper.hpp>
class archive_packages;
class packages_analysis;

struct block_wapper
{
	block_wapper(const_memory_block blk)
		:_blk(blk)
	{}

	const_memory_block _blk;
};

size_t archived_size(const block_wapper& wapper);
std::size_t archive_to(void* buffer,
	size_t size, const block_wapper& wapper);
memory_block archive(const block_wapper& wapper);

/*
	T为数据的类型
	Host是一块内存,必须支持begin
*/

template<class T,class Host>
class archive_packages_object_proxy
{
	//一个代理,用于安全访问在archive_packages的类型T的指针
	//该类型必须是POD类型且所有数据按照本机memory order完全序列化
	//且边界对齐
public:
	typedef std::size_t slot_type;
	typedef T value_type;
	typedef Host host_type;
	typedef archive_packages_object_proxy<T,Host> self_type;

	/*
		Host:指向一块内存
		slot:一个索引,在Host中该索引指向的内存都必是T的数据
	*/
	archive_packages_object_proxy()
		:_host(nullptr), _slot(0)
	{}

	template<class Host>
	archive_packages_object_proxy(Host* host,slot_type slot)
		:_host(host), _slot(slot)
	{

	}

	/*
		获取slot指向的T的数据
		这些函数都是根据slot动态索引数据.只要host的数据存在
		就不会发生错误
	*/
	operator T*()
	{
		return get();
	}

	operator const T*() const
	{
		return get();
	}

	T* operator->()
	{
		return get();
	}

	const T* operator->() const
	{
		return get();
	}

	/*
		获取数据的原始指针,但是当host改变T就有可能会失效
	*/
	T* get()
	{
		char* iter =
			reinterpret_cast<char*>(_host->begin());
		std::advance(iter, _slot);
		return reinterpret_cast<T*>(iter);
	}

	const T* get() const
	{
		return const_cast<self_type*>(this)->get();
	}

	/*
		数据在host中的索引
	*/
	slot_type slot() const
	{
		return _slot;
	}

	/*
		改变该索引
	*/
	void change_slot(slot_type slot)
	{
		_slot = slot;
	}

	/*
		获取宿主
	*/
	Host* get_host() const
	{
		return _host;
	}

	/*
		改变宿主
	*/
	void reattach(Host* host)
	{
		_host = host;
	}
private:
	host_type* _host;
	slot_type _slot;
};


class archive_packages
{
public:
	//静态包长度,动态包长度,动态包长度-但是解析时会由于包长度改变(必须要记录包长度)
	typedef archive_packages self_type;
	enum package_type{ concrete, dynamic, dynamic_analysis };

	/*
		包的大小,type为dynamic,dynamic_analysis下无效
		包的类型.concrete下package_size指定包的大小
		reserve:预留的内存
	*/
	archive_packages(size_t package_size,
		package_type type = concrete, size_t reserve = 0);

	/*
		清空所有被push_back的数据
		回复到_init后的状态
	*/
	void clear();

	/*
		填充0一块数据
	*/
	void fill(size_t count);

	/*
		添加一块数据
	*/
	size_t push_back(const_memory_block);

	/*
		添加一块数据,并返回该数据在内存中的代理
	*/
	template<class T>
	archive_packages_object_proxy<T,self_type>
		push_value(const T& value)
	{
		size_t slot = push_back(make_block(value));
		return archive_packages_object_proxy<T, self_type>
			(this, slot);
	}

	/*
		把数据直接archive到内存
		返回数据在archive_package内存中的开头索引
	*/
	template<class... Arg>
	size_t archive_in_here(bool reserve, const Arg&... arg)
	{
		size_t result = this->size();
		size_t size = archived_size(arg...);

		bool is_dynamic_anlaysis =
			(this->type() == dynamic_analysis);

		size_t real_size = size +
			(is_dynamic_anlaysis ? sizeof(size_t) : 0);
		_memory.alloc_to_copy(real_size,
			[&](memory_block block)
		{
			if (is_dynamic_anlaysis)
			{
				archive_to(block.buffer, block.size, size, arg...);
			}
			else
			{
				archive_to(block.buffer, block.size, arg...);
			}
		}, reserve);
		++_slot(_slot_of_size);
		return result + (real_size - size);
	}
	/*
		数据的区间,不包括预留的
	*/
	typedef void* iterator;
	typedef const iterator const_iterator;

	iterator begin();
	const_iterator begin() const;
	iterator end();
	const_iterator end() const;

	/*
		已经添加了多少数据
	*/
	size_t size() const;

	/*
		加入了多少包
	*/
	size_t package_count() const;

	/*
		已经使用了多少内存,包括预留的.
		想要知道准确使用了多少请memory().second
	*/
	size_t memory_size() const;

	/*
		包的固定大小,dynamic_analysis,dynamic下无效
	*/
	size_t package_size() const;

	/*
		包的类型
	*/
	package_type type() const;

	typedef shared_memory_block<char> shared_memory;;
	typedef shared_memory_block<const char> const_shared_memory;

	typedef std::pair<shared_memory, size_t> memory_type;
	typedef std::pair<const_shared_memory, size_t> const_memory_type;


	/*
		push_back一共占用了多少的内存
	*/
	memory_type memory();
	const_memory_type memory() const;
	//

private:
	typedef dynamic_memory_block<char> dynamic_memory;

	void _init(size_t size, package_type type);

	char* _start();
	const char* _start() const;
	
	char* _last();

	void _try_extern_memory(size_t size, bool reserve = true);
	size_t& _slot(size_t index);
	const size_t& _slot(size_t index) const;

	struct packages_info
	{
		size_t package_count;
		package_type type;
		size_t avg_size;
	};

	static std::pair<size_t, packages_info>
		from_memory(const_memory_block);

	friend class packages_analysis;

	size_t _slot_of_size;
	size_t _slot_of_type;
	//dynamic_analysis下无效
	size_t _slot_of_package_size;
	size_t _type;
	dynamic_memory _memory;
};

class packages_analysis
{
public:
	typedef shared_memory_block<char> shared_memory;
	typedef shared_memory_block<const char> const_shared_memory;

	typedef std::vector<const_memory_block> impl;

	/*
		需要是archie_packages的使用并返回的内存
	*/

	/*
		当为type为dynamic的时候解析必须提供一个函数
	*/
	packages_analysis(const_memory_block block);

	/*
		获取下一个还没有生成的块
	*/
	const_memory_block next_block();

	template<class Func>
	void next_block(Func fn)
	{
		const_memory_block blk(
			reinterpret_cast<const char*>(_memory.buffer) + _start, 
			_memory.size - _start);

		if (blk.size == 0)
			return;

		size_t size = 0;
		_start += (size = fn(_size, blk));

		blk.size = size;
		_impl.push_back(blk);
		++_size;
		if (_start > _memory.size)
		{
			throw std::out_of_range("memory size out of range");
		}
	}
	/*
		获取已经生成的块
	*/
	void generic_all();

	template<class Func>
	void generic_all(Func fn)
	{
		size_t pre = 0;
		do
		{
			pre = _impl.size();
			next_block(fn);
		} while (_impl.size() != pre);
	}
	typedef impl::iterator iterator;
	typedef impl::const_iterator const_iterator;

	/*
		对已经存在了的块进行遍历
	*/
	iterator begin();
	iterator end();

	const_iterator begin() const;
	const_iterator end() const;

	/*
		包的数量,第一次O(n),以后O(1)
	*/
	size_t size() const;

	/*
		已经解析过的O(1)
	*/
	size_t analysised() const;

private:
	const_memory_block _memory;
	impl _impl;
	size_t _size;

	//从哪里开始解析块
	size_t _start;
	archive_packages::packages_info _info;

	void _init(const_memory_block);
	void _generic();
};

#endif