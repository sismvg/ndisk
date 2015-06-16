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
//ack��,���ݰ�,���ݰ�,�հ�,�����ð�

/*
Flag magic
S-flag name=����flag���õ�ʱ��,�����ε�flag name������
posA&posB=��posA��posBͬʱ���õ�ʱ��������һ����ģʽ,��������pos����
������˫0����0����ʾһ��ģʽ

0~1λ-udp����ģʽ
WORK_IN_SLOW_NETWORK 0 -���������ٵ�����
WORK_IN_HIGH_SPEED_NETWORK 1 -��������پ�����

2-24λ-������
SLICE_WITH_ACKS 2 -�Ӵ���ACK�İ�
SLICE_WITH_DATA 3 -���з�Ƭ���ݵİ�
SLICE_NOP 4 -�հ�,��������ͷ��
SLICE_DEBUG 5 -���԰�,����
ACK_SINGLE 6 -ACK����ֻ��һ����ֵ
ACK_COMPRESSED 7 -��ACK���ж����ֵ,�����Ǳ�ѹ����
ACK_MULTI 8 -��ACK���ж����ֵ
SLICE_DATA_COMPRESSED 9 -�÷�Ƭ����������ѹ����
SLICE_DATA_COMPRESSED_BY_ZIP 10 -�÷�Ƭ����zip�㷨ѹ����
SLICE_WITH_RTT 11 -�÷�Ƭ����RTT����ͷ
SLICE_WITH_RTT_RECV 12 -�÷�Ƭ����RTT����
SLICE_WITH_WINDOW 13 -�÷�Ƭ���д��ڵ���
SLICE_DATA_WITH_MORE_SLICE 14 -�÷�Ƭ����nagle�ķ�Ƭ
SLICE_WITH_EX_DATA 15-������չͷ��

25-31λ-�����߲���ϵͳ��Ϣ
PLATFORM_X64 25
PLATFORM_AMD 26
OPERATOR_SYSTEM_WINDOWS 27
OPERATOR_SYSTEM_LINUX 28
OERATOOR_SYSTEM_OSX 29
OPERATOR_SYSTEM_UNKNOW 30
31����
*/

#define NOP_MAGIC 0

#define WORK_IN_SLOW_NETOWRK DEFINE_BIT_FLAG_HTONL(0)
#define WORK_IN_HIGH_SPEED_NETWORK DEFINE_BIT_FLAG_HTONL(1)

#define SLICE_WITH_ACKS	DEFINE_BIT_FLAG_HTONL(2)
#define SLICE_WITH_DATA	DEFINE_BIT_FLAG_HTONL(3)
#define SLICE_NOP		DEFINE_BIT_FLAG_HTONL(4)
#define SLICE_DEBUG		DEFINE_BIT_FLAG_HTONL(5)

//6,7,8��ackbitmap.hpp����

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
udp��������ģʽ
*/

//�ر�ACK��NAGLE�㷨
#define DISABLE_ACK_NAGLE DEFINE_BIT_FLAG_HTONL(3)

//��sendto��NAGLE�㷨
#define ENABLE_SENDTO_NAGLE DEFINE_BIT_FLAG_HTONL(4)

//����ѹ����Ƭ����
#define ENABLE_COMPRESSED_SLICE DEFINE_BIT_FLAG_HTONL(5)

#define RPCUDP_DEFAULT_ALLOCATOR_NAME std::allocator
#define RPCUDP_DEFAULT_ALLOCATOR(type) RPCUDP_DEFAULT_ALLOCATOR_NAME<type>

#endif