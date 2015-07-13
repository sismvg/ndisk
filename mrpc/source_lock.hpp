#ifndef ISU_SOURCE_LOCK_HPP
#define ISU_SOUCE_LCOK_HPP

#include <rpclock.hpp>
#include <functional>

#include <io_complete_port.hpp>

template<class T>
class producers_source
{//必须保证producer还活着才能用
public:
	//不允许手动release
	producers_source()
		:_val(nullptr), _port(nullptr)
	{

	}
	producers_source(T* value, io_complete_port* port)
		:_val(value), _port(port)
	{

	}

	~producers_source()
	{
		complete_result arg(0, 0, _val);
		_port->post(arg);
	}

	T& value()
	{
		return *_val;
	}
	const T value() const
	{
		return *_val;
	}

	T* operator->()
	{
		return _val;
	}

	const T* operator->() const
	{
		return _val;
	}
private:
	T* _val;
	io_complete_port* _port;
};

template<class T,class Lock=rwlock>
class producers
{
public:
	typedef std::size_t source_id;
	//给出当前资源表示符
	typedef std::function<T(source_id)> genericer;
	typedef producers_source<T, Lock> proxy;
	typedef producers_source<const T, Lock> const_proxy;
	typedef std::shared_ptr<proxy> get_result_type;
	/*
		reserve:允许创建的元素的最大数量
		gen:用户用它来生成数据
	*/
	producers()
		:_size(0), _cap(0), _port(0)
	{}

	producers(size_t reserve)
		//标准来说用cpu核心数的两倍的线程
		:_cap(reserve), _size(0), _port(cpu_core_count()*2)
	{

	}
	~producers()
	{
		std::for_each(_values.begin(), _values.end(),
			[&](T* ptr)
		{
			delete ptr;
		});
	}
	/*
		获取数据,proxy指向数据,并负责在析构时把数据还回来
	*/
	std::shared_ptr<proxy> get()
	{
		if (cap() < size())
		{
			T val = genericer(size() + 1);
			T* ptr = new T(val);
			_values.push_back(ptr);
			++_size;
			//嗯,不能直接返回,因为可能有别的线程在很早
			//以前就等待了,直接返回会有极大的可能造成饥饿
			_pass(ptr);
		}

		auto result = _port.get();
		if (result.key != 0)
			;//do
		return std::shared_ptr(new proxy(
			reinterpret_cast<T*>(result.argument), &_port));
	}

	//
	/*
		当前已经创建的元素的数量
	*/
	size_t size() const
	{
		return _size;
	}
	
	/*
		做多能创建多少个元素
	*/
	size_t cap() const
	{
		return _cap;
	}

	/*
		减少元素的最大值.这会导致不可预料的析构
		但是并不是立即析构,必须当有线程release了一些数据后
	*/
	void narrow(size_t narr)
	{
		
	}

	/*
		增加元素的最大值.会唤醒因为get而卡住的线程
	*/
	void extent(size_t ex)
	{
		_cap += ex;
	}

	/*
		立即生成所有数据
	*/
	void generic_all()
	{
		for (size_t index = 0; index != _cap; ++index)
			_gen();
	}

private:
	std::atomic_size_t	_cap, _size;
	genericer _gen;
	io_complete_port _port;
	//只是用来在析构时清理数据的
	std::vector<T*> _values;

	friend class producers_source < T, Lock > ;
	void _pass(T* ptr)
	{
		complete_result arg(0, 0, ptr);
		_port.post(arg);
	}

};
#endif