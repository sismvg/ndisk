#ifndef IMOUTO_RPC_EXCEPTION_HPP
#define IMOUTO_RPC_EXCEPTION_HPP

#include <stdexcept>
#include <xstddef>

class rpc_runtime_error
	:public std::runtime_error
{
public:
	typedef std::size_t size_type;
	typedef std::runtime_error father_type;
	rpc_runtime_error(const char* msg)
		:father_type(msg)
	{}

	//����ֱ�ӷ���
	XDR_DEFINE_TYPE_1(
		rpc_runtime_error,
		xdr_uint32,stat)
};

class rpc_timeout
	:public rpc_runtime_error
{
public:
	typedef std::size_t size_type;
	rpc_timeout(size_type ms)
		:rpc_runtime_error("rpc call timeout"),_ms(ms)
	{

	}

	inline size_type time() const
	{
		return _ms;
	}
private:
	size_type _ms;
};
/*
C����쳣�ṹ
*/
class c_style_runtime_error
	:public rpc_runtime_error
{
	/*
	�񱾵ز���ϵͳһ����raise����쳣�ṹ
	*/
public:
	typedef rpc_runtime_error father_type;
	c_style_runtime_error(const EXCEPINFO* info=nullptr)
		:father_type("remote call has c style error")
	{
		if (info)
			_info = *info;
	}

	const EXCEPINFO& info() const;
private:
	XDR_DEFINE_TYPE_1(
		c_style_runtime_error,
		EXCEPINFO,_info)
};

/*
C++����쳣�ṹ,����C++�쳣ϵͳ������̫��
û��������.������ֻ�Ǹ�tag
*/
class cpp_style_runtime_error
	:public rpc_runtime_error
{
public:
	cpp_style_runtime_error()
		:rpc_runtime_error("remote call has cpp excpetion")
	{}

	cpp_style_runtime_error(const std::string& str)
		:rpc_runtime_error("remote call has cpp excpetion"), _msg(str)
	{}

	/*
		�����쳣������Զ��,����std::runtime_error�Ĺ����޷�֧��
	*/
	const xdr_string& msg()
	{
		return _msg;
	}
private:
	XDR_DEFINE_TYPE_1(
		cpp_style_runtime_error,
		xdr_string,_msg)
};
#endif
