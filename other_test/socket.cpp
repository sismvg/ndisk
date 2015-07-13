
#include <socket.hpp>

core_sockaddr::core_sockaddr()
{
	_init(0, 0, 0);
}

core_sockaddr::core_sockaddr(family_type family, port_type port,
	const binary_address& address)
{
	_init(family, port, address);
}

core_sockaddr::core_sockaddr(const sockaddr_in& addr)
{
	_address = addr;
}

core_sockaddr::operator const sockaddr_in&() const
{
	return _address;
}

sockaddr* core_sockaddr::socket_addr()
{
	return reinterpret_cast<sockaddr*>(&_address);
}

const sockaddr* core_sockaddr::socket_addr() const
{
	return const_cast<self_type*>(this)->socket_addr();
}

int core_sockaddr::address_size() const
{
	return sizeof(sockaddr_in);
}

core_sockaddr::family_type core_sockaddr::family() const
{
	return const_cast<self_type*>(this)->family();
}

core_sockaddr::family_type& core_sockaddr::family()
{
	return _address.sin_family;
}

core_sockaddr::port_type core_sockaddr::port() const
{
	return const_cast<self_type*>(this)->port();
}

core_sockaddr::port_type& core_sockaddr::port()
{
	return _address.sin_port;
}

core_sockaddr::binary_address core_sockaddr::address() const
{
	return const_cast<self_type*>(this)->port();
}

core_sockaddr::binary_address& core_sockaddr::address()
{
	return _address.sin_addr.S_un.S_addr;
}

void core_sockaddr::_init(family_type family,
	port_type port, binary_address addr)
{
	_address.sin_family = family;
	_address.sin_port = htons(port);
	_address.sin_addr.S_un.S_addr = addr;
}

core_socket::core_socket()
	:_socket(invailed_socket)
{
}

core_socket::core_socket(family_type family, 
	socket_type type, protocol_type protocol)
{
	_init(family, type, protocol);
}

core_socket::core_socket(socket_type type, 
	protocol_type protocol, const core_sockaddr& addr)
{
	_init(addr.family(), type, protocol);
	this->bind(addr);
}

core_socket::system_socket core_socket::socket() const
{
	return _socket;
}

void core_socket::close()
{
	::closesocket(socket());
	_socket = SOCKET_ERROR;
}

core_socket::operation_result core_socket::bind(const core_sockaddr& addr)
{
	return ::bind(socket(), CORE_SOCKADDR_PAIR(addr));
}

const core_sockaddr* core_socket::bound() const
{
	return nullptr;
}

void core_socket::_init(family_type family, 
	socket_type type, protocol_type protocol)
{
	_socket = ::socket(family, type, protocol);
}