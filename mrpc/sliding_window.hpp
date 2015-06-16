#ifndef ISU_SLIDING_WINDOW_HPP
#define ISU_SLIDING_WINDOW_HPP

#include <atomic>
#include <slice.hpp>
#include <ackbitmap.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>

//负责数据的分片,窗口调整,

const size_t exit_because_destruct = 1;
const size_t exit_because_post = 0;

class sliding_window
{//udp按分片的数据
public:
	typedef char bit;
	typedef shared_memory_block<bit> shared_memory;
	typedef shared_memory_block<const bit> const_shared_memory;
	typedef slicer::user_head_genericer user_head_genericer;
	typedef slicer::value_type value_type;
	/*
		开始就能指定分片的数量
	*/
	sliding_window(size_t enable_slice = 0);

	/*
		把窗口绑定到指定数据上
	*/
	sliding_window(size_t enable_slice,size_t slice_size,
		const_shared_memory data, user_head_genericer gen);

	/*
		数据
	*/
	shared_memory data();
	const_shared_memory data() const;

	/*
		要退出了
	*/
	void close();
	/*
		对方允许多少片
	*/
	void post(size_t extern_slice = 1, size_t why = exit_because_post);

	/*
		这个wait需要用掉多少片
	*/
	size_t wait(size_t use = 1);

	/*
		尝试获取分片,返回可用分片数量
		并且占用这些分片.
	*/
	size_t try_wait(size_t use=1);

	/*
		带有超时功能罢了
	*/
	void timed_wait(size_t ms);

	/*
		现在还有多少片没有用
	*/
	size_t enabled_slice() const;

	/*
		获取一片数据,窗口不够会卡住,max指定最大bit数量
		为0表示自动调整
	*/
	/*
		缩小窗口
	*/
	void narrow_slice_window(size_t slice_count);

	/*
		缩小窗口，但是是对字节流的
	*/
	void narrow_stream_window(size_t bit);

	/*
		上面的反过来
	*/

	void expand_slice_window(size_t slice_count);
	void expand_stream_window(size_t bit);
	//

private:
	//why ,why atomic_size_t not support copy construct?
	volatile long _enabled;
	volatile long stream_enabled;
	volatile long _state;
//	typedef boost::interprocess::interprocess_semaphore impl;
	//WARNING:这只是实现的妥协
	//std::shared_ptr<impl> _sem;
	typedef std::atomic_size_t impl;
	std::shared_ptr<std::atomic_size_t> _sem;
};
#endif