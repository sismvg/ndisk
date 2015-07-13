#ifndef ISU_TYPE_LIST_HPP
#define ISU_TYPE_LIST_HPP

/*
 按照MetaFunction定义的函数来静态过滤Arg,type为type_list<filter args>
*/
#include <boost/mpl/if.hpp>

template<class... Arg>
struct type_list
{//一个空类,用于在模板变参中传输类型
	//template<class... ArgList1,class... ArgList2>
	//void fun(type_list<ArgList1...>,ArgList2...)
};

/*
template<class MetaFunction,class T,class... Arg>
struct filter_type_list_impl
{
	
	typedef typename boost::mpl::if_ < MetaFunction::functional<T>::type,
		filter_type_list_impl<MetaFunction, 
			type_list<filtered_args..., T>, Arg...>,
		filter_type_list_impl < MetaFunction, 
			type_list<filtered_args...>, Arg... >> ::type type;


};

template<class MetaFunction,class... Arg>
struct filter_type_list
{
	typedef typename 
		filter_type_list_impl<MetaFunction, Arg...>::type type;
};*/

//call_with_filter的Maker并不关心Maker::functional<T>返回什么类型的对象
//只要他能通过给Func传入参数时的编译即可

struct meta_functional
{};

template<class T>
struct is_meta_functional
{
	typedef typename std::is_base_of<meta_functional, T>::type type;
};

template<class Func,class... Parments>
void _call_impl(Func& fn, Parments&&... parments)
{
	fn(parments...);
}

template<class MetaFunction,class Maker, class Func,class... Parment>
void call_with_filter_impl(type_list<> list,Func& fn, Parment&&... parments)
{
	_call_impl(fn, parments...);
}

template<class MetaFunction,class Maker,class Func,class T,class... Arg,class... Parment>
void call_with_filter_impl(type_list<T,Arg...> list, Func& fn, Parment&&... parments)
{
	call_with_filter_impl_1<MetaFunction,Maker>(MetaFunction::functional<T>::type(),
		list, fn, parments...);
}

template<class MetaFunction,class Maker,class T,class... Arg,class Func,class... Parment>
void call_with_filter_impl_1(std::true_type,
	type_list<T, Arg...> list, Func& fn,Parment&&... parments)
{
	auto value = Maker::functional<T>();
	call_with_filter_impl_1<MetaFunction,Maker, T, Arg...>(std::false_type(),
		list, fn, parments..., value);
}

template<class MetaFunction,class Maker,class T,class... Arg,class Func,class... Parment>
void call_with_filter_impl_1(std::false_type,
	type_list<T, Arg...> list, Func& fn,Parment&&... parments)
{
	call_with_filter_impl < MetaFunction,Maker,Arg... > 
		(type_list<Arg...>(), fn, parments...);
}

template<class MetaFunction,class Maker,class T,class... Arg,class Func,class... Parment>
void call_with_filter_impl(
	type_list<T, Arg...> list, Func& fn,Parment&&... parments)
{
	//must be support default construct
	call_with_filter_impl_1 < MetaFunction,Maker,
		T, Arg... >(MetaFunction::functional<T>::type(),
		list, fn, parments...);
}

//边界
template<class MetaFunction, class Maker,class Func>
void call_with_filter_impl(type_list<> list, Func& fn)
{
	_call_impl(fn);
}

template<class MetaFunction, class Maker,class T, class... Arg, class Func>
void call_with_filter_impl_1(std::true_type,
	type_list<T, Arg...> list, Func& fn)
{
	auto value = Maker::functional<T>();
	call_with_filter_impl_1<MetaFunction,Maker, T, Arg...>(std::false_type(),
		list, fn, value);
}

template<class MetaFunction, class Maker,class T, class... Arg,class Func>
void call_with_filter_impl_1(std::false_type,
	type_list<T, Arg...> list,Func& fn)
{
	call_with_filter_impl < MetaFunction,Maker,
		Arg... > (type_list<Arg...>(), fn);
}

template<class MetaFunction, class Maker,class T, class... Arg, class Func>
void call_with_filter_impl(
	type_list<T, Arg...> list, Func& fn)
{
	//must be support default construct
	call_with_filter_impl_1 < MetaFunction,Maker,
		T, Arg... >(MetaFunction::functional<T>::type(),
		list, fn);
}
//
template<class MetaFunction,class Maker,class... Arg,class Func>
void call_with_filter(type_list<Arg...> list,Func& fn)
{
	call_with_filter_impl<MetaFunction,Maker, Arg...>(list, fn);
}
#endif