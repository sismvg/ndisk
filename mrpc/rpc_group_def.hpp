#ifndef ISU_RPC_GROUP_DEF_HPP
#define ISU_RPC_GROUP_DEF_HPP

class remote_produce_group;
class remote_produce_middleware;

typedef size_t seralize_uid;

class active_event;
struct group_trunk
{
	active_event* event_ptr;
};

#define GROUP_CLIENT 0x1
#define GROUP_SERVER 0x10

struct group_head
{
	size_t flag;//flag
	group_trunk trunk;
	seralize_uid uid;//序列化程序标识
	size_t reailty_marble;//固有包大小
	size_t mission_count;
};

//包的格式
struct real_mission_client
{
	size_t size;//包的大小
	rpc_head head;
};

struct real_mission_server
{
	size_t size;//^
	rpc_head head;
	rpc_request_msg msg;
};

#endif