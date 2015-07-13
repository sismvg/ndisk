#ifndef ISU_TRANS_CORE_HPP
#define ISU_TRANS_CORE_HPP

#include <socket.hpp>
#include <memory_helper.hpp>
extern std::atomic_size_t retry;
class trans_core_impl;

typedef unsigned long package_id_type;



enum trans_error_code{
	//给头部预留的内存不足
	not_enough_for_head = 1,
	//传输的线程退出
	recv_thread_exit,
	//内核socket出错
	has_system_socket_error,
	//内存不足
	memory_not_enough,
	//遇到非内存不足以外的C++异常
	have_cpp_exception,
	//重传多次过后,包还是没有传输成功
	send_faild,
};

struct io_result
{//BE RULE3:基本上会被inline的函数允许写在头文件中
	typedef int error_code;
	typedef void* io_mission_handle;
	/*
	构造实现相关,不要调用
	*/
	io_result()
		:_code(0), _system_error(0), _id(nullptr)
	{}

	io_result(io_mission_handle id,
		error_code local_code = 0, error_code system_code = 0)
		:_id(id), _code(local_code), _system_error(system_code)
	{}

	/*
	I/O操作是否成功进行
	*/
	inline bool in_vaild() const
	{
		return _code == 0 && _system_error == 0;
	}

	/*
	trans_core的I/O操作发生了什么错误
	*/
	inline error_code what() const
	{
		return _code;
	}

	/*
	内核socket发生了什么错误
	*/
	inline error_code system_socket_error() const
	{
		return _system_error;
	}

	/*
	传输的数据的标识
	*/
	inline io_mission_handle id() const
	{
		return _id;
	}
private:
	error_code _code;
	error_code _system_error;
	io_mission_handle _id;
};

#define WORK_DEFAULT 0x0
#define POST_SEND_COMPLETE 0x1

#define SEND_COMPLETE 0x0
#define FAILED_SEND_COMPLETE 0x1
#define RECV_COMPLETE 0x10
#define EXIT_COMPLETE 0x100

struct complete_io_result
{
public:
	typedef std::size_t size_t;
	typedef char bit;
	typedef const char const_bit;
	typedef bit* iterator;
	typedef const_bit* const_iterator;
	typedef void* io_mission_handle;
	typedef complete_io_result self_type;

	typedef shared_memory_block<const char> const_shared_memory;
	typedef shared_memory_block<char> shared_memory;

	/*
		构造,实现相关
	*/
	complete_io_result();
	complete_io_result(const io_result&, const core_sockaddr& from,size_t io_type,
		const shared_memory& memory, size_t size);

	/*
		谁发送过来的
	*/
	const core_sockaddr& from() const;

	/*
		进行I/O操作时的I/O状态
	*/
	const io_result& get_io_result() const;

	/*
		有效数据区间,这个区间不包含头部
	*/
	iterator begin();
	iterator end();
	const_iterator begin() const;
	const_iterator end() const;

	/*
		udp传输中的头部预留
	*/
	size_t reserve();

	/*
		完成io的类型
	*/
	size_t io_type() const;
	/*
		接收到的包的id
	*/
	io_mission_handle id() const;

	/*
		包含头部的原始内存
	*/
	shared_memory raw_memory();
	const_shared_memory raw_memory() const;
private:
	size_t _io_type;
	io_result _io_error;
	core_sockaddr _from;
	shared_memory _memory;
	size_t _size;
	iterator _begin();
	const_iterator _begin() const;
};

typedef complete_io_result recv_result;
#define DEFAULT_ALLOCATOR std::allocator<char>


struct retrans_control
{
	typedef std::size_t size_t;
	retrans_control(size_t first_time = 64, size_t retry = 15)
		:first_timeout(first_time), retry_limit(retry)
	{
	}
	size_t first_timeout, retry_limit;
};


class trans_core
{
	//不保证顺序发送
	//在超过重传次数以前,不会丢包
public:
	/*
		trans_core_impl预留的头部长度
	*/
	static size_t trans_core_reserve();
	/*
		一个数据分片最高可以有多长(不包括头部)
	*/
	static size_t maxinum_slice_size();

	typedef shared_memory_block<const char> const_shared_memory;
	typedef shared_memory_block<char> shared_memory;

	typedef sockaddr_in udpaddr;
	typedef void* io_mission_handle;
	typedef std::function < void(const core_sockaddr&,
		const const_shared_memory&, io_mission_handle) > bad_send_functional;
	/*
		本地地址
		limit_thread:用于处理recv的返回值的最大线程数量
		可以当成windows的I/O完成端口使用
	*/
	trans_core()
	{}
	trans_core(const udpaddr&, size_t limit_thread = 1);
	trans_core(const udpaddr&,
		bad_send_functional bad_send, size_t limit_thread = 1);
	//发送方必须预留trans_core_reserve个字节的头部

	/*
		装配要发送的数据的头部
		如果其memory.size()小于预留的头部长度
		io_resul的waht会返回not_enough_for_head
	*/
	io_result assemble_send(const shared_memory& memory,
		const udpaddr& dest, const retrans_control* ctrl_ptr = nullptr);

	/*
		发送一块已经装配好头部的数据
	*/
	io_result send_assembled(const shared_memory& memory, const udpaddr& dest);

	/*
		发送数据,头部装配由内部进行
	*/
	io_result send(const shared_memory&, const udpaddr&,
		const retrans_control* ctrl_ptr = nullptr);

	/*
		接受数据并阻塞,不允许限定接受地址
	*/
	recv_result recv();

	void set_flag(size_t flag);

	void close();

	bool cancel_io(const io_mission_handle& handle, bool active_io_faild);

	typedef int raw_socket;
	
	/*
		内核socket
	*/
	raw_socket native() const;
private:
	std::shared_ptr<trans_core_impl> _impl;

	static size_t trans_core_reserve_size;
	static size_t maxinum_slice_data_size;
};
#endif