#ifndef ISU_SOCKET_HPP
#define ISU_SOCKET_HPP

#include <string>

#include <platform_config.hpp>

#ifdef _WINDOWS
#include <windows.h>

#else

#endif

#define CORE_SOCKADDR_PAIR(addr) addr.socket_addr(),addr.address_size()

class core_sockaddr
{
public:
	typedef core_sockaddr self_type;
	typedef short family_type;
	typedef unsigned short port_type;
	typedef unsigned long binary_address;

	core_sockaddr();
	/*
		����һ����ַ,��ַ��addrָ��
	*/
	core_sockaddr(family_type family, port_type port,
		const binary_address& address);

	/*
		����һ����ַ,��ַ��ip_strָ��
		��ʽ����Ϊn.n.n.n
	*/
	core_sockaddr(family_type family, port_type port,
		const std::string& ip_str);

	/*
		��addr������һ����ַ
	*/
	core_sockaddr(const sockaddr_in& addr);

	/*
		һЩת��
	*/
	operator const sockaddr_in&() const;
	/*
		���س���ĵ�ַ.���������tcp/ip�ں�
	*/
	sockaddr* socket_addr();
	const sockaddr* socket_addr() const;

	/*
		��ַ�ĳ���
	*/
	int address_size() const;

	/*
		������ַ��
	*/
	family_type family() const;
	family_type& family();

	/*
		�����˿�
	*/
	port_type port() const;
	port_type& port();

	/*
		�����Ƶ�ַ�ִ�
	*/
	binary_address address() const;
	binary_address& address();

private:
	void _init(family_type family, port_type port, binary_address addr);
	sockaddr_in _address;
};

const int invailed_socket = -1;

class core_socket
{
public:
	typedef int system_socket;
	typedef int operation_result;
	typedef int socket_type;
	typedef core_sockaddr::family_type family_type;
	typedef core_sockaddr::port_type port_type;
	typedef int protocol_type;

	/*
		�չ���,socketΪinvailed_socket
	*/
	core_socket();

	/*
		����һ��socket
		family:socket�ĵ�ַ��
		type:socket������
		protocol:Э��
	*/
	core_socket(family_type family,
		socket_type type, protocol_type protocol);

	/*
		����һ��socket,���Ұ󶨵�addr��
		��ַ�������addrָ��
	*/
	core_socket(socket_type type, protocol_type protocol,
		const core_sockaddr& addr);

	/*
		�����ں�socket
	*/
	system_socket socket() const;

	/*
		�ر�socket
	*/
	void close();

	/*
		�󶨵�ָ��socket��
	*/
	operation_result bind(const core_sockaddr& addr);

	/*
		���ذ󶨵ĵ�ַ -δʵ��
	*/
	const core_sockaddr* bound() const;
private:
	system_socket _socket;
	void _init(family_type family, 
		socket_type type, protocol_type protocol);
};
#endif
