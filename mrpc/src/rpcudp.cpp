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

rpcudp::rpcudp()
	: _udpsock(-1), _timer(MYCALLBACK(_check_ack)),
	_sem(_get_window_size()), _group_id_allocer(0),
	_recved_lock(), _ack_timer(MYCALLBACK(_timed_clear_ack)),
	_recved_init(50000)
{
	_ack_timer.set_timer(nullptr, 10);
	_recved_init.ready_wait();
}

rpcudp::rpcudp(socket_type sock, udpmode mode)
	: _udpsock(sock), _timer(MYCALLBACK(_check_ack)), _mode(mode),
	_sem(_get_window_size()), _group_id_allocer(0),
	_ack_timer(MYCALLBACK(_timed_clear_ack)), _recved_init(50000)
{
	_ack_timer.set_timer(nullptr, 10);
	_recved_init.ready_wait();
}

rpcudp::~rpcudp()
{
//	mylog.log(log_debug, "rpcudp exit");
	_timed_clear_ack(0, nullptr);
}
void rpcudp::wait_all_ack()
{
	//当所有的包发送完毕，并且收到ack时推出
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
	while (true)
	{
	//	mylog.log(log_debug, "an");
	//	mylog.log(log_debug, "ready recv");
		int ret = _recvfrom_impl(_udpsock, recvbuf, realflag, addr, addrlen);
		if (ret == SOCKET_ERROR)
		{
		//	mylog.log(log_error, "recvfrom socket error");
			break;
		}
	//	mylog.log(log_debug, "recv prg");
	//	mylog.log(log_debug, "my process");
		auto pair =
			_package_process(addr, addrlen, const_memory_block(recvbuf.buffer, ret));
	//	mylog.log(log_debug, "package process");
		if (flag&WHEN_RECV_ACK_BREAK)
		{
	//		mylog.log(log_debug, "recv ret");
			if (pair.first)
				return _nop_block;
			continue;
		}
		else
		{
			auto blk = _try_chose_block();
			if (blk.buffer == nullptr)
				continue;
			return blk;
		}
	}
	return _nop_block;
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
			if (!_filter_ack(blk))
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
	int flag, const sockaddr_in& addr, int addrlen)
{
	auto vec = _reqack();
	//std::vector<packid_type> vec;
	if (!vec.empty())
	{
	//	vec.clear();
	}
	const_memory_block myblk(buf, bufsize);
	size_t v = 0;
	if (!vec.empty())
	{
	//	v = crc16l(reinterpret_cast<const char*>(buf), bufsize);
		vector_air ph;
		auto blk = archive(ph,vec.size(),vec.begin());
		std::vector<packid_type> vec2;
		
		char* tmp = nullptr;
		char* mybuf = new char[blk.size + bufsize];
		tmp = mybuf;
		size_t size = blk.size + bufsize;
		memcpy(mybuf, blk.buffer, blk.size);
		
		incerment<1>(mybuf, blk.size);
		memcpy(mybuf, buf, bufsize);
		myblk = const_memory_block(tmp, size);
	}
	/*
	if (vec.size())
	{
		auto blk = myblk;
		slice_head head;
	//	rarchive(blk.buffer, blk.size, head);
	//	advance_in(blk, 20);
		head.slice_type = IS_ACK_PACK | UDP_ACK_MULTI;
		_accept_ack(head, blk);
		int v2=crc16l(reinterpret_cast<const char*>(blk.buffer), blk.size);
		int v = 10;
	}*/
	auto slices = _slice(myblk, vec.size());
	int ret = 0;
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

std::vector<rpcudp::packid_type> rpcudp::_reqack()
{//保证长度不会超过一个mtu-head
	_acklock.lock();
	size_t liner = 0;
	std::vector<packid_type> acks;
	sockaddr_in addr;
	packid_type last = 0;
	size_t total_size = sizeof(size_t);
	size_t max_size = _get_mtu() - archived_size<slice_head>();
	auto begin = _ack_list.begin();
	for (; begin != _ack_list.end(); ++begin)
	{
		addr = begin->addr;
		const auto& head = begin->head;
		auto id = _makeid(head.group_id, head.slice_id);
		if (id != last)
		{
			if ((acks.size()*sizeof(packid_type)) >= max_size)
				break;
			acks.push_back(id);
		}
		last = id;
		//	auto blk = archive(head);
		//	_sendto_impl(_udpsock, blk, 0, begin->addr, sizeof(begin->addr));
	}
	_ack_list.erase(_ack_list.begin(), begin);
	_acklock.unlock();
	return acks;
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

memory_block rpcudp::_try_chose_block()
{
	//假设有完成却没有被删除的包则返回
	auto rlock = ISU_AUTO_LOCK(_findished_lock);
	auto iter = _finished.begin();
	memory_block ret = _nop_block;
	if (iter != _finished.end())
	{
		ret = iter->memory;
		_finished.erase(iter);
	}
	return ret;
}

void rpcudp::_check_ack(timer_handle handle, sysptr_t arg)
{
	resend_argument* parment = reinterpret_cast<resend_argument*>(arg);
	assert(parment);
	//TODO:thread safe
	_recved_lock.lock();
	auto iter = _recved.find(parment->id);
	if (iter == _recved.end())
	{//已经接受过这个ACK了
		//mylog.log(log_debug, "no id");
		_recved_lock.unlock();
		_timer.cancel_timer_when_exit(handle);
		return;
	}
	else
	{//没有则重传
		//	mylog.log(log_debug, "retry");
		if (parment->max_retry-- == 0)
		{
		//	mylog.log(log_debug, "eixt");
			_recved_lock.unlock();
			_timer.cancel_timer_when_exit(handle);
			//	mylog.log(log_debug, "exit retry");
			return;
		}
		//mylog.log(log_debug, "myretyrtosend");
		_sendto_impl(_udpsock, parment->block, 0,
			parment->addr, sizeof(parment->addr));
	}
	_recved_lock.unlock();
}

rpcudp::resend_argument*
rpcudp::_regist_slice(size_t group_id, const sockaddr_in& addr,
const slice_vector::slice& block)
{
	resend_argument* ptr = new resend_argument;
	ptr->block = block.blk;
	auto val = _makeid(group_id, block.slice_id);
	ptr->id = val;
	ptr->addr = addr;
	ptr->max_retry = 1000;
	return ptr;
}

size_t rpcudp::_dynamic_timeout()
{
	return 10;
}

size_t rpcudp::_dynamic_retry_count(const sendout_record& record)
{
	return 4;
}

size_t rpcudp::_wait_ack(const sockaddr_in& addr, packid_type id)
{
	size_t ret = 0;
	auto addrtmp = addr;
	int len = sizeof(addr);
	_recved_lock.lock();
	auto iter = _recved.find(id);
	if (iter != _recved.end())
	{
		_recved_lock.unlock();
	//	recvfrom(WHEN_RECV_ACK_BREAK, addrtmp, len);
		recvfrom(WHEN_RECV_ACK_BREAK, addrtmp, len);
		//MSG:只要收到ACK就表示又可以发数据了，所以当收到ACK会立即返回
	}
	else
		_recved_lock.unlock();
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

slice_vector rpcudp::_slice(const_memory_block blk, size_t keep)
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
	size_t slice_count = std::ceil(static_cast<double>(blk.size) /
		(mtu - slice_head_size));

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
		head.group_id = ret.group_id;
		head.slice_type = IS_MAIN_PACK;
		if (keep != 0)
			head.slice_type |= (IS_ACK_PACK | UDP_ACK_MULTI);
		head.total_size = size_with_head;
		head.total_slice = total_slice;
		head.slice_id = id++;
		//
		incerment<1>(ptr, archive_to(ptr, slice_size, head));
		memcpy(ptr, dataptr, data_size);
//		memcpy_s(ptr, data_size+1, dataptr, data_size);
		incerment<1>(ptr, data_size);
		incerment<1>(dataptr, data_size);
		//
		slice_vector::slice slice;
		slice.slice_id = head.slice_id;
		slice.blk = slice_blk;

		ret.slices.push_back(slice);
		less_size -= slice_size;
	}

	return ret;
}

bool rpcudp::_filter_ack(memory_block& blk)
{
	slice_head head;
	advance_in(blk, rarchive_from(blk.buffer, blk.size, head));
	if (head.slice_type&IS_ACK_PACK)
	{//TODO:_accept_ack需要返回一个size_t来表示ACK头用了多少字节
		const_memory_block tmp = blk;
		_accept_ack(head, tmp);
		blk.buffer = const_cast<void*>(tmp.buffer);
		blk.size = tmp.size;
		return true;
	}
	return false;
}

std::pair<bool, memory_block>
rpcudp::_package_process(const sockaddr_in& addr, int len, const_memory_block blk)
{
	//ack包和主包&主包withack
	//主包直接分拆头后写入数据

	memory_block ret = _nop_block;
	slice_head head;
	advance_in(blk.buffer, blk.size,
		rarchive_from(blk.buffer, blk.size, head));

	bool have_ack = false;

	if (head.slice_type&IS_ACK_PACK)
	{
		have_ack = _accept_ack(head, blk);
	}
	if (head.slice_type&IS_MAIN_PACK)
	{
		auto myid = _makeid(head.group_id, head.slice_id);
		auto wlock_recv = ISU_AUTO_LOCK(_recved_pack_lock);
		if (_recved_pack.find(myid) == _recved_pack.end())
		{//没有收到过完整的包.
			auto wlock = ISU_AUTO_LOCK(_recved_dgrm_lock);
			const_memory_block rawdata = blk;
			auto iter = _write_to(head, rawdata);
			_recved_pack.insert(myid);
			if (iter->second.accepted == head.total_slice)
			{
				auto wlock = ISU_AUTO_LOCK(_findished_lock);
				
				_finished.push_back(iter->second);
				_recved_dgrm.erase(iter);//不再需要这个包了
			}
		}
		else
		{
			int val = 10;
		}
		_send_ack(head, addr, len);
	}
	return std::make_pair(have_ack, ret);
}

rpcudp::hash_map_iter rpcudp::_write_to(
	const slice_head& head, const_memory_block blk)
{//blk.size不能包含slice_head
	size_t id = _makeid(head.group_id, head.slice_id);
	auto iter = _recved_dgrm.find(id);
	if (iter == _recved_dgrm.end())
	{
		//初始化
		recved_record record;
		record.accepted = 0;
		size_t size = head.total_size -
			head.total_slice*archived_size<slice_head>();
		record.memory.buffer = _alloc_memory_raw(size);
		record.memory.size = size;
		auto pair = _recved_dgrm.insert(std::make_pair(id, record));
		if (!pair.second)
		{
			throw std::runtime_error("");
		}
		else
		{
			iter = pair.first;
		}
	}

	memory_block buf = _get_range(head, blk.size, iter->second.memory);
	if (buf.size > blk.size)
	{
		int vbl = 0;
		++vbl;
	}
	memcpy(buf.buffer, blk.buffer, buf.size);

	++iter->second.accepted;
	return iter;
}

memory_block rpcudp::_get_range(const slice_head& head,
	size_t max_size, memory_block from)
{
	size_t data_size = _get_mtu() - archived_size<slice_head>();
	size_t curpos = head.slice_id*data_size;
	size_t mysize = 0;
	if (head.slice_id == head.total_slice - 1)//最后一个包要做点手脚
	{
		mysize = from.size - curpos;
	}
	else
	{
		mysize = data_size;
	}
	auto* ptr = from.buffer;
	incerment<1>(ptr, curpos);
	return memory_block(ptr, mysize);
}

void rpcudp::_timed_clear_ack(timer_handle handle, sysptr_t ptr)
{
	_acklock.lock();
	size_t liner = 0;
	std::vector<packid_type> acks;
	sockaddr_in addr;
	packid_type last = 0;

	for (auto begin = _ack_list.begin(); begin != _ack_list.end(); ++begin)
	{
		addr = begin->addr;
		const auto& head = begin->head;
		auto id= _makeid(head.group_id, head.slice_id);
		if (id != last)
			acks.push_back(id);
		last = id;
	}
	_ack_list.clear();
	_acklock.unlock();
	slice_head myhead;
	myhead.group_id = 0;
	myhead.slice_id = 0;
	myhead.slice_type = IS_ACK_PACK | UDP_ACK_MULTI;
	myhead.total_size = 0;
	myhead.total_slice = 0;
	if (!acks.empty())
	{
	//	mylog.log(log_debug, "send ack");
		vector_air ph;
		auto blk = archive(myhead, ph, acks.size(), acks.begin());
		_sendto_impl(_udpsock, blk,0, addr, sizeof(addr));
	}
	else
	{
	//	mylog.log(log_debug, "no anything");
	}
}
void rpcudp::_send_ack(slice_head head, const sockaddr_in& addr, int len)
{//这个模式下压包是不允许的
	head.slice_type = IS_ACK_PACK | UDP_ACK_SINGAL;
	_acklock.lock();
	store_ack ack;
	ack.head = head;
	ack.addr = addr;
//	auto blk = archive(head);
//	_sendto_impl(_udpsock, blk, 0, addr, sizeof(addr));
	_ack_list.push_back(ack);
	_acklock.unlock();
	if (_ack_list.size() >= 8)
		_timed_clear_ack(0, nullptr);
}

bool rpcudp::_accept_ack_impl(packid_type id)
{
	_recved_lock.lock();
	auto iter = _recved.find(id);
	if (iter != _recved.end())
	{
		_sem.post();
		_recved.erase(iter);
	//	_recved_init.fin();
		_recved_lock.unlock();
		return true;
	}
	_recved_lock.unlock();
	return false;
}
bool rpcudp::_accept_ack_impl(const slice_head& head, size_t slice_id)
{
	return _accept_ack_impl(_makeid(head.group_id, head.slice_id));

}

rpcudp::packid_type rpcudp::_makeid(size_t group_id, size_t slice_id)
{
	struct tmp
	{
		size_t lh;
		size_t rh;
	};
	tmp val;
	val.lh = group_id;
	val.rh = slice_id;
	return *reinterpret_cast<packid_type*>(&val);
}

void rpcudp::_to_send(int flag,size_t group_id, const sockaddr_in& addr,
	slice_iterator begin, slice_iterator end)
{
	int real_flag = 0;
	for (auto mybegin = begin; mybegin != end;++mybegin)
	{
		auto* buf = _regist_slice(group_id, addr, *mybegin);
		_recved_lock.lock();
		auto pair = _recved.insert(buf->id);
		_recved_lock.unlock();
		if (!_sem.try_wait())
		{
			if (flag&RECV_ACK_BY_UDP)
				_wait_ack(addr, buf->id);
			_sem.wait();
		}
		buf->timer_id = _timer.set_timer(buf, _dynamic_timeout(), 0);
		_sendto_impl(_udpsock, buf->block, 0,
			buf->addr, sizeof(buf->addr));
	}
	if (flag&RECV_ACK_BY_UDP)
	{
		for (; begin != end; ++begin)
		{
		//	mylog.log(log_debug, "a1");
			_wait_ack(addr, _makeid(group_id, begin->slice_id));
			//mylog.log(log_debug, "a2");
		}
	}
}

bool rpcudp::_accept_ack(const slice_head& head, const_memory_block& blk)
{
	//1个字节的头
	bool ack_fin = false;
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
			ack_fin = _accept_ack_impl(head, comack.start);
		}
	}
	if (acktype&UDP_ACK_SINGAL)
	{
		ack_fin = _accept_ack_impl(head, head.slice_id);
	}
	else if (acktype&UDP_ACK_MULTI)
	{
		vector_air ph;
		std::vector<packid_type> ackids;
		size_t size = 0;
		size_t adv = rarchive(blk.buffer,
			blk.size, ph, size, std::back_inserter(ackids));
		advance_in(blk, adv);
		for (auto ackid : ackids)
			if (!ack_fin)
				ack_fin = _accept_ack_impl(ackid);
	}
	return ack_fin;
}

