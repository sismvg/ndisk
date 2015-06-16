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
	TΪ���ݵ�����
	Host��һ���ڴ�,����֧��begin
*/

template<class T,class Host>
class archive_packages_object_proxy
{
	//һ������,���ڰ�ȫ������archive_packages������T��ָ��
	//�����ͱ�����POD�������������ݰ��ձ���memory order��ȫ���л�
	//�ұ߽����
public:
	typedef std::size_t slot_type;
	typedef T value_type;
	typedef Host host_type;
	typedef archive_packages_object_proxy<T,Host> self_type;

	/*
		Host:ָ��һ���ڴ�
		slot:һ������,��Host�и�����ָ����ڴ涼����T������
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
		��ȡslotָ���T������
		��Щ�������Ǹ���slot��̬��������.ֻҪhost�����ݴ���
		�Ͳ��ᷢ������
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
		��ȡ���ݵ�ԭʼָ��,���ǵ�host�ı�T���п��ܻ�ʧЧ
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
		������host�е�����
	*/
	slot_type slot() const
	{
		return _slot;
	}

	/*
		�ı������
	*/
	void change_slot(slot_type slot)
	{
		_slot = slot;
	}

	/*
		��ȡ����
	*/
	Host* get_host() const
	{
		return _host;
	}

	/*
		�ı�����
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
	//��̬������,��̬������,��̬������-���ǽ���ʱ�����ڰ����ȸı�(����Ҫ��¼������)
	typedef archive_packages self_type;
	enum package_type{ concrete, dynamic, dynamic_analysis };

	/*
		���Ĵ�С,typeΪdynamic,dynamic_analysis����Ч
		��������.concrete��package_sizeָ�����Ĵ�С
		reserve:Ԥ�����ڴ�
	*/
	archive_packages(size_t package_size,
		package_type type = concrete, size_t reserve = 0);

	/*
		������б�push_back������
		�ظ���_init���״̬
	*/
	void clear();

	/*
		���0һ������
	*/
	void fill(size_t count);

	/*
		���һ������
	*/
	size_t push_back(const_memory_block);

	/*
		���һ������,�����ظ��������ڴ��еĴ���
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
		������ֱ��archive���ڴ�
		����������archive_package�ڴ��еĿ�ͷ����
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
		���ݵ�����,������Ԥ����
	*/
	typedef void* iterator;
	typedef const iterator const_iterator;

	iterator begin();
	const_iterator begin() const;
	iterator end();
	const_iterator end() const;

	/*
		�Ѿ�����˶�������
	*/
	size_t size() const;

	/*
		�����˶��ٰ�
	*/
	size_t package_count() const;

	/*
		�Ѿ�ʹ���˶����ڴ�,����Ԥ����.
		��Ҫ֪��׼ȷʹ���˶�����memory().second
	*/
	size_t memory_size() const;

	/*
		���Ĺ̶���С,dynamic_analysis,dynamic����Ч
	*/
	size_t package_size() const;

	/*
		��������
	*/
	package_type type() const;

	typedef shared_memory_block<char> shared_memory;;
	typedef shared_memory_block<const char> const_shared_memory;

	typedef std::pair<shared_memory, size_t> memory_type;
	typedef std::pair<const_shared_memory, size_t> const_memory_type;


	/*
		push_backһ��ռ���˶��ٵ��ڴ�
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
	//dynamic_analysis����Ч
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
		��Ҫ��archie_packages��ʹ�ò����ص��ڴ�
	*/

	/*
		��ΪtypeΪdynamic��ʱ����������ṩһ������
	*/
	packages_analysis(const_memory_block block);

	/*
		��ȡ��һ����û�����ɵĿ�
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
		��ȡ�Ѿ����ɵĿ�
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
		���Ѿ������˵Ŀ���б���
	*/
	iterator begin();
	iterator end();

	const_iterator begin() const;
	const_iterator end() const;

	/*
		��������,��һ��O(n),�Ժ�O(1)
	*/
	size_t size() const;

	/*
		�Ѿ���������O(1)
	*/
	size_t analysised() const;

private:
	const_memory_block _memory;
	impl _impl;
	size_t _size;

	//�����￪ʼ������
	size_t _start;
	archive_packages::packages_info _info;

	void _init(const_memory_block);
	void _generic();
};

#endif