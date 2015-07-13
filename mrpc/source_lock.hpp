#ifndef ISU_SOURCE_LOCK_HPP
#define ISU_SOUCE_LCOK_HPP

#include <rpclock.hpp>
#include <functional>

#include <io_complete_port.hpp>

template<class T>
class producers_source
{//���뱣֤producer�����Ų�����
public:
	//�������ֶ�release
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
	//������ǰ��Դ��ʾ��
	typedef std::function<T(source_id)> genericer;
	typedef producers_source<T, Lock> proxy;
	typedef producers_source<const T, Lock> const_proxy;
	typedef std::shared_ptr<proxy> get_result_type;
	/*
		reserve:��������Ԫ�ص��������
		gen:�û���������������
	*/
	producers()
		:_size(0), _cap(0), _port(0)
	{}

	producers(size_t reserve)
		//��׼��˵��cpu���������������߳�
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
		��ȡ����,proxyָ������,������������ʱ�����ݻ�����
	*/
	std::shared_ptr<proxy> get()
	{
		if (cap() < size())
		{
			T val = genericer(size() + 1);
			T* ptr = new T(val);
			_values.push_back(ptr);
			++_size;
			//��,����ֱ�ӷ���,��Ϊ�����б���߳��ں���
			//��ǰ�͵ȴ���,ֱ�ӷ��ػ��м���Ŀ�����ɼ���
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
		��ǰ�Ѿ�������Ԫ�ص�����
	*/
	size_t size() const
	{
		return _size;
	}
	
	/*
		�����ܴ������ٸ�Ԫ��
	*/
	size_t cap() const
	{
		return _cap;
	}

	/*
		����Ԫ�ص����ֵ.��ᵼ�²���Ԥ�ϵ�����
		���ǲ�������������,���뵱���߳�release��һЩ���ݺ�
	*/
	void narrow(size_t narr)
	{
		
	}

	/*
		����Ԫ�ص����ֵ.�ỽ����Ϊget����ס���߳�
	*/
	void extent(size_t ex)
	{
		_cap += ex;
	}

	/*
		����������������
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
	//ֻ������������ʱ�������ݵ�
	std::vector<T*> _values;

	friend class producers_source < T, Lock > ;
	void _pass(T* ptr)
	{
		complete_result arg(0, 0, ptr);
		_port.post(arg);
	}

};
#endif