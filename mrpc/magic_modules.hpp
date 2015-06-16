#ifndef ISU_MAGIC_MODULE_HPP
#define ISU_MAGIC_MODULE_HPP

#include <vector>
#include <algorithm>
#include <functional>

#include <boost/bind.hpp>
#include <mpl_helper.hpp>

/*
	MagicType-ģ��ı�ʾ,һ��Ϊunsigned blah...
	Ret and Arg... �Ǻ���ǩ��,ģ�麯������������ͬ��ǩ��
*/

//TODO:����ϸ�µ�ģ�����

template<class MagicType,class Ret,class Type,class... Arg>
class magic_modules
{
public:
	typedef std::size_t size_t;
	typedef MagicType magic_type;
	typedef std::function<Ret(Type,Arg...)> module_processor;
	typedef std::vector <
		std::pair < magic_type, module_processor >> impl;
	typedef typename impl::value_type value_type;
	typedef typename impl::iterator iterator;
	typedef typename impl::const_iterator const_iterator;

	static const size_t invailed_pos = -1;

	/*
		����magic��func�Ĳ�����
	*/

	template<class... Funcs>
	magic_modules(Funcs... funcs)
	{
		_init(funcs...);
	}

	/*
		����һ����ģ��,����ģ��ĵ�ǰλ��
		����и�ģ��,���ش���
		*/
	size_t insert_modules(magic_type magic, module_processor fn)
	{
		_impl.push_back(std::make_pair(magic, fn));
		return _impl.size() - 1;
	}

	/*
		��ģ����뵽ָ����λ��,���ص�ǰλ��
		�������ʧ��,����invailed_pos;
		*/
	size_t insert_modules_before(size_t order,
		magic_type magic, module_processor fn)
	{
		_impl.insert(_impl.begin() + order,
			std::make_pair(magic, fn));
		return order;
	}

	/*
		��ģ�����ӵ��ָ����magic_typeǰ��
		���ص�ǰλ����Ϣ
		*/
	size_t inster_modules_before_magic(magic_type before,
		magic_type magic, module_processor fn)
	{
		size_t ret = get_magic_pos(before);
		if (ret != invailed_pos)
		{
			_impl.insert(_impl.begin() + ret,
				std::make_pair(magic, fn));
		}
		return ret;
	}

	/*
		���ص�ǰģ������
		*/
	size_t size() const
	{
		return _impl.size();
	}

	/*
		����ģ��
		*/
	iterator begin()
	{
		return _impl.begin();
	}

	iterator end()
	{
		return _impl.end();
	}

	const_iterator begin() const
	{
		return _impl.begin();
	}

	const_iterator end() const
	{
		return _impl.end();
	}

	/*
		��ȡģ���λ��
		*/
	size_t get_magic_pos(magic_type magic)
	{
		auto iter = std::find_if(_impl.begin(), _impl.end(),
			[&](const std::pair<magic_type, module_processor>& val)
			->bool
		{
			return val.first == magic;
		});
		size_t ret = invailed_pos;
		if (iter != _impl.end())
			ret = std::distance(_impl.begin(), iter);
		return ret;
	}

	/*
		��ȡĿ��ģ��
		*/
	module_processor& get_modules(magic_type magic)
	{
		size_t pos = get_magic_pos(magic);
		if (pos == invailed_pos)
			throw std::runtime_error("");
		return _impl[pos].second;
	}

	/*
		��ȡģ��,size��ʾ
		*/

	std::pair<magic_type, magic_modules> get_modules_info(size_t size)
	{
		return _impl[size];
	}

	/*
		����һ��ģ��
		*/
	void pop_modules(size_t size)
	{
		_impl.erase(_impl.begin() + size);
	}

	void pop_modules_by_magic(magic_type magic)
	{
		pop_modules(get_magic_pos(magic));
	}

	/*
		��������,ִ��.Ϊ�˱�֤�����Arg������Ϊ����ϵͳ�����ref
		���Ա�����type�����Ĳ���
		*/
	void execute(Arg... arg)
	{
		execute_as([&](std::pair<magic_type, module_processor>& pair)
		{
			pair.second(pair.first, arg...);
		});
	}

	typedef std::function < void
		(std::pair<magic_type, module_processor>&) > invoker;

	void execute_as(invoker fn)
	{
		std::for_each(begin(), end(), fn);
	}

	/*
		����ͬ�첽����-δʵ��
		*/
	//void async_control(bool enable);
private:

	void _init_impl()
	{}

	void _init_impl(const value_type& modules)
	{
		insert_modules(modules.first, modules.second);
	}

	template<class... Funcs>
	void _init_impl(const value_type& modules,Funcs... funcs)
	{
		insert_modules(modules);
		_init_impl(funcs...);
	}

	template<class... Funcs>
	void _init(Funcs... funcs)
	{
		_init_impl(funcs...);
	}

	impl _impl;
};
#endif