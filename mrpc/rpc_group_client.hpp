#ifndef ISU_RPC_GROUP_CLIENT_HPP
#define ISU_RPC_GROUP_CLIENT_HPP

#include <vector>
#include <archive.hpp>

#include <rpcdef.hpp>
#include <rpclock.hpp>

//��Ϊͬ�������첽������
//������ͬͷ:pointer of rpcinfo,rpcid[func_ident,1λ������,31λ���ñ�ʶ]
//�첽��: 
//client��:��ͷ,����
//server��:��ͷ,rpc_request_msg,����
//ͬ����
//client��:��ͷ����ʱrpcid�ĺ�31λ��ʶ���Ĵ�С,����
//server��:^,rpc_request_msg,����
//rpc��
//��ͬͷ:group type
//�첽��:
//client��:��ͷ,�첽���ͬ����Ϣ��,�첽��ͷ��rpcidֻ��func_ident����
//server��:^
//ͬ����:
//client��:��ͷ,�ɰ�ͷָ�����Ƿ�ӵ����Ϣ��,ǰmislength_count
//����ֻ��func_ident����,ʣ��rpcid����32λ���ǰ���С
//server��:^
//���ͷ
//һ��8λ�� �����Ҵ�С����һ�ηֱ���
//[������][�Ƿ�ӵ����Ϣ��][ͬ�����Ƿ�ǿ��ÿ����ӵ��size]�������
//[�����Ƿ�Ϊ����][���Ƿ�ѹ��][3λ����,Ϊ1]
//ʵ��

class rpc_group_middleware;

class rpc_group_client
{//��ʵ�ϣ��ⶫ�����Ǹ�archiveģ��..
public:
	~rpc_group_client();
	rpc_group_client(size_t length = 0, size_t mission_count = 0,
		size_t flag = GRPACK_HAVE_MSG_LOCK,
		initlock* lock = nullptr);
	rpc_group_client(const rpc_group_middleware&);

	template<class... Arg>
	const_memory_block push_mission(const rpc_head& head,Arg&... arg)
	{
		rpcid id = head.id.id;
		bool is_sync_pack = id.id&ID_ISSYNC;

		size_t size = archived_size(head, arg...);
		if (head.id.id&ID_ISSYNC)
			id.id = size;//ͬ������packet_identû������
		else
			_impl.miscount.group_type |= GRPACK_ASYNC;

		if (_memory.capacity() < (_memory.size() + size))
		{//WARNING:����ָ����ܲ�������
			//_memory.reserve(_memory.size() + size);
		}

		memory_block blk = archive(head, arg...);
	//	blk.buffer = reinterpret_cast<char*>(&_memory.back()) + 1;
		const char* begin = reinterpret_cast<const char*>(blk.buffer);
	//	blk.size = size;

	//	archive_to(blk.buffer, blk.size, arg...);
		_memory.insert(_memory.end(), begin, begin + blk.size);
		_update_grpinfo(size);
		return blk;
	}

	const_memory_block group_block();

	size_t size() const;
	size_t memory_size() const;

	bool standby();
	void ready_wait();
	void wait();
	void force_standby();
	void set_manually_standby();//�����ֶ�standby�Ժ���Ҫ��force_standby����������
	void reset();
public:
	static const size_t head_size = sizeof(group_impl);

	size_t _head_size(size_t flag);
	initlock* _lock;//����֧��wait
	group_impl _impl;
	size_t _flag;
	size_t _mission_count;
	std::vector<unsigned char> _memory;//��vector��Ϊ��ָ���������
	//utility
	void _update_grpinfo(size_t size);
};
#endif