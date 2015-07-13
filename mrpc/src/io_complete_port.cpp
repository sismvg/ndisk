
#include <stdexcept>
#include <exception>

#include <rpcdef.hpp>

#include <io_complete_port.hpp>

io_complete_port::io_complete_port(size_t limit_thread)
{
	_init(nullptr, limit_thread);
}

io_complete_port::io_complete_port(
	kernel_handle first_bind, size_t limit_thread)
{
	_init(first_bind, limit_thread);
}

io_complete_port::~io_complete_port()
{
#ifdef _WINDOWS
	if (native() != INVALID_HANDLE_VALUE)
		CloseHandle(native());
#else
#endif
}

io_complete_port::kernel_handle 
	io_complete_port::native() const
{
	size_t ret = _complete_port;
	return reinterpret_cast<kernel_handle>(ret);
}

void io_complete_port::bind(kernel_handle object)
{
#ifdef _WINDOWS
	CreateIoCompletionPort(
		object, native(), NULL, _limit_thread);
#else
#endif
}

void io_complete_port::close()
{
	if (native() != INVALID_HANDLE_VALUE)
	{
		CloseHandle(native());
		_set_handle_value(INVALID_HANDLE_VALUE);
	}
}

io_complete_port::size_t 
	io_complete_port::post(size_t key, sysptr_t argument)
{
	complete_result arg;
	arg.argument = argument;
	arg.key = key;
	arg.user_key = key;
	return post(arg);
}

io_complete_port::size_t io_complete_port::post(complete_result arg)
{

#ifdef _WINDOWS
	OVERLAPPED* lapped = new OVERLAPPED;
	ZeroMemory(lapped, sizeof(OVERLAPPED));

	lapped->hEvent = new complete_result(arg);

	size_t ret = PostQueuedCompletionStatus(
		native(), 0, arg.key, lapped);
	if (GetLastError() != 0)
	{
		mylog.log(log_error,
			"PostQueued failed:", GetLastError());
	}
	return ret;
#else
#endif
}

complete_result io_complete_port::get(size_t timeout)
{
#ifdef _WINDOWS
	DWORD word = 0;
	ULONG_PTR key = 0;

	OVERLAPPED* lapped = nullptr;
	BOOL accepted = GetQueuedCompletionStatus(
		native(), &word, &key, &lapped, timeout == -1 ? INFINITE : timeout);
	
	DWORD last_error = GetLastError();
	complete_result arg;
	if (accepted)
	{
		auto* ptr = reinterpret_cast<complete_result*>(lapped->hEvent);
		arg = *ptr;
		delete ptr;
	}
	else
	{
		//do some thing
		arg.key = complete_port_exit;
		arg.argument = nullptr;
	}
#else
#endif
	return arg;
}

void io_complete_port::_init(kernel_handle port, size_t limit)
{
	if (limit == 0)
	{
		throw std::runtime_error("");
	}
	_limit_thread = limit;
	_set_handle_value(CreateIoCompletionPort(
		port == nullptr ? INVALID_HANDLE_VALUE : port, NULL,
		0, limit));
	if (native() == INVALID_HANDLE_VALUE)
	{
		throw std::runtime_error("");
	}
}

void io_complete_port::_set_handle_value(kernel_handle val)
{
	_complete_port = reinterpret_cast<size_t>(val);
}