#ifndef ISU_RPCUDP_DETAIL_HPP
#define ISU_RPCUDP_DETAIL_HPP

#include <udp_client.hpp>

#include <archive_def.hpp>
#include <archive_packages.hpp>
//实现细节

struct udp_header;
struct udp_header_ex;
struct slice_head;


namespace rpcudp_detail
{
	struct __slice_head_attacher
	{
		//会附着到内存块上
		//但是不允许修改数据
		__slice_head_attacher(packages_analysis&);

		const udp_header* udp;
		const udp_header_ex* udp_ex;
	};
}
#endif