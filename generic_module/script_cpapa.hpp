#ifndef ISU_SCRIPT_CPAPA_HPP
#define ISU_SCRIPT_CPAPA_HPP

#include <type_traits>
#include <algorithm>
#include <random>

//·µ»ØÊÇ·ñ·ûºÏFunc
template<class Con,class Func>
bool compare_container(const Con& lhs, const Con& rhs,Func fn)
{
	if (lhs.size() != rhs.size())
		return false;

	auto iter = lhs.begin();
	auto rhs_iter = rhs.begin();

	for (; iter != lhs.end();++iter,++rhs_iter)
	{ 
		if (!fn(*iter, *rhs_iter))
			return false;
	}
	return true;
}

template<class LCon,class RCon>
bool equal_container(const LCon& lhs, const RCon& rhs)
{
	typedef typename 
		std::remove_reference<
			decltype(*std::begin(lhs))>::type value_type;

	return compare_container(lhs, rhs, std::equal_to<value_type>());

}

template<class T>
T random_generic_int(T start, T end)
{
	static std::random_device dev;
	static std::mt19937 mt(dev());
	std::uniform_int<T> uniform(start, end);
	return uniform(dev);
}

template<class Iter,class T>
void fill_range(Iter begin, T start, const T& end)
{
	for (; start != end; ++start, ++begin)
		*begin = start;
}

template<class Con,class Stream>
void output(const Con& value,Stream& stream)
{
	typedef decltype(*value.begin()) value_type;
	std::for_each(value.begin(), value.end(),
		[&](const value_type& val)
	{
		stream << val << endl;
	});
}

template<class Con,class Func,class Iter>
bool erase_iter_if(Con& con, Func fn, Iter& iter)
{
	bool ret = false;
	if (fn())
	{
		con.erase(iter++);
		ret = true;
	}
	else
	{
		++iter;
	}
	return ret;
}

template<class Con,class Con2,class Iter>
bool erase_iter_if_empty(Con& con, const Con2& con2, Iter& iter)
{
	return erase_iter_if(con, [&]()->bool
	{
		return con2.size() == 0;
	}, iter);
}

#endif