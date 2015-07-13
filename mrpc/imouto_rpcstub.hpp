#ifndef ISU_IMOUTO_RPCSTUB_HPP
#define ISU_IMOUTO_RPCSTUB_HPP

//�ṩmemory_block�ĳ���
#include <archive_def.hpp>
/*
	imouto-rpc�Ļ���׮�����ṩ
	imouto-client-stub��<-server-<��Ϊ������
*/

/*
	������ģ�����
	�������л�-��������ǩ��
*/

/*
	�������ɿɴ���Ķ�����
*/
template<class T>
memory_block to_binary_stream(const T&);

/*
	^������
*/
template<class T>
T from_binary_stream(const const_memory_block&);

//�ṩ����
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