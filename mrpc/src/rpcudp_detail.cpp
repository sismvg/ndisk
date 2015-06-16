
#include <rpcudp.hpp>
#include <rpcudp_detail.hpp>

namespace rpcudp_detail
{
	__slice_head_attacher::__slice_head_attacher(packages_analysis& sis)
		:udp(nullptr), udp_ex(nullptr)
	{
		sis.next_block([&](size_t index, const_memory_block blk)
			->size_t
		{
			udp = reinterpret_cast<const udp_header*>(blk.buffer);
			return sizeof(udp_header);
		});
		if (udp->type_magic&SLICE_WITH_DATA)
		{
			sis.next_block([&](size_t index, const_memory_block blk)
				->size_t
			{
				udp_ex = reinterpret_cast<const udp_header_ex*>(blk.buffer);
				return sizeof(udp_header_ex);
			});
		}
	}
}