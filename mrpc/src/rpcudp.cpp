#include <rpcudp.hpp>
#include <iostream>
#include <random>

#include <boost/lexical_cast.hpp>
std::random_device dev;
std::mt19937 mt(dev());
std::uniform_int<> from(0, 100);

#define MYCALLBACK(name) [&](timer_handle id,sysptr_t ptr)\
	{\
		name(id,ptr);\
	}

#define RPCUDP_INIT :_udpmode(0),_group_id_allocer(0),\
	_window(_get_window_size()),\
	_recved_init(50000),_timer(MYCALLBACK(_check_ack)),\
	_ack_timer(MYCALLBACK(_timed_clear_ack)),_udpsock(-1)

rpcudp::rpcudp()
RPCUDP_INIT
{
	_init();
}

rpcudp::rpcudp(socket_type sock, size_t mode)
RPCUDP_INIT
{
	_udpmode = mode;
	_udpsock = sock;
	_init();
}

rpcudp::~rpcudp()
{
	_timed_clear_ack(rpctimer::timer_handle(), nullptr);
	closesocket(_udpsock);
}

void rpcudp::wait_all_ack()
{
	std::cout << _recved_init._count << std::endl;
	_recved_init.wait();
}

memory_block rpcudp::recvfrom(int flag, sockaddr_in& addr,
	int& addrlen, mtime_t timeout)
{
	size_t bufsize = _get_mtu();
	auto ptr = _alloc_memory(bufsize);
	memory_block recvbuf(ptr.get(), bufsize);

	int realflag = flag&WHEN_RECV_ACK_BREAK ? 0 : flag;
	int code = 0;
	while (true)
	{
		code = _recvfrom_impl(_udpsock, recvbuf, realflag, addr, addrlen);
		if (code == SOCKET_ERROR)
		{
			mylog.log(log_error,
				"rpcudp recvfrom have socket error code:", code);
			break;
		}
		auto package_ret = _package_process(addr, addrlen,
			const_memory_block(recvbuf.buffer, code));
		if (flag&WHEN_RECV_ACK_BREAK)
		{
			if (package_ret.accepted_ack)
				return _nop_block;
		}
		else
		{
			auto& block = package_ret.data;
			if (block.buffer != nullptr)
				return block;
		}
		continue;
	}
	return memory_block(nullptr, code);
}

int rpcudp::recvfrom(memory_block blk, int flag,
	sockaddr_in& addr, mtime_t timeout)
{
	int len = sizeof(addr);
	int ret = 0;
	while (true)
	{
		ret = _recvfrom_impl(_udpsock, blk, flag, addr, len);
		if (ret != SOCKET_ERROR)
		{
			if (!_filter_ack(addr, blk))
				break;
		}
	}
	return ret;
}

int rpcudp::recvfrom(void* buf, size_t bufsize,
	int flag, sockaddr_in& addr, int& addrlen, mtime_t timeout)
{
	memory_block block(buf, bufsize);
	return recvfrom(block, flag, addr, timeout);
}

void rpcudp::recvack()
{
	//TODO:实现
}


int rpcudp::sendto(const_memory_block blk, int flag, const sockaddr_in& addr)
{
	return sendto(blk.buffer, blk.size, flag, addr, sizeof(addr));
}

