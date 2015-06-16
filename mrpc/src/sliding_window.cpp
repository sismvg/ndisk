
#include <windows.h>
#include <sliding_window.hpp>
//#include <boost/date_time/posix_time/ptime.hpp>

#define SLIDING_WINDOW_INIT(val)\
	_enabled(val),_sem(new impl(val)),_state(exit_because_post)
sliding_window::sliding_window(size_t enable_slice)
	:SLIDING_WINDOW_INIT(enable_slice)
{

}

sliding_window::sliding_window(size_t enable_slice,size_t slice_size,
	const_shared_memory data, user_head_genericer gen)
	: SLIDING_WINDOW_INIT(enable_slice)
{

}

void sliding_window::close()
{
	InterlockedExchange(&_state, exit_because_destruct);
}

void sliding_window::post(size_t extern_slice,size_t why)
{
	(*_sem) += extern_slice;
	/*
	for (size_t index = 0; index != extern_slice; ++index)
	{
		_sem->post();
	}
	InterlockedAdd(&_enabled, extern_slice);
	*/
}

size_t sliding_window::wait(size_t use)
{
	auto& blance = (*_sem);
	for (unsigned int index = 0; index != use; ++index)
	{
		size_t loop = 0;
		while (blance == 0)
		{
			boost::detail::yield(1);
			++loop;
		}
		--blance;
	}
	/*
	while (_sem)
	for (size_t index = 0; index != use; ++index)
	{
		_sem->wait();
		//检测一下exit_by的问题
	}
	*/
	InterlockedAdd(&_enabled,-static_cast<int>(use));
	return _state;
}

void sliding_window::timed_wait(size_t ms)
{
	//TODO:没有完成
}

size_t sliding_window::try_wait(size_t use)
{
	/*
	size_t ret = use;
	for (size_t index = 0; index != use; ++index)
	{
		if (!_sem->try_wait())
			ret = index;
	}
	_enabled -= ret;
	*/
//	return ret;
	return 0;
}

size_t sliding_window::enabled_slice() const
{
	return _enabled;
}

void sliding_window::narrow_slice_window(size_t slice_count)
{
	wait(slice_count);
}

void sliding_window::narrow_stream_window(size_t bit)
{

}

void sliding_window::expand_slice_window(size_t slice_count)
{
	post(slice_count);
}

void sliding_window::expand_stream_window(size_t bit)
{

}
