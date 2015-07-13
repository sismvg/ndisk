#ifndef ISU_IMOUTO_RPCSTUB_HPP
#define ISU_IMOUTO_RPCSTUB_HPP

//提供memory_block的抽象
#include <archive_def.hpp>
/*
	imouto-rpc的基础桩功能提供
	imouto-client-stub和<-server-<皆为其子类
*/

/*
	由两个模块组成
	对象序列化-函数对象签名
*/

/*
	将对象变成可传输的二进制
*/
template<class T>
memory_block to_binary_stream(const T&);

/*
	^反过来
*/
template<class T>
T from_binary_stream(const const_memory_block&);

//提供抽象
template<class IdentType>
class basic_stub
{
public:
	typedef IdentType ident_type;

	basic_stub(const ident_type& ident)
		:_ident(ident)
	{}

	const ident_type& ident() const throw()
	{
		return _ident;
	}
private:
	const ident_type _ident;
};

#endif