#ifndef ISU_SLIDING_WINDOW_HPP
#define ISU_SLIDING_WINDOW_HPP

#include <atomic>
#include <slice.hpp>
#include <ackbitmap.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>

//�������ݵķ�Ƭ,���ڵ���,

const size_t exit_because_destruct = 1;
const size_t exit_because_post = 0;

class sliding_window
{//udp����Ƭ������
public:
	typedef char bit;
	typedef shared_memory_block<bit> shared_memory;
	typedef shared_memory_block<const bit> const_shared_memory;
	typedef slicer::user_head_genericer user_head_genericer;
	typedef slicer::value_type value_type;
	/*
		��ʼ����ָ����Ƭ������
	*/
	sliding_window(size_t enable_slice = 0);

	/*
		�Ѵ��ڰ󶨵�ָ��������
	*/
	sliding_window(size_t enable_slice,size_t slice_size,
		const_shared_memory data, user_head_genericer gen);

	/*
		����
	*/
	shared_memory data();
	const_shared_memory data() const;

	/*
		Ҫ�˳���
	*/
	void close();
	/*
		�Է��������Ƭ
	*/
	void post(size_t extern_slice = 1, size_t why = exit_because_post);

	/*
		���wait��Ҫ�õ�����Ƭ
	*/
	size_t wait(size_t use = 1);

	/*
		���Ի�ȡ��Ƭ,���ؿ��÷�Ƭ����
		����ռ����Щ��Ƭ.
	*/
	size_t try_wait(size_t use=1);

	/*
		���г�ʱ���ܰ���
	*/
	void timed_wait(size_t ms);

	/*
		���ڻ��ж���Ƭû����
	*/
	size_t enabled_slice() const;

	/*
		��ȡһƬ����,���ڲ����Ῠס,maxָ�����bit����
		Ϊ0��ʾ�Զ�����
	*/
	/*
		��С����
	*/
	void narrow_slice_window(size_t slice_count);

	/*
		��С���ڣ������Ƕ��ֽ�����
	*/
	void narrow_stream_window(size_t bit);

	/*
		����ķ�����
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
	//WARNING:��ֻ��ʵ�ֵ���Э
	//std::shared_ptr<impl> _sem;
	typedef std::atomic_size_t impl;
	std::shared_ptr<std::atomic_size_t> _sem;
};
#endif