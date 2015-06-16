#ifndef ISU_MAGIC_MODULE_HPP
#define ISU_MAGIC_MODULE_HPP

#include <vector>
#include <algorithm>
#include <functional>

#include <boost/bind.hpp>
#include <mpl_helper.hpp>

/*
	MagicType-模组的表示,一般为unsigned blah...
	Ret and Arg... 是函数签名,模块函数都必须有相同的签名
*/

//TODO:更加细致的模组分类

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
		传入magic和func的参数对
	*/

	template<class... Funcs>
	magic_modules(Funcs... funcs)
	{
		_init(funcs...);
	}

	/*
		插入一个新模块,返回模块的当前位置
		如果有该模块,返回错误
		*/
	size_t insert_modules(magic_type magic, module_processor fn)
	{
		_impl.push_back(std::make_pair(magic, fn));
		return _impl.size() - 1;
	}

	/*
		把模块插入到指定的位置,返回当前位置
		如果插入失败,返回invailed_pos;
		*/
	size_t insert_modules_before(size_t order,
		magic_type magic, module_processor fn)
	{
		_impl.insert(_impl.begin() + order,
			std::make_pair(magic, fn));
		return order;
	}

	/*
		把模块插入拥有指定的magic_type前面
		返回当前位置信息
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
		返回当前模块数量
		*/
	size_t size() const
	{
		return _impl.size();
	}

	/*
		遍历模块
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
		获取模块的位置
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
		获取目标模块
		*/
	module_processor& get_modules(magic_type magic)
	{
		size_t pos = get_magic_pos(magic);
		if (pos == invailed_pos)
			throw std::runtime_error("");
		return _impl[pos].second;
	}

	/*
		获取模块,size表示
		*/

	std::pair<magic_type, magic_modules> get_modules_info(size_t size)
	{
		return _impl[size];
	}

	/*
		弹出一个模块
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
		给出参数,执行.为了保证传入的Arg不会因为类型系统都变成ref
		所以必须用type给出的参数
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
		开关同异步功能-未实现
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