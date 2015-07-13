#ifndef IMOUTO_CLIENT_STUB_HPP
#define IMOUTO_CLIENT_STUB_HPP

#include <imouto_rpcstub.hpp>

#include <type_list.hpp>

template<class IdentType,class SerializeObject,class R,class... Arg>
class imouto_client_stub
	:public basic_stub<IdentType>
{
public:

	typedef IdentType ident_type;
	typedef SerializeObject serialize_object;
	typedef basic_stub<IdentType> father_type;
	typedef R result_type;

	imouto_client_stub(const ident_type& ident)
		:father_type(ident)
	{}

	/*
		���������л������real,�����ط���ֵ�����õȷ�����������
	*/
	//����Ҫ��Func�ǿ������
	//WAR.Arg���û�������������
	memory_block push(Arg... parments) const
	{
		serialize_object core;
		return core.archive_real(type_list<Arg...>(), parments...);
	}

	/*
		�����ķ����л�����,parments�����Ǵ���invoke�Ĳ���
	*/
	template<class... Parments>
	result_type result(const const_memory_block& memory,Parments&&... parments) const
	{
		serialize_object core;
		return _invoke_result_impl(core,
			std::is_void<result_type>::type(), memory, parments...);
	}

	template<class Func,class... Parments>
	result_type operator()(Func real, Parments&&... parments)
	{
		return invoke_result(invoke(real, parments...), parments...);
	}
private:
	template<class... Parments>
	result_type _invoke_result_impl(serialize_object& core,std::true_type,
		const const_memory_block& memory, Parments&&... parments) const
	{
		core.rarchive_real(memory, type_list<Arg...>(), parments...);
	}

	template<class... Parments>
	result_type _invoke_result_impl(serialize_object& core,std::false_type,
		const const_memory_block& memory, Parments&&... parments) const
	{
		result_type result;
		core.rarchive_real(memory, type_list<result_type&, Arg...>(),
			result, parments...);
		return result;
	}
};

template<class SerializeObject,class IdentType,class R,class... Arg>
imouto_client_stub<IdentType,SerializeObject,R,Arg...>
	basic_make_client_stub(const IdentType& ident, R(*fn)(Arg...))
{
	return imouto_client_stub<IdentType, SerializeObject, R, Arg...>(ident);
}
#endif