#ifndef ISU_ARCHIVE_SIZE_HPP
#define ISU_ARCHIVE_SIZE_HPP

#include <archive_def.hpp>

template<class T>
size_t archived_size(const T& var)
{
	return archived_size<T>();
}
template<class T>
size_t archived_size()
{
	return archived_size<T>(typename detail_of_type<T>::type());
}

//还没有支持
template<class T>
size_t archived_size(reference_type)
{
	return sizeof(typename std::remove_reference<T>::type);
}
template<class T>
size_t archived_size(const T& var, multiple_type);
template<class T>
size_t archived_size(const T& var, diamond_type);//菱形继承的类型
//

template<class T>
size_t archived_size(nop_type)
{
	return 0;
}

template<class T>
size_t archived_size(const T& var, nop_type tp)//空类型
{
	return archived_size<T>(tp);
}

template<class T>
size_t archived_size(pod_type)
{
	return sizeof(T);
}
template<class T>
size_t archived_size(const T& var, pod_type tp)//POD类型以及内置标量
{
	return archived_size<T>(tp);
}

template<class T>
size_t archived_size(normal_type)
{
	return sizeof(T);
}
template<class T>
size_t archived_size(const T var, normal_type)
{
	return archived_size<T>(normal_type);
}

//
inline size_t archived_size()
{
	return 0;
}

#define ITER_VALUE_TYPE(tp)\
	typedef typename iterator_value_type <\
		typename iter_wapper<tp>::type > ::type value_type;

template<class T1, class Iter>
size_t archived_vector_size_impl(const T1& size, Iter iter, std::true_type)
{
	ITER_VALUE_TYPE(Iter);
	return size*archived_size<value_type>();
}

template<class T1, class Iter>
size_t archived_vector_size_impl(const T1& size, Iter iter, std::false_type)
{
	size_t result = 0;
	for (T1 index = 0; index != size; ++index)
	{
		result += archived_size(*iter);
	}
	return result;
}

template<class T1, class Iter>
size_t archived_vector_size(const T1& size, Iter iter)
{
	ITER_VALUE_TYPE(Iter);
	return archived_vector_size_impl(size, iter,
		std::integral_constant<bool, constant_length<value_type>::value>());
}

template<class T1, class T2, class... Arg>
size_t archived_size_impl(const vector_air& air,
	const T1& a1, const T2& a2, const Arg&... arg)
{//a1是size,a2是迭代器
	ITER_VALUE_TYPE(T2);
	return
		archived_size(a1) +
		archived_vector_size(a1, a2) +
		archived_size(arg...);
}

inline size_t archived_size_impl()
{
	return 0;
}

template<class T,class... Arg>
size_t archived_size_impl(const T& var, const Arg&... arg)
{
	return archived_size(var) + archived_size_impl(arg...);
}

template<class... Arg>
size_t archived_size(const Arg&... arg)
{
	return archived_size_impl(arg...);
}

template<class T, class... Arg>
std::size_t _count_size(T a1, const Arg&... arg)
{
	return archived_size(a1, arg...);
}

#endif