
#include <rpcudp.hpp>
#include <iostream>
#include <random>

#include <boost/lexical_cast.hpp>
std::random_device dev;
std::mt19937 mt(dev());
std::uniform_int<> from(0,100);

#define MYCALLBACK(name) [&](timer_handle id,sysptr_t ptr)\
	{\
		name(id,ptr);\
	}

rpcudp::rpcudp()
	: _udpsock(-1), _timer(MYCALLBACK(_check_ack)), 
	_sem(_get_window_size()), _group_id_allocer(0),
	_recved_lock()
{}

rpcudp::rpcudp(socket_type sock,udpmode mode)
	: _udpsock(sock), _timer(MYCALLBACK(_check_ack)),_mode(mode),
	_sem(_get_window_size()), _group_id_allocer(0)
{}

rpcudp::~rpcudp()
{
	mylog.log(log_debug, "rpcudp exit");
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
		int ret = _recvfrom_impl(_udpsock, recvbuf, realflag, addr, addrlen);
		if (ret ==SOCKET_ERROR)
		{
			break;
		}
		auto pair =
			_package_process(addr, addrlen, const_memory_block(recvbuf.buffer, ret));
		if (flag&WHEN_RECV_ACK_BREAK)
		{
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
	sockaddr_in& addr,mtime_t timeout)
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
	//TODO:ʵ��
}


int rpcudp::sendto(const_memory_block blk, int flag, const sockaddr_in& addr)
{
	return sendto(blk.buffer, blk.size, flag, addr, sizeof(addr));
}

