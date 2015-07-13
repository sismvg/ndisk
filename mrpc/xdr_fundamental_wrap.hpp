#ifndef ISU_XDR_FUNDAMENTAL_WRAP_HPP
#define ISU_XDR_FUNDAMENTAL_WRAP_HPP

#include <iterator>
#include <xdr_convert.hpp>
//算数类型的xdr包装,保证序列化时按照XDR协议来

//type->小于XDR_MININUM_ALIGN,大于等于XDR_MININUM_ALIGN
//小于的直接用XDR_MININUM_ALIGN_TYPE
//大于等于有两种,一种刚好对齐,一种没有,对齐的直接typedef T type;
//没有的用一个struct type,然后塞入less个字节填充

//继承包装,因为不能继承一个算数类型,所以需要
template<class T, bool v>
struct fundamental_warp
{
	T value;
	typedef T source_type;
	source_type& get()
	{
		return value;
	}

	const source_type& get() const
	{
		return value;
	}

	void set(const source_type& v)
	{
		get() = v;
	}
	source_type& set_get(const source_type& v)
	{
		set(v);
		return get();
	}
};

template<class T>
struct fundamental_warp<T,false>
	:public T
{
	typedef T source_type;
	source_type& get()
	{
		return *this;
	}

	const source_type& get() const
	{
		return *this;
	}

	void set(const source_type& v)
	{
		get() = v;
	}
	source_type& set_get(const source_type& v)
	{
		set(v);
		return get();
	}
};

template<size_t size>
struct static_align
{
	static const size_t value =
		(size + XDR_MININUM_ALIGN - 1) & (~(XDR_MININUM_ALIGN - 1));
};

template<class T>
struct fundamental_father_type
{
	typedef fundamental_warp < T,
		std::is_fundamental<T>::value
		||
		std::is_enum<T>::value > type;//不能继承一个enum
};

template<class T,size_t less>
struct mininum_fundamental_impl_2
	:public fundamental_father_type<T>::type
{
	typedef char bit;
	bit zero[less];

public:
	typedef T source_type;

	mininum_fundamental_impl_2()
	{
		memset(zero, 0, less);
	}

	source_type& get()
	{
		return *this;
	}

	const source_type& get() const
	{
		return *this;
	}

	void set(const source_type& v)
	{
		get() = v;
	}

	source_type& set_get(const source_type& v)
	{
		set(v);
		return get();
	}
};


template<class T>
struct mininum_fundamental_impl_2 < T, 0 >
	:public fundamental_father_type<T>::type
{
	typedef T type;
	typedef T source_type;
public:
};

template<class T,bool v>
struct mininum_fundamental_impl
{
	typedef typename mininum_fundamental_impl_2 < T,
		static_align<sizeof(T)>::value - sizeof(T) > type;
};

template<class T>
struct mininum_fundamental_impl <T, true >
{
	typedef fundamental_warp<XDR_MININUM_ALIGN_TYPE,true> type;
};

template<class T>
struct mininum_fundamental
{
	typedef typename mininum_fundamental_impl < T,
		sizeof(T) < XDR_MININUM_ALIGN > ::type type;
};

template<class T>
class xdr_fundamental_wrap_normal
	:public mininum_fundamental<T>::type
{
public:
	typedef T source_type;
	typedef typename mininum_fundamental<T>::type value_type;

};

template<class T>
class xdr_fundamental_wrap_pointer
	:public xdr_fundamental_wrap_normal<T>
{//给指针用的
public:

	typedef T source_type;
	typedef typename mininum_fundamental<T>::type value_type;

	auto operator ->()
		->decltype(*get())
	{
		return *get();
	}

	auto operator ->() const
		->decltype(*get())
	{
		return *get();
	}

	template<class T>
	auto operator[](const xdr_fundamental_wrap_normal<T>& v)
		->decltype(get()[v])
	{
		return get()[v];
	}

	template<class T>
	auto operator[](const xdr_fundamental_wrap_normal<T>& v) const
		->decltype(get()[v])
	{
		return get()[v];
	}
};

template<class T,class Sign>
struct xdr_get_type
{
	typedef T type;
};

template<class T>
struct xdr_get_type < T,std::true_type >
{
	typedef xdr_fundamental_wrap_pointer<T> type;
};

template<class T>
struct xdr_get_type < T,std::false_type >
{
	typedef xdr_fundamental_wrap_normal<T> type;
};

#define TWO_OP_RET(call,cv,type)\
	template<class T>\
	friend cv type operator call(cv xdr_fundamental_wrap<T>& lhs, \
		const xdr_fundamental_wrap<T>& rhs)\

#define TWO_OP_SELF(call,cv)\
	template<class T>\
	friend cv xdr_fundamental_wrap<T>& operator call(cv xdr_fundamental_wrap<T>& lhs, \
		const xdr_fundamental_wrap<T>& rhs)\

