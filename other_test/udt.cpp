
#include <udt.hpp>
#include <mpl_helper.hpp>

udt::udt(const udpaddr& local, size_t limit_thread)
	:
		_core(local,MEMBER_BIND_IN(_udt_bad_send),limit_thread),
		_groups_lock(BOOST_DETAIL_SPINLOCK_INIT)
{

}

udt::udt(const udpaddr& local, bad_send_functional bad_send,
	size_t limit_thread)
	: 
		_core(local, MEMBER_BIND_IN(_udt_bad_send),limit_thread),
		_bad_send(bad_send),
		_groups_lock(BOOST_DETAIL_SPINLOCK_INIT)
{

}

udt_recv_result udt::recv()
{
	udt_recv_result result;
	bool runable = true;
	do
	{
		recv_result data = _core.recv();
		const io_result& error = data.get_io_result();
		if (error.in_vaild())
		{//没有复制到标准缓冲区中...
			const _group_ident* ptr =
				reinterpret_cast<const _group_ident*>(data.begin());

			recv_result::iterator begin =
				data.begin() + sizeof(_group_ident);

			shared_memory memory = _insert(*ptr,
				const_memory_block(begin, data.end() - begin));

			if (memory.get() != nullptr)
			{
				result.io_state = error;
				result.memory = memory;
				runable = false;
			}
		}
		else
		{
			result.io_state = error;
			runable = false;
		}
	} while (runable);
	return result;
}

io_result udt::recv(bit_type* buffer, size_t size, core_sockaddr& from)
{
	recv_result data = _core.recv();
	if (data.get_io_result().in_vaild())
	{
		recv_result::iterator begin = data.begin() + sizeof(_group_ident);
		deep_copy_to(memory_block(buffer, size),
			const_memory_block(begin, data.end() - begin));
		from = data.from();
	}
	return data.get_io_result();
}

io_result udt::send(const core_sockaddr& dest,
	const const_shared_memory& data, size_t size)
{
	size_t slice_size =
		trans_core::maxinum_slice_size() + trans_core::trans_core_reserve();

	size_t group_id = _alloc_group_id();
	slicer::user_head_genericer slicer_head_gen
		= [&](size_t slice_id, size_t data_start, archive_packages& pac)
	{
		pac.fill(trans_core::trans_core_reserve());

		_group_ident ident;
		ident.group_id = group_id;
		ident.slice_start = data_start;
		ident.data_size = data.size();

		pac.push_back(make_block(ident));
	};
	//头部大小不对
	size_t user_keep = trans_core::trans_core_reserve() +
		sizeof(_group_ident) + sizeof(slicer_head);

	slicer slice_core(data, slice_size, slicer_head_gen,
		user_keep, concrete);

	io_result result;
	while (!slice_core.slice_end())
	{
		auto iter = slice_core.get_next_slice();
		size_t size = iter->first.size() + iter->second.size();
		dynamic_memory_block<char> buffer(size);

		buffer.push(iter->first);
		buffer.push(iter->second.data());

		result = _core.send(buffer.memory(), dest);
		if (!result.in_vaild())
			break;
	}
	return result;
}

//
udt::group_id_type udt::_alloc_group_id()
{
	return _group_id_allocer++;
}

udt::shared_memory udt::_insert(const _group_ident& ident,
	const const_memory_block& data)
{
	shared_memory result(memory_block(nullptr, 0));

	auto lock = ISU_AUTO_LOCK(_groups_lock);
	auto iter = _recv_groups.find(ident.group_id);
	if (iter == _recv_groups.end())
	{
		slicer_head head;
		head.data_size = ident.data_size;

		_recv_group group;
		group._combin = combination(head);
		iter = _recv_groups.emplace(ident.group_id,group).first;
	}	

	auto& combin = iter->second._combin;

	combin.insert(ident.slice_start, data);

	if (combin.accepted_all())
		result = combin.data().first;
	return result;
}

void udt::_udt_bad_send(const core_sockaddr& dest,
	const const_shared_memory& memory, package_id_type id)
{//cancel掉改组的所有包?
	if (_bad_send)
	{
		const auto* begin = memory.get() +
			trans_core::trans_core_reserve();

		const auto* ptr = reinterpret_cast<const _group_ident*>(begin);
		const char* end = memory.get() + memory.size();

		_bad_send(ptr->group_id, dest,
			const_memory_block(begin, end - begin));
	}
}