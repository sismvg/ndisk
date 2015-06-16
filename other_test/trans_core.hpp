#ifndef ISU_TRANS_CORE_HPP
#define ISU_TRANS_CORE_HPP

#include <socket.hpp>
#include <memory_helper.hpp>
extern std::atomic_size_t retry;
class trans_core_impl;

typedef unsigned long package_id_type;



enum trans_error_code{
	//��ͷ��Ԥ�����ڴ治��
	not_enough_for_head = 1,
	//������߳��˳�
	recv_thread_exit,
	//�ں�socket����
	has_system_socket_error,
	//�ڴ治��
	memory_not_enough,
	//�������ڴ治�������C++�쳣
	have_cpp_exception,
	//�ش���ι���,������û�д���ɹ�
	send_faild,
};

struct io_result
{//BE RULE3:�����ϻᱻinline�ĺ�������д��ͷ�ļ���
	typedef int error_code;

	/*
	����ʵ�����,��Ҫ����
	*/
	io_result()
		:_code(0), _system_error(0), _id(-1)
	{}

	io_result(package_id_type id,
		error_code local_code = 0, error_code system_code = 0)
		:_id(id), _code(local_code), _system_error(system_code)
	{}

	/*
	I/O�����Ƿ�ɹ�����
	*/
	inline bool in_vaild() const
	{
		return _code == 0 && _system_error == 0;
	}

	/*
	trans_core��I/O����������ʲô����
	*/
	inline error_code what() const
	{
		return _code;
	}

	/*
	�ں�socket������ʲô����
	*/
	inline error_code system_socket_error() const
	{
		return _system_error;
	}

	/*
	��������ݵı�ʶ
	*/
	inline package_id_type id() const
	{
		return _id;
	}
private:
	error_code _code;
	error_code _system_error;
	package_id_type _id;
};

struct recv_result
{
public:
	typedef std::size_t size_t;
	typedef char bit;
	typedef const char const_bit;
	typedef bit* iterator;
	typedef const_bit* const_iterator;
	typedef recv_result self_type;

	typedef shared_memory_block<const char> const_shared_memory;
	typedef shared_memory_block<char> shared_memory;

	/*
		����,ʵ�����
	*/
	recv_result();
	recv_result(const io_result&, const core_sockaddr& from,
		const shared_memory& memory, size_t size);

	/*
		˭���͹�����
	*/
	const core_sockaddr& from() const;

	/*
		����I/O����ʱ��I/O״̬
	*/
	const io_result& get_io_result() const;

	/*
		��Ч��������,������䲻����ͷ��
	*/
	iterator begin();
	iterator end();
	const_iterator begin() const;
	const_iterator end() const;

	/*
		udp�����е�ͷ��Ԥ��
	*/
	size_t reserve();

	/*
		���յ��İ���id
	*/
	package_id_type id() const;

	/*
		����ͷ����ԭʼ�ڴ�
	*/
	shared_memory raw_memory();
	const_shared_memory raw_memory() const;
private:
	io_result _io_error;
	core_sockaddr _from;
	shared_memory _memory;
	size_t _size;
	iterator _begin();
	const_iterator _begin() const;
};

#define DEFAULT_ALLOCATOR std::allocator<char>

class trans_core
{
	//����֤˳����
	//�ڳ����ش�������ǰ,���ᶪ��
public:
	/*
		trans_core_implԤ����ͷ������
	*/
	static size_t trans_core_reserve();
	/*
		һ�����ݷ�Ƭ��߿����ж೤(������ͷ��)
	*/
	static size_t maxinum_slice_size();

	typedef shared_memory_block<const char> const_shared_memory;
	typedef shared_memory_block<char> shared_memory;

	typedef sockaddr_in udpaddr;
	typedef std::function < void(const core_sockaddr&,
		const const_shared_memory&, package_id_type) > bad_send_functional;
	/*
		���ص�ַ
		limit_thread:���ڴ���recv�ķ���ֵ������߳�����
		���Ե���windows��I/O��ɶ˿�ʹ��
	*/
	trans_core(const udpaddr&, size_t limit_thread = 1);
	trans_core(const udpaddr&,
		bad_send_functional bad_send, size_t limit_thread = 1);
	//���ͷ�����Ԥ��trans_core_reserve���ֽڵ�ͷ��

	/*
		װ��Ҫ���͵����ݵ�ͷ��
		�����memory.size()С��Ԥ����ͷ������
		io_resul��waht�᷵��not_enough_for_head
	*/
	io_result assemble_send(const shared_memory& memory, const udpaddr& dest);

	/*
		����һ���Ѿ�װ���ͷ��������
	*/
	io_result send_assembled(const shared_memory& memory, const udpaddr& dest);

	/*
		��������,ͷ��װ�����ڲ�����
	*/
	io_result send(const shared_memory&, const udpaddr&);

	/*
		�������ݲ�����,�������޶����ܵ�ַ
	*/
	recv_result recv();

	typedef int raw_socket;
	
	/*
		�ں�socket
	*/
	raw_socket native() const;
private:
	std::shared_ptr<trans_core_impl> _impl;

	static size_t trans_core_reserve_size;
	static size_t maxinum_slice_data_size;
};
#endif