#define TWO_OP_RET_DEF(call,cv,type)\
	template<class T>\
	cv type operator call(cv xdr_fundamental_wrap<T>& lhs, \
		const xdr_fundamental_wrap<T>& rhs)\
	{\
		return lhs.get() call rhs.get(); \
	}

#define TWO_OP_SELF_DEF(call,cv)\
	template<class T>\
	cv xdr_fundamental_wrap<T>& operator call(cv xdr_fundamental_wrap<T>& lhs, \
		const xdr_fundamental_wrap<T>& rhs)\
	{\
		lhs.get() call rhs.get();\
		return lhs;\
	}

template<class T>
class xdr_fundamental_wrap
	:public xdr_get_type<T,typename std::is_pointer<T>::type>::type
{
public:

	typedef T source_type;
	typedef xdr_fundamental_wrap<T> self_type;

	xdr_fundamental_wrap()
	{
	}

	template<class T>
	xdr_fundamental_wrap(const xdr_fundamental_wrap<T>& rhs)
	{
		set(rhs.get());
	}

	template<class T>
	xdr_fundamental_wrap(const T& rhs)
	{
		get() = source_type(rhs);
	}

	xdr_fundamental_wrap(const source_type& v)
	{
		_init(v);
	}
	xdr_fundamental_wrap(const self_type& v)
	{
		set(v);
	}

	operator source_type&()
	{
		return get();
	}
	operator const source_type&() const
	{
		return get();
	}


	//一元操作

	template<class T>
	self_type& operator=(const xdr_fundamental_wrap<T>& rhs)
	{
		return get() = rhs.get();
	}

	self_type& operator++()
	{
		++get();
		return *this;
	}

	self_type operator++(int)
	{
		self_type tmp = *this;
		++get();
		return tmp;
	}
	self_type& operator--()
	{
		--get();
		return *this;
	}
	self_type& operator--(int)
	{
		self_type tmp = *this;
		--get();
		return tmp;
	}
	
	TWO_OP_RET(> , const, bool);
	TWO_OP_RET(< , const, bool);
	TWO_OP_RET(!= , const, bool);
	TWO_OP_RET(== , const, bool);
		
	TWO_OP_RET(>= , const, bool);
	TWO_OP_RET(<= , const, bool);

	TWO_OP_SELF(+= , );
	TWO_OP_SELF(-= , );
	TWO_OP_SELF(*= , );
	TWO_OP_SELF(/= , );
	TWO_OP_SELF(%= , );

	TWO_OP_SELF(>>= , );
	TWO_OP_SELF(<<= , );

	TWO_OP_SELF(|= , );
	TWO_OP_SELF(&= , );
	TWO_OP_SELF(^= , );

	//
	TWO_OP_SELF(+, const);
	TWO_OP_SELF(-, const);
	TWO_OP_SELF(*, const);
	TWO_OP_SELF(/ , const);
	TWO_OP_SELF(%, const);

	TWO_OP_SELF(>> , const);
	TWO_OP_SELF(<< , const);

	TWO_OP_SELF(| , const);
	TWO_OP_SELF(&, const);
	TWO_OP_SELF(^, const);

	bool operator !() const
	{
		return !get();
	}

	self_type operator ~() const
	{
		return ~get();
	}
private:

	void _init(const source_type& v)
	{
		set(v);
	}
	
};

TWO_OP_RET_DEF(> , const, bool)
TWO_OP_RET_DEF(< , const, bool)
TWO_OP_RET_DEF(!= , const, bool)
TWO_OP_RET_DEF(== , const, bool)

TWO_OP_RET_DEF(>= , const, bool)
TWO_OP_RET_DEF(<= , const, bool)

TWO_OP_SELF_DEF(+= , )
TWO_OP_SELF_DEF(-= , )
TWO_OP_SELF_DEF(*= , )
TWO_OP_SELF_DEF(/= , )
TWO_OP_SELF_DEF(%= , )

TWO_OP_SELF_DEF(>>= , )
TWO_OP_SELF_DEF(<<= , )

TWO_OP_SELF_DEF(|= , )
TWO_OP_SELF_DEF(&= , )
TWO_OP_SELF_DEF(^= , )

//
TWO_OP_SELF_DEF(+, const)
TWO_OP_SELF_DEF(-, const)
TWO_OP_SELF_DEF(*, const)
TWO_OP_SELF_DEF(/ , const)
TWO_OP_SELF_DEF(%, const)

TWO_OP_SELF_DEF(>> , const)
TWO_OP_SELF_DEF(<< , const)

TWO_OP_SELF_DEF(| , const)
TWO_OP_SELF_DEF(&, const)
TWO_OP_SELF_DEF(^, const)

template<class T>
struct is_fixed_length_data < xdr_fundamental_wrap<T> >
{
	typedef typename is_fixed_length_data<T>::type type;
	static const bool value = type::value;
};

//一些模板特化

namespace std
{
	template<class T>
	struct is_pod < xdr_fundamental_wrap<T> >
	{
		typedef typename is_pod<T>::type type;
		static const bool value = type::value;
	};
}
#endif
