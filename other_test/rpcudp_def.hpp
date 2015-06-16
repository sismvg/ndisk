#ifndef ISU_RPCUDP_DEF_HPP
#define ISU_RPCUDP_DEF_HPP

#include <exception>
#include <slice.hpp>
#include <ackbitmap.hpp>

class rpcudp_wrong_format
	:public std::runtime_error
{
public:
	rpcudp_wrong_format(const char* msg)
		:std::runtime_error(msg)
	{}
};

struct udp_header
{
	size_t type_magic;
};

struct udp_header_ex
{
	size_t group_id;
	size_t slice_id;
	slicer_head slihead;
};


#define SHARED_MEMORY(obj) rpcudp::shared_memory(obj)
//ack包,数据包,数据包,空包,调试用包

/*
Flag magic
S-flag name=当该flag设置的时候,会屏蔽掉flag name的设置
posA&posB=当posA和posB同时设置的时候允许定义一个新模式,允许更多的pos参与
不允许双0或多个0来表示一种模式

0~1位-udp工作模式
WORK_IN_SLOW_NETWORK 0 -工作于慢速的外网
WORK_IN_HIGH_SPEED_NETWORK 1 -工作与高速局域网

2-24位-包类型
SLICE_WITH_ACKS 2 -捎带了ACK的包
SLICE_WITH_DATA 3 -带有分片数据的包
SLICE_NOP 4 -空包,仅仅包含头部
SLICE_DEBUG 5 -测试包,保留
ACK_SINGLE 6 -ACK包中只有一个数值
ACK_COMPRESSED 7 -该ACK包有多个数值,但包是被压缩的
ACK_MULTI 8 -该ACK包有多个数值
SLICE_DATA_COMPRESSED 9 -该分片包被被无损压缩过
SLICE_DATA_COMPRESSED_BY_ZIP 10 -该分片包被zip算法压缩过
SLICE_WITH_RTT 11 -该分片带有RTT计算头
SLICE_WITH_RTT_RECV 12 -该分片带有RTT接受
SLICE_WITH_WINDOW 13 -该分片带有窗口调整
SLICE_DATA_WITH_MORE_SLICE 14 -该分片带有nagle的分片
SLICE_WITH_EX_DATA 15-带有扩展头部

25-31位-发送者操作系统信息
PLATFORM_X64 25
PLATFORM_AMD 26
OPERATOR_SYSTEM_WINDOWS 27
OPERATOR_SYSTEM_LINUX 28
OERATOOR_SYSTEM_OSX 29
OPERATOR_SYSTEM_UNKNOW 30
31保留
*/

#define NOP_MAGIC 0

#define WORK_IN_SLOW_NETOWRK DEFINE_BIT_FLAG_HTONL(0)
#define WORK_IN_HIGH_SPEED_NETWORK DEFINE_BIT_FLAG_HTONL(1)

#define SLICE_WITH_ACKS	DEFINE_BIT_FLAG_HTONL(2)
#define SLICE_WITH_DATA	DEFINE_BIT_FLAG_HTONL(3)
#define SLICE_NOP		DEFINE_BIT_FLAG_HTONL(4)
#define SLICE_DEBUG		DEFINE_BIT_FLAG_HTONL(5)

//6,7,8在ackbitmap.hpp定义

#define SLICE_DATA_COMPRESSED DEFINE_BIT_FLAG_HTONL(9)
#define SLICE_DATA_COMPRESSED_BY_ZIP DEFINE_BIT_FLAG_HTONL(10)
#define SLICE_WITH_RTT		DEFINE_BIT_FLAG_HTONL(11)
#define SLICE_WITH_RTT_RECV	DEFINE_BIT_FLAG_HTONL(12)
#define SLICE_WITH_WINDOW	DEFINE_BIT_FLAG_HTONL(13)
#define SLICE_DATA_WITH_MORE_SLICE DEFINE_BIT_FLAG_HTONL(14)
#define SLICE_WITH_EX_DATA DEFINE_BIT_FLAG_HTONL(15)

#define PLATFORM_X64 DEFINE_BIT_FLAG_HTONL(25)
#define PLATFORM_AMD DEFINE_BIT_FLAG_HTONL(26)

#define OPERATOR_SYSTEM_WINDOWS DEFINE_BIT_FLAG_HTONL(27)
#define OPERATOR_SYSTEM_LINUX	DEFINE_BIT_FLAG_HTONL(28)
#define OPERATOR_SYSTEM_OSX		DEFINE_BIT_FLAG_HTONL(29)
#define OPERATOR_SYSTEM_UNKNOW	DEFINE_BIT_FLAG_HTONL(30)

/*
udp本机工作模式
*/

//关闭ACK的NAGLE算法
#define DISABLE_ACK_NAGLE DEFINE_BIT_FLAG_HTONL(3)

//打开sendto的NAGLE算法
#define ENABLE_SENDTO_NAGLE DEFINE_BIT_FLAG_HTONL(4)

//允许压缩分片数据
#define ENABLE_COMPRESSED_SLICE DEFINE_BIT_FLAG_HTONL(5)

#define RPCUDP_DEFAULT_ALLOCATOR_NAME std::allocator
#define RPCUDP_DEFAULT_ALLOCATOR(type) RPCUDP_DEFAULT_ALLOCATOR_NAME<type>

#endif