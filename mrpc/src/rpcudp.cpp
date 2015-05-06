
#include <rpcudp.hpp>
#include <iostream>
#include <random>

std::random_device dev;
std::mt19937 mt(dev());
std::uniform_int<> from(0, 100);

rpcudp::mybuf::mybuf()
	:buffer(nullptr), size(0)
{
}

rpcudp::mybuf::mybuf(const void* buf, size_t sz)
	: buffer(buf), size(sz)
{
}

#define MYCALLBACK(name) [&](timer_handle id,sysptr_t ptr)\
	{\
		name(id,ptr);\
	}

rpcudp::rpcudp()
	: _udpsock(-1), _timer(MYCALLBACK(_ack_callback))
{}

rpcudp::rpcudp(socket_type sock)
	: _udpsock(sock), _timer(MYCALLBACK(_ack_callback))
{}

rpcudp::~rpcudp()
{
	closesocket(_udpsock);
}

int rpcudp::recvfrom(void* buf, size_t bufsize,
	int flag, sockaddr_in& addr, int& addrlen, mtime_t timeout /* = default_timeout */)
{
	//必须保证对方使用rpcudp发送过来的包
	_1:
	char* mybuf = new char[bufsize];
	int ret = _recvfrom_impl(_udpsock, mybuf, bufsize,
		flag, addr, addrlen);

	packet_head head;
	auto data = split_pack(mybuf, ret, head);
	using namespace std;
	if (head.pack_type == packet_head::pack_ack)
	{
		//cout << "受到包:" << head.ack_id << endl;
		_accept_ack(head);
		delete mybuf;
		goto _1;
	}
	else if (head.pack_type == packet_head::main_pack)
	{
		//处理主包
		//返回个ack
		if (!from(mt))
		{
		//	cout << "随机丢弃包序号：" << head.ack_id << endl;
		//	goto _1;
		}
		head.pack_type = packet_head::pack_ack;
		_send_ack(head, addr, addrlen);

	}
	else if (head.pack_type == packet_head::ack_requery)
	{
		//表示ack丢包了.
		//WARNING:ID回绕怎么办
		auto rlock = ISU_AUTO_RLOCK(_lock);
		auto iter = _packets.find(head.ack_id);
		if (iter == _packets.end())
		{
			//表明已经收到了主包，但是返回的ack丢了
		//	cout << "重发ack包:" << head.ack_id << endl;
			head.pack_type = packet_head::pack_ack;
			_send_ack(head, addr, addrlen);
			delete mybuf;
			goto _1;
		}
		//回绕的话，也许可以给head加上time_t来判断包的创建时间
		//time_t回绕要49天，所以不用担心
	}
	else if (head.pack_type == packet_head::main_pack_and_ack)
	{
		//有点麻烦
	}

	//ack包:包标识,ack id
	//主包:包标识,ack id,数据
	//主包捎带另一个主包的ack,包标识,ack id,主包id,数据

	memcpy(buf, data.buffer, data.size);
	delete mybuf;
	return data.size;
}

void rpcudp::_waitlist_of_ack(rpcid id, buftype buf, const sockaddr_in& addr)
{
	mybuf arch(buf.first, buf.second);
	arch.addr = addr;
	auto wlock = ISU_AUTO_WLOCK(_lock);
	auto* ptr = new rpcid(id);
	arch.timer_id = _timer.set_timer(ptr, 30, 10);
	_packets.insert(std::make_pair(id, arch));
}

int rpcudp::sendto(rpcid id, const void* buf, size_t bufsize,
	int flag, const sockaddr_in& addr,
	int addrlen, mtime_t timeout /* = default_timeout */)
{//这个函数只能用来发送主包
	auto pair = _make_buf(id, buf, bufsize);
	_waitlist_of_ack(id, pair, addr);

	int ret = _sendto_impl(_udpsock, pair.first, 
		pair.second, flag, addr, addrlen);
	return ret;
}

socket_type rpcudp::socket() const
{
	return _udpsock;
}

void rpcudp::setsocket(socket_type sock)
{
	if (_udpsock != SOCKET_ERROR)
		closesocket(_udpsock);
	_udpsock = sock;
}

//impl

void rpcudp::_resend_packet(rpcid id, mybuf& buf)
{
	_sendto_impl(_udpsock, buf.buffer,
		buf.size, 0, buf.addr, sizeof(buf.addr));
}

void rpcudp::_ack_callback(timer_handle timer_id, sysptr_t arg)
{
	using namespace std;
	rpcid* id = reinterpret_cast<rpcid*>(arg);
	auto iter = _packets.find(*id);
	if (iter != _packets.end())
	{
		_resend_packet(*id, iter->second);
		//cout << "重发包序号:" << *id << endl;
		return;
	}
	//cout << "do nothing" << endl;
}

int rpcudp::_sendto_impl(socket_type sock, 
	const void* buf, size_t size,
	int flag,const sockaddr_in& addr, int addrlen)
{
	int ret = ::sendto(sock, 
		reinterpret_cast<const char*>(buf), size,
		flag, reinterpret_cast<const sockaddr*>(&addr), addrlen);
	return ret;
}

int rpcudp::_recvfrom_impl(socket_type sock,
	void* buf, size_t size, 
	int flag, sockaddr_in& addr, int& addrlen)
{
	int ret = ::recvfrom(sock,
		reinterpret_cast<char*>(buf), size,
		flag, reinterpret_cast<sockaddr*>(&addr), &addrlen);
	return ret;
}

void rpcudp::_send_ack(const packet_head& head,
	const sockaddr_in& addr,int addrlen)
{
	auto pair = archive(head);
	_sendto_impl(_udpsock, pair.first,
		pair.second, 0, addr, addrlen);
}

void rpcudp::_accept_ack(const packet_head& head)
{
	auto wlock = ISU_AUTO_WLOCK(_lock);
	auto iter = _packets.find(head.ack_id);

	if (iter == _packets.end())
		return;//WARNING:也许要做点处理

	auto pair = iter->second;
	delete pair.buffer;
	_timer.cancel_timer(pair.timer_id);
	_packets.erase(iter);
}

rpcudp::buftype rpcudp::_make_buf(rpcid id, 
	const void* buf, size_t size,size_t pack_type)
{
	size_t newbuf_size = sizeof(packet_head) + size;
	char* newbuf = new char[newbuf_size];
	packet_head head;
	head.ack_id = id;
	head.pack_type = pack_type;
	archive_to(newbuf, sizeof(packet_head), head);
	memcpy(newbuf + sizeof(packet_head), buf, size);
	return std::make_pair(newbuf, newbuf_size);
}