int rpcudp::_sendto_impl(socket_type sock, const_memory_block blk,
	int flag, const sockaddr_in& addr, int addrlen)
{
	int ret = ::sendto(sock,
		reinterpret_cast<const char*>(blk.buffer), blk.size,
		flag, reinterpret_cast<const sockaddr*>(&addr), addrlen);
	if (ret == SOCKET_ERROR)
	{
		mylog.log(log_error, "sendto_impl have socket_error");
		std::cout << WSAGetLastError() << std::endl;
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
		mylog.log(log_error, "recvfrom_impl have socket_error");
	}
	return ret;
}

const const_memory_block rpcudp::_const_nop_block(nullptr, 0);

const memory_block rpcudp::_nop_block(nullptr, 0);
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

/*
memory_block rpcudp::recvfrom(int flag, sockaddr_in& addr,
int& addrlen, mtime_t timeout)
{
memory_block recvbuf;
recvbuf.size = mtu;
recvbuf.buffer = new char[recvbuf.size];
memory_block ret;
ret.buffer = nullptr;
while (true)
{
int ret = _recvfrom_impl(_udpsock, recvbuf, flag, addr, addrlen);
if (ret == SOCKET_ERROR)
{
mylog.log(log_error, "SOCKET_ERROR", WSAGetLastError());
continue;
}
auto blk = _write(recvbuf, ret, addr);
if (blk.buffer != nullptr)
return blk;
}
return ret;
}
memory_block rpcudp::_write(memory_block recvbuf, size_t packsize,
const sockaddr_in& addr)
{
memory_block nop_ret;
nop_ret.buffer = nullptr;
//用来处理数据的
char* recvbuf_ptr = reinterpret_cast<char*>(recvbuf.buffer);
size_t recvbuf_size = recvbuf.size;
//完整的数据块的缓冲区，不包括头部
memory_block retbuf;
retbuf.buffer = nullptr;//无法现在初始化，要收到第一个包以后
char* retbuf_ptr = nullptr;
recvbuf_size = packsize;
packet_head head;
advance_in(recvbuf_ptr, recvbuf_size,
rarchive_from(recvbuf_ptr, recvbuf_size, head));
auto rlock = ISU_AUTO_RLOCK(_lock);
auto iter = _recv_records.find(head.pack_id);
if (iter == _recv_records.end())
{
rlock.unlock();
auto wlock = ISU_AUTO_WLOCK(_lock);
retbuf.size = head.total_packsize;//无头包的数据总量
retbuf.buffer = (retbuf_ptr = new char[retbuf.size]);
recved_record record;
record.accpeted = 0;
record.pack_count = head.pack_count;
record._memory = retbuf;
auto pair = _recv_records.insert(std::make_pair(head.pack_id, record));
if (!pair.second)
{
mylog.log(log_error, "Insert recv record faild", head.pack_id);
return nop_ret;
}
iter = pair.first;
}
retbuf = iter->second._memory;
auto* recvmap = &iter->second.mybitmap;
size_t& accepted = iter->second.accpeted;
//用于读数据的
retbuf_ptr = reinterpret_cast<char*>(retbuf.buffer);
if (head.pack_type&IS_ACK_PACK)
{
_accept_ack(head, recvbuf_ptr, recvbuf_size);
return nop_ret;
}
if (head.pack_type&IS_MAIN_PACK)
{
size_t adv = recvbuf_size;
//由于包的顺序不一定，所以需要jump一次
size_t length = 0;
size_t mysize = 0;
size_t data_size = mtu - archived_size<packet_head>();
size_t curpos = head.shareding_id*data_size;
if (recvmap->find(head.shareding_id) != recvmap->end())
{
mylog.log(log_debug, "重复的数据包",
head.pack_id, head.shareding_id);
return nop_ret;
}
if (head.shareding_id == head.pack_count - 1)//最后一个包要做点手脚
{
mysize = head.total_packsize
- (head.shareding_id * data_size);
length = (head.shareding_id * data_size);
}
else
{
length = head.shareding_id * data_size;
mysize = data_size;
}
retbuf_ptr = reinterpret_cast<char*>(retbuf.buffer) + length;
memcpy(retbuf_ptr, recvbuf_ptr, recvbuf_size);
//			incerment<1>(retbuf_ptr, adv);
_send_ack(head, addr, sizeof(addr));
++accepted;
if (accepted == head.pack_count)
{
auto ret = iter->second._memory;
//_recv_records.erase(iter);
return ret;
}
}
else if (head.pack_type&IS_ACK_QUERY)
{
_send_ack(head, addr, sizeof(addr));
}
return nop_ret;
}
void rpcudp::_waitlist_of_ack(size_t id,const sockaddr_in& addr,
sendout_record& record)
{
timer_parment* parment = new timer_parment;
parment->pack_id = id;
parment->addr = addr;
parment->resend_count = 0;
auto wlock = ISU_AUTO_WLOCK(_lock);
auto iter = _records.insert(
std::make_pair(id, record)).first;
wlock.unlock();
//WARNING:必须放在这里,以防止set_timer的时候轮转
id = _timer.set_timer(parment, 30);
std::cout << "设置超时器" <<id<< std::endl;
iter->second.timer_id = id;
}
int rpcudp::sendto(rpcid id, const void* buf, size_t bufsize,
int flag, const sockaddr_in& addr,
int addrlen, mtime_t timeout /* = default_timeout */
/*{//这个函数只能用来发送主包
const_memory_block blk;
blk.buffer = buf; blk.size = bufsize;
//返回一个保存着已打好包的数据块和分块点的vector
//这些点都是相对与上一个点的
size_t myid = 0;
auto pair = _make_buf(myid, blk,IS_MAIN_PACK);
_waitlist_of_ack(myid, addr, pair);
int ret = 0;
size_t start = 0;
for (size_t split_point : pair.shardings)
{
const_memory_block sendblk;
sendblk.buffer =
reinterpret_cast<const char*>(pair._memory.buffer) + start;
sendblk.size = split_point;
ret = _sendto_impl(_udpsock, sendblk, flag,
addr, addrlen);
start += split_point;
}
return ret;
}
//impl
void rpcudp::_resend_packet(rpcid id,
const sendout_record& record, const sockaddr_in& addr)
{//发送所有没被清空的包
//	auto rlock = ISU_AUTO_RLOCK(_lock);
for (size_t index = 0; index != record.shardings.size(); ++index)
{
size_t size = record.shardings[index];
if (size)
{
//可以重发了
size_t length = 0;
size_t mysize = 0;
if (index == record.shardings.size()-1)//最后一个包要做点手脚
{
mysize = record._memory.size
- (index*mtu);
length = (index*mtu);
}
else
{
length = index*mtu;
mysize = mtu;
}
const_memory_block blk;
blk.size = mysize;
blk.buffer =
reinterpret_cast<const char*>(record._memory.buffer) + length;
_sendto_impl(_udpsock,
blk, 0, addr, sizeof(addr));
}
}
}
void rpcudp::_ack_callback(timer_handle timer_id, sysptr_t arg)
{
using namespace std;
auto* parment = reinterpret_cast<timer_parment*>(arg);
auto wlock = ISU_AUTO_RLOCK(_lock);
auto iter = _records.find(parment->pack_id);
if (parment->resend_count > 10)
{
std::cout <<parment->pack_id<< "传输失败" << std::endl;
_timer.cancel_timer_when_exit(timer_id);
_records.erase(iter);
return;
}
else
{
iter = _records.find(parment->pack_id);
if (iter != _records.end())
{
wlock.unlock();
std::cout << "重传" << parment->pack_id << std::endl;
_resend_packet(parment->pack_id, iter->second, parment->addr);
++parment->resend_count;
}
}
std::cout << "还有" << _records.size() << "个包等待ACK" << std::endl;
}
void rpcudp::_send_ack(const packet_head& head,
const sockaddr_in& addr,int addrlen)
{
auto myhead = head;
myhead.pack_type = IS_ACK_PACK | UDP_ACK_SINGAL;
auto blk = archive(myhead);
const_memory_block cblk;
cblk.buffer = blk.buffer; cblk.size = blk.size;
std::cout << "发送ack" << std::endl;
_sendto_impl(_udpsock, cblk, 0, addr, addrlen);
}
bool rpcudp::_accept_ack_impl(const packet_head& head,size_t id)
{
auto rlock = ISU_AUTO_RLOCK(_lock);
auto iter = _records.find(head.pack_id);
bool ret = false;
if (iter != _records.end())
{
rlock.unlock();
auto wlock = ISU_AUTO_WLOCK(_lock);
auto& record = iter->second;
record.shardings[id] = 0;//rlock没问题
for (size_t index = 0; index != record.shardings.size(); ++index)
{
size_t is_accepted = record.shardings[index];
if (is_accepted != 0)
return false;
}
_timer.cancel_timer(record.timer_id);
_records.erase(iter);
ret = true;
}
std::cout << "还有" << _records.size() << "个包没有收到ACK" << std::endl;
return ret;
}
bool rpcudp::_accept_ack(const packet_head& head,
void* buffer,size_t bufsize)
{
//1个字节的头
bool ack_fin = false;
size_t acktype = head.pack_type;
struct ack_compressed
{
size_t start, last; //[start, last], [begin,end) warning
};
//不用上锁
if (acktype&UDP_ACK_COMMPRESSED)
{
ack_compressed comack;
advance_in(buffer, bufsize, rarchive_from(buffer, bufsize, comack));
for (; comack.start <= comack.last; ++comack.start)
{
ack_fin=_accept_ack_impl(head, comack.start);
}
}
if (acktype&UDP_ACK_SINGAL)
{
ack_fin=_accept_ack_impl(head, head.shareding_id);
}
else if (acktype&UDP_ACK_MULTI)
{
std::vector<size_t> ackids;
size_t adv = rarchive(buffer, bufsize, ackids);
advance_in(buffer, bufsize, adv);
for (size_t ackid : ackids)
ack_fin=_accept_ack_impl(head, ackid);
}
return ack_fin;
}
size_t rpcudp::_alloc_packid()
{
return ++_allocer;
}
rpcudp::sendout_record
rpcudp::_make_buf(size_t& myid, const_memory_block blk, size_t pack_type)
{
sendout_record ret;
//计算出每个分片点
//打包这些数据
size_t shareing = ceil(static_cast<double>(blk.size) /
(mtu - sizeof(packet_head)));
size_t head_size = archived_size<packet_head>();
memory_block pack;//完整的，要发送的数据包
pack.size = shareing*head_size + blk.size;
pack.buffer = new char[pack.size];
char* packbuf = reinterpret_cast<char*>(pack.buffer);
size_t less_size = pack.size;
std::vector<size_t> shareing_point;//分片点
const char* databuf = reinterpret_cast<const char*>(blk.buffer);
size_t id_index = 0;
size_t id = _alloc_packid();
myid = id;
size_t count = shareing;
assert(shareing != 0);
while (shareing--)
{
size_t shareing_size = (less_size < mtu) ? less_size : mtu;
less_size -= shareing_size;
shareing_point.push_back(shareing_size);
packet_head head;
head.pack_count = count;
head.shareding_id = id_index++;
head.total_packsize = pack.size;
head.pack_type = pack_type;
head.pack_id = id;
size_t adv = archive_to(packbuf, shareing_size, head);
advance_in(packbuf, shareing_size, adv);
//普通数据块
memcpy(packbuf, databuf, shareing_size);
incerment<1>(packbuf, shareing_size);
incerment<1>(databuf, shareing_size);
}
assert(less_size == 0);
ret.shardings = shareing_point;
ret._memory = pack;
return ret;
}*/