int rpcudp::sendto(const void* buf, size_t bufsize,
	int flag, const udpaddr& addr, int addrlen)
{
//	mylog.log(log_debug, "send to");
	//auto vec = _reqack(addr);
	std::vector<packid_type> vec;
	const_memory_block myblk(buf, bufsize);
	//int crc = crc16l(reinterpret_cast<const char*>(buf), bufsize);
	size_t v = 0;
	if (!vec.empty())
	{
		vector_air ph;
		auto blk = archive(ph,vec.size(),vec.begin());

		char* tmp = nullptr;
		char* mybuf = new char[blk.size + bufsize];
		tmp = mybuf;
		size_t size = blk.size + bufsize;
		memcpy(mybuf, blk.buffer, blk.size);
		
		incerment<1>(mybuf, blk.size);
		memcpy(mybuf, buf, bufsize);
		myblk = const_memory_block(tmp, size);
	}
//	mylog.log(log_debug, "slice");
	auto slices = _slice(myblk, vec.size(), 0);
	/*size_t start = 0;
	char* newbuf = new char[bufsize];
	char* tmp = newbuf;
	std::for_each(slices.begin(), slices.end(), [&](
		const slice_vector::slice& sli)
	{
		slice_head head;
		auto blk = sli.blk;
		advance_in(blk, rarchive(blk.buffer, blk.size, head));
		memcpy(
			reinterpret_cast<char*>(tmp) +head.start,
			blk.buffer, head.size);
	});
	int mycrc = crc16l(reinterpret_cast<const char*>(newbuf), bufsize);
	if (mycrc !=crc)
	{
		int val = 10;
		++val;
		mylog.log(log_debug, "crc errro");
	}*/
	int ret = 0;
	//mylog.log(log_debug, "send");
	_to_send(flag, slices.group_id, addr, slices.begin(), slices.end());
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

//
std::shared_ptr<rpcudp::bit> rpcudp::_alloc_memory(size_t size)
{
	std::shared_ptr<rpcudp::bit> ret(
		reinterpret_cast<char*>(_alloc_memory_raw(size)));
	return ret;
}

std::vector<packid_type> 
rpcudp::_reqack_impl(rpcudp::acks& list, size_t max)
{
	std::vector<packid_type> ret;
	auto copyed = isu::copy_limit(list.begin(), list.end(),
		max, std::back_inserter(ret));
	list.erase(list.begin(), copyed);
	return ret;
}

std::vector<packid_type> 
	rpcudp::_reqack_impl(const udpaddr& addr, size_t max)
{
	auto iter = _ack_list.find(addr);
	if (iter != _ack_list.end())
	{
		return _reqack_impl(iter->second, max);
	}
	return std::vector<packid_type>();//um..
}

std::vector<packid_type> rpcudp::_reqack(const udpaddr& addr)
{//保证长度不会超过一个mtu-head
	_acklock.lock();
	auto ret = _reqack_impl(addr, _get_max_ackpacks());
	_acklock.unlock();
	return ret;
}

size_t rpcudp::_get_mtu()
{
	return 1500;
}

size_t rpcudp::_alloc_group_id()
{
	return ++_group_id_allocer;
}

memory_address rpcudp::_alloc_memory_raw(size_t size)
{
	char* ret = new char[size];
	return ret;
}

void rpcudp::_check_ack(timer_handle handle, sysptr_t arg)
{
	resend_argument* parment = reinterpret_cast<resend_argument*>(arg);
	assert(parment);
	//TODO:thread safe
	_ack_waitlist_lock.lock();
	auto& map = _ack_waitlist[parment->addr];
	auto iter = map.find(parment->packid);
	if (iter == map.end()||parment->retry_limit--==0)
	{//已经接受过这个ACK了
		_ack_waitlist_lock.unlock();
		_timer.cancel_timer_when_exit(handle);
		_release_resend_argument(parment);
		return;
	}
	else
	{
		_sendto_impl(_udpsock, parment->data, 0,
			parment->addr, sizeof(parment->addr));
	}
	_ack_waitlist_lock.unlock();
}

size_t rpcudp::_dynamic_timeout()
{
	return 10;
}

void rpcudp::_recvack(const udpaddr* addr)
{
	udpaddr myaddr;
	int len = sizeof(myaddr);
	while (true)
	{
		recvfrom(WHEN_RECV_ACK_BREAK, myaddr, len);
		if (addr&& (*addr)==myaddr)
		{
			break;
		}
		else if (!addr)
		{
			break;
		}
	}
}

size_t rpcudp::_wait_ack(const sockaddr_in& addr, packid_type id)
{
	size_t ret = 0;
	auto addrtmp = addr;
	int len = sizeof(addrtmp);

	_ack_waitlist_lock.lock();
	auto iter = _ack_waitlist.find(addr);
	if (iter != _ack_waitlist.end())
	{
		_ack_waitlist_lock.unlock();
		_recvack(&addr);
	}
	else
		_ack_waitlist_lock.unlock();
	return (std::min)(ret, _get_window_size());
}

size_t rpcudp::_get_window_size(const sockaddr_in& addr)
{
	return _get_window_size();
}

size_t rpcudp::_get_window_size()
{
	return 50000;
}

slice_vector rpcudp::_slice(const_memory_block blk, size_t keep,int crc)
{
	//TODO:代码太长，需要优化
	//简单流程:
	//1:计算需要分出多少片和塞入分片头后的总长度
	//2:复制头和原始数据到分片中
	//3:放入slice_vector
	slice_vector ret;
	
	ret.group_id = _alloc_group_id();
	ret.accepted = 0;

	//开始分片
	size_t mtu = _get_mtu();
	size_t slice_head_size = archived_size<slice_head>();

	assert(mtu > slice_head_size);
	size_t slice_count =
		static_cast<size_t>(std::ceil(static_cast<double>(blk.size) /
		(mtu - slice_head_size)));//MSG:i hate warning

	size_t size_with_head = blk.size + slice_head_size*slice_count;
	memory_address ptr = _alloc_memory_raw(size_with_head);
	memory_block memory_with_head(ptr, size_with_head);

	const char* dataptr = reinterpret_cast<const char*>(blk.buffer);
	size_t less_size = size_with_head;
	size_t id = 0;

	size_t total_slice = slice_count;
	while (slice_count--)
	{
		slice_head head;
		size_t slice_size = (mtu > less_size ? less_size : mtu);
		memory_block slice_blk(ptr, slice_size);

		size_t data_size = slice_size - slice_head_size;
		//初始化头
		head.slice_type = IS_MAIN_PACK;
		if (keep != 0&&slice_count==(total_slice-1))
			head.slice_type |= (IS_ACK_PACK | UDP_ACK_MULTI);
		head.packsize = size_with_head;
		head.slice_count = total_slice;
		head.packid = packid_type(ret.group_id, id++);
		head.crc = crc;
		//head.slice_crc = crc16l(dataptr, data_size);
		head.size = data_size;
		head.start = dataptr - blk.buffer;
		//
		incerment<1>(ptr, archive_to(ptr, slice_size, head));
		memcpy(ptr, dataptr, data_size);
		incerment<1>(ptr, data_size);
		incerment<1>(dataptr, data_size);
		//
		slice_vector::slice slice;
		slice.slice_id = head.packid._slice_id;
		slice.blk = slice_blk;

		ret.slices.push_back(slice);
		less_size -= slice_size;
	}

	return ret;
}

size_t rpcudp::_filter_ack(const udpaddr& addr, memory_block& blk)
{
	size_t ret = 0;
	slice_head head;
	advance_in(blk, rarchive_from(blk.buffer, blk.size, head));
	if (head.slice_type&IS_ACK_PACK)
	{//TODO:_accept_ack需要返回一个size_t来表示ACK头用了多少字节
		const_memory_block tmp = blk;
		ret = _accept_ack(head, addr, tmp);
		blk.buffer = const_cast<void*>(tmp.buffer);
		blk.size = tmp.size;
	}
	return ret;
}

rpcudp::package_process_ret 
	rpcudp::_package_process(
		const sockaddr_in& addr, int len, const_memory_block blk)
{
	slice_head head;
	advance_in(blk.buffer, blk.size,
		rarchive_from(blk.buffer, blk.size, head));
	size_t accept_ack = 0;
	package_process_ret ret;
	memory_block test(_alloc_memory_raw(4), 4);
	if (head.slice_type&IS_ACK_PACK)
	{
		accept_ack = _accept_ack(head, addr, blk);
	}
	if (head.slice_type&IS_MAIN_PACK)
	{
		_recved_lock.lock();

		auto recved_iter = _recved.find(addr);
		if (recved_iter == _recved.end())
		{
			auto pair = _recved.insert(std::make_pair(addr, 
				std::map < packid_type, recved_record >()));
			recved_iter = pair.first;
		}
		auto& map = recved_iter->second;

		auto iter = map.find(head.packid._group_id);

		if (iter == map.end()||!iter->second.is_fin())
		{//没有收到过完整的包.
			const_memory_block rawdata = blk;
			auto iter = _write_to(head, addr, rawdata);
			
			auto& record = iter->second;
			if (record.accepted() == head.slice_count)
			{
				ret.data = record.set_fin();
			/*	int crc = crc16l(reinterpret_cast<char*>(record.data().buffer)
					, record.data().size);
				if (crc != head.crc)
				{
					mylog.log(log_error, "crc not match");
				}*/
			}
		}
		_recved_lock.unlock();
		_send_ack(head, addr);
	}
	ret.accepted_ack = accept_ack;
	return ret;
}

void rpcudp::_release_resend_argument(resend_argument* ptr)
{
	static std::hash_set<resend_argument*> myset;
	auto iter = myset.find(ptr);
	if (iter != myset.end())
	{
		int val = 10;
	}
	else
	{
		myset.insert(ptr);
		delete ptr;
	}
}

rpcudp::hash_map_iter rpcudp::_write_to(
	const slice_head& head, const udpaddr& addr, const_memory_block blk)
{//blk.size不能包含slice_head
	auto& map = _recved[addr];
	auto iter = map.find(head.packid._group_id);
	if (iter == map.end())
	{
		size_t size = head.packsize -
			head.slice_count*archived_size<slice_head>();

		recved_record record(
			memory_block(_alloc_memory_raw(size), size), false);

		auto pair = map.insert(std::make_pair(head.packid._group_id, record));
		record.set_fin();
		if (!pair.second)
		{
			mylog.log(log_error,
				"function:write_to can not insert packid:", head.packid);
			throw std::runtime_error("");
		}
		iter = pair.first;
	}
	//mylog.log(log_debug, "ok");
	/*memory_block buf = _get_data_range(head, blk.size,
		iter->second.data());
	if (buf.size > blk.size)//并不一定每个分片的值都对
	{
		mylog.log(log_error,
			"write out of range argument:", buf.size, blk.size);
		throw std::out_of_range("get data range error");
	}*/
	if (!iter->second.is_accepted(head.packid._slice_id))
	{
		/*
		int crc = crc16l(reinterpret_cast<const char*>(blk.buffer), blk.size);
		if (head.slice_crc != crc)
		{
			mylog.log(log_error, "slice crc error", head.packid._slice_id);
		}*/
		memcpy(
			reinterpret_cast<char*>(iter->second.data().buffer) + head.start,
			blk.buffer, head.size);
		//memcpy(buf.buffer, blk.buffer, buf.size);
		iter->second.imaccept();
		iter->second.set_accpeted(head.packid._slice_id);
	}
	return iter;
}

memory_block rpcudp::_get_data_range(const slice_head& head,
	size_t max_size, memory_block from)
{
//	size_t data_size = _get_mtu() - archived_size<slice_head>();
	size_t curpos = head.start;// head.packid._slice_id*data_size;
	size_t mysize = head.size;
/*	if (head.packid._slice_id == head.slice_count - 1)//最后一个包要做点手脚
	{
		mysize = from.size - curpos;
	}
	else
	{
		mysize = data_size;
	}*/
	if (head.packid._slice_id == head.slice_count - 1)
	{
		int val = 10;
	}
	auto* ptr = from.buffer;
	incerment<1>(ptr, curpos);
	return memory_block(ptr, mysize);
}

void rpcudp::_timed_clear_ack(timer_handle handle, sysptr_t ptr)
{
	//清理所有ack
	_acklock.lock();
	size_t max = _get_max_ackpacks();
	std::for_each(_ack_list.begin(), _ack_list.end()
		, [&](std::pair<const udpaddr, rpcudp::acks>& pair)
	{
		auto acks = _reqack_impl(pair.second, max);
		slice_head myhead;
		myhead.packid = packid_type(0, 0);
		myhead.slice_type = IS_ACK_PACK | UDP_ACK_MULTI;
		if (!acks.empty())
		{
			vector_air ph;
			auto blk = archive(myhead, ph, acks.size(), acks.begin());
			_sendto_impl(_udpsock, blk, 0, pair.first, sizeof(pair.first));
		}
	//mylog.log(log_debug, "ack list size:", pair.second.size());
	});
	//mylog.log(log_debug, "ack list:", _ack_list.size());
	_acklock.unlock();
}

void rpcudp::_send_ack(slice_head head, const sockaddr_in& addr)
{
	if (!_udpmode&DISABLE_ACK_NAGLE)
	{
		head.slice_type = IS_ACK_PACK | UDP_ACK_SINGAL;
		auto blk = archive(head);
		_sendto_impl(_udpsock, blk, 0, addr, sizeof(addr));
	}
	else
	{
		_acklock.lock();
		auto iter = _ack_list.find(addr);
		if (iter == _ack_list.end())
		{
			auto pair = _ack_list.insert(std::make_pair(addr, acks()));
			iter = pair.first;
		}
		auto& list = iter->second;
		list.push_back(head.packid);
		if (list.size() >= 8)//8是最大ack缓存
		{
			_acklock.unlock();
			_timed_clear_ack(rpctimer::timer_handle(), nullptr);
		}
		else
			_acklock.unlock();
	}
}

size_t rpcudp::_accept_ack_impl(const udpaddr& addr, packid_type id)
{
	_ack_waitlist_lock.lock();
	auto iter = _ack_waitlist.find(addr);
	if (iter != _ack_waitlist.end())
	{
		auto& map = iter->second;
		_window.post();
		map.erase(id);
		_recved_init.fin();//debug用
		_ack_waitlist_lock.unlock();
		return 1;
	}
	_ack_waitlist_lock.unlock();
	return 0;
}

rpcudp::resend_argument* rpcudp::_make_resend_argment(size_t group_id,
	const udpaddr& addr, const slice_vector::slice& sli)
{
	resend_argument* ptr = new resend_argument;
	ptr->addr = addr;
	ptr->data = sli.blk;
	ptr->retry_limit = 13;
	ptr->packid = packid_type(group_id, sli.slice_id);
	return ptr;
}

void rpcudp::_to_send(int flag,size_t group_id, const sockaddr_in& addr,
	slice_iterator begin, slice_iterator end)
{
	assert(this);
	int real_flag = 0;
	auto iter = _ack_waitlist.find(addr);
	if (iter == _ack_waitlist.end())
	{
		auto pair = _ack_waitlist.insert(
			std::make_pair(addr, std::hash_set<packid_type>()));
		iter = pair.first;
	}
	auto& map = iter->second;
//	mylog.log(log_debug, "to send");
	for (auto mybegin = begin; mybegin != end;++mybegin)
	{
		const auto& sli = *mybegin;
		resend_argument* arg =
			_make_resend_argment(group_id, addr, sli);

		_ack_waitlist_lock.lock();
		map.insert(arg->packid);
		_ack_waitlist_lock.unlock();
		if (!_window.try_wait())
		{
			if (flag&RECV_ACK_BY_UDP)
				_wait_ack(addr, arg->packid);
			_window.wait();
		}
		arg->handle = _timer.set_timer(arg, _dynamic_timeout(), 0);
		_sendto_impl(_udpsock, arg->data, 0,
			arg->addr, sizeof(arg->addr));
	}
	//assert(this);
	if (flag&RECV_ACK_BY_UDP)
	{
		std::for_each(begin, end, [&](const slice_vector::slice& sli)
		{
			_wait_ack(addr, packid_type(group_id, sli.slice_id));
		});
	}
}

size_t rpcudp::_accept_ack(slice_head& head,
	const udpaddr& addr, const_memory_block& blk)
{
	size_t ack_count = 0;
	size_t acktype = head.slice_type;

	struct ack_compressed
	{
		size_t start, last; //[start, last], [begin,end) warning
	};
	//不用上锁
	if (acktype&UDP_ACK_COMMPRESSED)
	{
		ack_compressed comack;
		advance_in(blk, rarchive_from(blk.buffer, blk.size, comack));
		for (; comack.start <= comack.last; ++comack.start)
		{
			ack_count += _accept_ack_impl(addr,
				packid_type(head.packid._group_id, comack.start));
		}
	}
	if (acktype&UDP_ACK_SINGAL)
	{
		ack_count = _accept_ack_impl(addr, head.packid);
	}
	else if (acktype&UDP_ACK_MULTI)
	{
		vector_air ph;
		std::vector<packid_type> ackids;
		size_t size = 0;
		size_t adv = rarchive(blk.buffer,
			blk.size, ph, size, std::back_inserter(ackids));
		head.packsize -= adv;//WARNING:由于是impl的包，
					//所以要调整一下block的大小
		if (adv >100)
		{
			std::cout << "wtf";
		}

		advance_in(blk, adv);
		for (auto ackid : ackids)
			ack_count += _accept_ack_impl(addr, ackid);
	}
	return ack_count;
}

int rpcudp::_sendto_impl(socket_type sock, const_memory_block blk,
	int flag, const sockaddr_in& addr, int addrlen)
{
	int ret = ::sendto(sock,
		reinterpret_cast<const char*>(blk.buffer), blk.size,
		flag, reinterpret_cast<const sockaddr*>(&addr), addrlen);
	if (ret == SOCKET_ERROR)
	{
		mylog.log(log_error,
			"sendto_impl have socket_error code:", WSAGetLastError());
	}
	return ret;
}

int rpcudp::_recvfrom_impl(socket_type sock,
	memory_block blk, int flag, sockaddr_in& addr, int& addrlen)
{
	int ret = ::recvfrom(sock,
		reinterpret_cast<char*>(blk.buffer), blk.size,
		flag, reinterpret_cast<sockaddr*>(&addr), &addrlen);
	if (ret == SOCKET_ERROR)
	{
		std::cout << WSAGetLastError() << std::endl;
		mylog.log(log_error,
			"recvfrom_impl have socket_error code:",WSAGetLastError());
	}
	return ret;
}

void rpcudp::_init()
{
	if (!(_udpmode&DISABLE_ACK_NAGLE))
	{
		_ack_timer.set_timer(nullptr, 15);
	}
}

const const_memory_block rpcudp::_const_nop_block(nullptr, 0);

const memory_block rpcudp::_nop_block(nullptr, 0);

size_t rpcudp::_get_max_ackpacks()
{
	size_t keep = archived_size<slice_head>() - sizeof(size_t);
	return (_get_mtu() - keep) / sizeof(size_t);
}

//slice_head
slice_vector::slice_vector()
{

}

slice_vector::slice_vector(const_memory_block block)
	:memorys(block)
{

}

size_t slice_vector::size() const
{
	return slices.size();
}

slice_vector::iterator slice_vector::begin()
{
	return slices.begin();
}

slice_vector::iterator slice_vector::end()
{
	return slices.end();
}