int rpcudp::sendto(const void* buf, size_t bufsize,
	int flag, const sockaddr_in& addr, int addrlen)
{
	auto slices = _slice(const_memory_block(buf, bufsize));

	int ret = 0;
	_to_send(slices.group_id, addr, slices.begin(), slices.end());
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
	//���������ȴû�б�ɾ���İ��򷵻�
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
	{//�Ѿ����ܹ����ACK��
		_recved_lock.unlock();
		_timer.cancel_timer_when_exit(handle);
		return;
	}
	else
	{//û�����ش�
	//	mylog.log(log_debug, "retry");
		if (parment->max_retry-- == 0)
		{
			_recved_lock.unlock();
			_timer.cancel_timer_when_exit(handle);
		//	mylog.log(log_debug, "exit retry");
			return;
		}
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
	return 30;
}

size_t rpcudp::_dynamic_retry_count(const sendout_record& record)
{
	return 4;
}

size_t rpcudp::_wait_ack(const sockaddr_in& addr,packid_type id)
{ 
	size_t ret = 0;
	auto addrtmp = addr;
	int len = sizeof(addr);
	auto iter = _recved.find(id);
	if (iter != _recved.end())
	{
		memory_block blk;
		blk.buffer = new char[20];
		blk.size = 20;
		_recvfrom_impl(_udpsock, blk, 0, addrtmp, len);
		slice_head head;
		rarchive_from(blk.buffer, blk.size, head);
		if (head.slice_type&IS_ACK_PACK)
		{
			advance_in(blk, 20);
			_accept_ack(head, blk);
		}
		//recvfrom(WHEN_RECV_ACK_BREAK, addrtmp, len);
		//MSG:ֻҪ�յ�ACK�ͱ�ʾ�ֿ��Է������ˣ����Ե��յ�ACK����������
	}
	return (std::min)(ret, _get_window_size());
}

size_t rpcudp::_get_window_size(const sockaddr_in& addr)
{
	return _get_window_size();
}

size_t rpcudp::_get_window_size()
{
	return 2000;
}

slice_vector rpcudp::_slice(const_memory_block blk)
{
	//TODO:����̫������Ҫ�Ż�
	//������:
	//1:������Ҫ�ֳ�����Ƭ�������Ƭͷ����ܳ���
	//2:����ͷ��ԭʼ���ݵ���Ƭ��
	//3:����slice_vector
	slice_vector ret;

	ret.group_id = _alloc_group_id();
	ret.accepted = 0;

	//��ʼ��Ƭ
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
		//��ʼ��ͷ
		head.group_id = ret.group_id;
		head.slice_type = IS_MAIN_PACK;
		head.total_size = size_with_head;
		head.total_slice = total_slice;
		head.slice_id = id++;
		//
		incerment<1>(ptr, archive_to(ptr, slice_size, head));
		memcpy(ptr, dataptr, data_size);
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
	{//TODO:_accept_ack��Ҫ����һ��size_t����ʾACKͷ���˶����ֽ�
		_accept_ack(head, blk);
		return true;
	}
	return false;
}

std::pair<bool,memory_block> 
rpcudp::_package_process(const sockaddr_in& addr, int len, const_memory_block blk)
{
	//ack��������&����withack
	//����ֱ�ӷֲ�ͷ��д������

	memory_block ret = _nop_block;
	slice_head head;
	advance_in(blk.buffer, blk.size,
		rarchive_from(blk.buffer, blk.size, head));

	bool have_ack = false;

	if (head.slice_type&IS_ACK_PACK)
	{
		have_ack=_accept_ack(head, blk);
	}
	if (head.slice_type&IS_MAIN_PACK)
	{
		auto myid = _makeid(head.group_id, head.slice_id);
		auto wlock_recv = ISU_AUTO_LOCK(_recved_pack_lock);
		if (_recved_pack.find(myid) == _recved_pack.end())
		{//û���յ��������İ�.
			auto wlock = ISU_AUTO_LOCK(_recved_dgrm_lock);
			const_memory_block rawdata = blk;
			auto iter = _write_to(head, rawdata);

			if (iter->second.accepted == head.total_slice)
			{
				auto wlock = ISU_AUTO_LOCK(_findished_lock);
				_recved_pack.insert(myid);
				_finished.push_back(iter->second);
				_recved_dgrm.erase(iter);//������Ҫ�������
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
{//blk.size���ܰ���slice_head
	size_t id = _makeid(head.group_id, head.slice_id);
	auto iter = _recved_dgrm.find(id);
	if (iter == _recved_dgrm.end())
	{
		//��ʼ��
		recved_record record;
		record.accepted = 0;
		size_t size= head.total_size - 
			head.total_slice*archived_size<slice_head>();
		record.memory.buffer=_alloc_memory_raw(size);
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
	if (head.slice_id == head.total_slice - 1)//���һ����Ҫ�����ֽ�
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

void rpcudp::_send_ack(slice_head head,const sockaddr_in& addr,int len)
{
	head.slice_type = 0;
	head.slice_type = IS_ACK_PACK | UDP_ACK_SINGAL;
	auto myblk = archive(head);
	_sendto_impl(_udpsock, myblk, 0, addr, len);
}

bool rpcudp::_accept_ack_impl(const slice_head& head,size_t slice_id)
{
	_recved_lock.lock();
	auto id = _makeid(head.group_id, head.slice_id);
	auto iter = _recved.find(id);
	if (iter != _recved.end())
	{
		_sem.post();
	//	_timer.cancel_timer(iter->second);
		_recved.erase(iter);
		_recved_lock.unlock();
		return true;
	}
	_recved_lock.unlock();
	return false;
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

void rpcudp::_to_send(size_t group_id, const sockaddr_in& addr,
		slice_iterator begin, slice_iterator end)
{
//	auto wlock = ISU_AUTO_LOCK(_send_list_lock);
	auto& wlock = _send_list_lock;
	_send_list_lock.lock();
	for (auto mybegin = begin; mybegin != end; ++mybegin)
	{
		auto* ptr = _regist_slice(group_id, addr, *mybegin);
		_send_list.push_back(ptr);
	}
	auto iter = _send_list.begin();
//	auto wlock2 = ISU_AUTO_LOCK(_recved_lock);
	auto& wlock2 = _recved_lock;
	for (; iter != _send_list.end();)
	{
		_recved_lock.lock();
		auto* buf = *iter;
		auto pair = _recved.insert(std::make_pair(buf->id, iter++));
		_recved_lock.unlock();
		if(!_sem.try_wait())
		{
			wlock.unlock();
			_wait_ack(addr, buf->id);
			_sem.wait();
			wlock.lock();
		}
		buf->timer_id = _timer.set_timer(buf, _dynamic_timeout(), 0);
		_sendto_impl(_udpsock, buf->block, 0,
			buf->addr, sizeof(buf->addr));
		auto err = GetLastError();
	}
	wlock.unlock();
	for (; begin != end; ++begin)
	{
		_wait_ack(addr, _makeid(group_id, begin->slice_id));
	}
	_send_list.clear();
}

bool rpcudp::_accept_ack(const slice_head& head, const_memory_block blk)
{
	//1���ֽڵ�ͷ
	bool ack_fin = false;
	size_t acktype = head.slice_type;

	struct ack_compressed
	{
		size_t start, last; //[start, last], [begin,end) warning
	};
	//��������
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
		std::vector<size_t> ackids;
		size_t adv = rarchive(blk.buffer, blk.size, ackids);
		advance_in(blk, adv);
		for (size_t ackid : ackids)
			ack_fin = _accept_ack_impl(head, ackid);
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
		mylog.log(log_error, "sendto_impl have socket_error");
	return ret;
}

int rpcudp::_recvfrom_impl(socket_type sock,
	memory_block blk, int flag, sockaddr_in& addr, int& addrlen)
{
	int ret = ::recvfrom(sock,
		reinterpret_cast<char*>(blk.buffer), blk.size,
		flag, reinterpret_cast<sockaddr*>(&addr), &addrlen);
	if (ret == SOCKET_ERROR)
		mylog.log(log_error, "recvfrom_impl have socket_error");
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

	//�����������ݵ�
	char* recvbuf_ptr = reinterpret_cast<char*>(recvbuf.buffer);
	size_t recvbuf_size = recvbuf.size;

	//���������ݿ�Ļ�������������ͷ��
	memory_block retbuf;
	retbuf.buffer = nullptr;//�޷����ڳ�ʼ����Ҫ�յ���һ�����Ժ�
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

		retbuf.size = head.total_packsize;//��ͷ������������
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

	//���ڶ����ݵ�
	retbuf_ptr = reinterpret_cast<char*>(retbuf.buffer);

	if (head.pack_type&IS_ACK_PACK)
	{
		_accept_ack(head, recvbuf_ptr, recvbuf_size);
		return nop_ret;
	}
	if (head.pack_type&IS_MAIN_PACK)
	{
		size_t adv = recvbuf_size;
		//���ڰ���˳��һ����������Ҫjumpһ��
		size_t length = 0;
		size_t mysize = 0;
		size_t data_size = mtu - archived_size<packet_head>();
		size_t curpos = head.shareding_id*data_size;
		if (recvmap->find(head.shareding_id) != recvmap->end())
		{
			mylog.log(log_debug, "�ظ������ݰ�",
				head.pack_id, head.shareding_id);
			return nop_ret;
		}
		if (head.shareding_id == head.pack_count - 1)//���һ����Ҫ�����ֽ�
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
	//WARNING:�����������,�Է�ֹset_timer��ʱ����ת
	id = _timer.set_timer(parment, 30);
	std::cout << "���ó�ʱ��" <<id<< std::endl;
	iter->second.timer_id = id;
}

int rpcudp::sendto(rpcid id, const void* buf, size_t bufsize,
	int flag, const sockaddr_in& addr,
	int addrlen, mtime_t timeout /* = default_timeout */
/*{//�������ֻ��������������
	const_memory_block blk;
	blk.buffer = buf; blk.size = bufsize;
	//����һ���������Ѵ�ð������ݿ�ͷֿ���vector
	//��Щ�㶼���������һ�����
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
{//��������û����յİ�
//	auto rlock = ISU_AUTO_RLOCK(_lock);
	for (size_t index = 0; index != record.shardings.size(); ++index)
	{
		size_t size = record.shardings[index];
		if (size)
		{
			//�����ط���
			size_t length = 0;
			size_t mysize = 0;
			if (index == record.shardings.size()-1)//���һ����Ҫ�����ֽ�
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
		std::cout <<parment->pack_id<< "����ʧ��" << std::endl;
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
			std::cout << "�ش�" << parment->pack_id << std::endl;
			_resend_packet(parment->pack_id, iter->second, parment->addr);
			++parment->resend_count;
		}
	}
	std::cout << "����" << _records.size() << "�����ȴ�ACK" << std::endl;
}



void rpcudp::_send_ack(const packet_head& head,
	const sockaddr_in& addr,int addrlen)
{
	auto myhead = head;

	myhead.pack_type = IS_ACK_PACK | UDP_ACK_SINGAL; 
	auto blk = archive(myhead);

	const_memory_block cblk;
	cblk.buffer = blk.buffer; cblk.size = blk.size;
	std::cout << "����ack" << std::endl;
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
		record.shardings[id] = 0;//rlockû����

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
	std::cout << "����" << _records.size() << "����û���յ�ACK" << std::endl;
	return ret;
}

bool rpcudp::_accept_ack(const packet_head& head,
	void* buffer,size_t bufsize)
{
	//1���ֽڵ�ͷ
	bool ack_fin = false;
	size_t acktype = head.pack_type;

	struct ack_compressed
	{
		size_t start, last; //[start, last], [begin,end) warning
	};
	//��������
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
	//�����ÿ����Ƭ��
	//�����Щ����

	size_t shareing = ceil(static_cast<double>(blk.size) /
		(mtu - sizeof(packet_head)));

	size_t head_size = archived_size<packet_head>();

	memory_block pack;//�����ģ�Ҫ���͵����ݰ�
	pack.size = shareing*head_size + blk.size;
	pack.buffer = new char[pack.size];

	char* packbuf = reinterpret_cast<char*>(pack.buffer);
	size_t less_size = pack.size;
	std::vector<size_t> shareing_point;//��Ƭ��

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
		//��ͨ���ݿ�
		memcpy(packbuf, databuf, shareing_size);
		incerment<1>(packbuf, shareing_size);
		incerment<1>(databuf, shareing_size);
	}
	assert(less_size == 0);
	ret.shardings = shareing_point;
	ret._memory = pack;
	return ret;
}*/