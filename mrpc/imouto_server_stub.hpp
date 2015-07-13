#ifndef ISU_IMOUTO_SERVER_STUB_HPP
#define ISU_IMOUTO_SERVER_STUB_HPP

#include <imouto_rpcstub.hpp>
#include <serialize.hpp>
#include <xdr.hpp>
template<class IdentType>
class basic_server_stub
	:public basic_stub<IdentType>
{
public:
	typedef IdentType ident_type;
	typedef basic_stub<IdentType> father_type;

	basic_server_stub(const ident_type& ident)
		:father_type(ident)
	{}

	virtual memory_block invoke(const const_memory_block&) const = 0;
	virtual memory_block operator()(const const_memory_block&) const = 0;
};

struct mymaker
{
	template<class T>
	static typename real_type<T>::type functional()
	{
		return real_type<T>::type();
	}
};

struct nop_meta_functional
{
	template<class T>
	struct functional
	{
		static const bool value = true;
		typedef std::true_type type;
	};
};

template<
	class IdentType,class SerializeObject,
	class Func,class R,class... Arg>
class imouto_server_stub
	:public basic_server_stub<IdentType>
{
public:
	typedef IdentType ident_type;
	typedef SerializeObject serialize_object;
	typedef R result_type;
	typedef basic_server_stub<IdentType> father_type;
	typedef Func functional_type;

	imouto_server_stub(const ident_type& ident,functional_type func)
		:father_type(ident), _real_function(func)
	{}

	struct server_stub_functional
		:public meta_functional
	{//没有模板lambda的临时解决方案
		server_stub_functional(serialize_object& core,
			const functional_type& func,const const_memory_block& memory)
			:_memory(memory), _core(&core), _func(&func)
		{}

		template<class... Parments>
		void operator()(Parments&&... parments)
		{
			_core->rarchive_real(_memory,type_list<Arg...>(), parments...);
			_invoke_impl(std::is_void<result_type>(), parments...);
		}

		const memory_block& results() const
		{
			return _results_memory;
		}
	private:

		template<class... Parments>
		void _invoke_impl(std::true_type, Parments&&... parments)
		{
			(*_func)(parments...);
			_results_memory = _core->archive_real(
				type_list<Arg...>(), parments...);
		}

		template<class... Parments>
		void _invoke_impl(std::false_type, Parments&&... parments)
		{
			result_type ret = (*_func)(parments...);
			_results_memory = _core->archive_real(
				type_list<result_type&, Arg...>(),
				ret, parments...);
		}

		const functional_type* _func;
		serialize_object* _core;
		memory_block _results_memory;
		const_memory_block _memory;
	};

	virtual memory_block invoke(const const_memory_block& memory) const
	{
		/*
			流程
			反序列化参数
			执行真实函数
			序列化返回值
		*/
		serialize_object core;
		server_stub_functional server_stub_call(core, _real_function, memory);
		call_with_filter<nop_meta_functional, mymaker>(
			type_list<Arg...>(), server_stub_call);
		return server_stub_call.results();
	}

	virtual memory_block operator()(const const_memory_block& memory) const
	{
		return invoke(memory);
	}
private:
	functional_type _real_function;
};

template<class SerializeObject,class IdentType,class R,class... Arg>
imouto_server_stub<IdentType,SerializeObject,R(*)(Arg...),R,Arg...>
	basic_make_server_stub(const IdentType& ident, R(*fn)(Arg...))
{
	return imouto_server_stub < IdentType,
		SerializeObject, R(*)(Arg...), R, Arg... > (ident, fn);
}

#endif