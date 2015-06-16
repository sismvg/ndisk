#ifndef ISU_IO_COMPLETE_PORT
#define ISU_IO_COMPLETE_PORT

#include <platform_config.hpp>

#ifdef _WINDOWS
#include <windows.h>
#else
#endif

struct complete_result
{
	complete_result()
		:key(0), user_key(0), argument(nullptr)
	{}

	complete_result(size_t ckey,
		size_t cuser_key, sysptr_t cargument)
		:key(ckey), user_key(cuser_key), argument(cargument)
	{}

	size_t key;
	size_t user_key;
	sysptr_t argument;
};

class io_complete_port
{
public:
	typedef unsigned long size_t;
	typedef unsigned long long ulong64;
#ifdef _WINDOWS
	typedef HANDLE kernel_handle;
#else
#endif

	enum complete_port_msg{ complete_port_exit = 1 };
	/*
		limit_thread指定处理线程池的数量
	*/
	io_complete_port(size_t limit_thread = 1);
	io_complete_port(kernel_handle first_bind, size_t limit_thread = 1);
	~io_complete_port();

	/*
		平台相关
	*/
	kernel_handle native() const;

	/*
		绑定到某个设备上,该设备上的所有I/O完成消息都会被
		post接受到
		平台相关的
	*/
	void bind(kernel_handle);

	/*
		post数据,会被某个get中的线程获取
		返回值不为0时成功
		为0时失效
	*/
	size_t post(size_t key, sysptr_t);
	
	/*
		同上
	*/
	size_t post(complete_result);

	/*
		关闭完成端口
	*/
	void close();
	/*
		get数据,如果队列中没有数据则会阻塞
			也可以使用timeout参数设定超时时间,单位为毫秒
		-1表示永远(相对的)
	*/
	complete_result get(size_t timeout = -1);

	/*
	未实现
	void set_dynamic_blance();
	*/
private:
	size_t _limit_thread;
#ifdef _WINDOWS
	std::atomic_size_t _complete_port;
	void _set_handle_value(kernel_handle);
	void _init(kernel_handle, size_t);
#else
#endif
};

#endif