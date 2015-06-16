#ifndef ISU_MPL_HELPER_HPP
#define ISU_MPL_HELPER_HPP

#include <functional>
#include <type_traits>

template<class Sptr, class T, class R, class... Arg>
auto mem_fn_object(Sptr ptr, R(T::*mem_ptr)(Arg...))
->std::function < R(Arg...) >
{
	return[=](Arg... arg)
	{
		return ((*ptr).*mem_ptr)(arg...);
	};
}

#define MEMBER_BIND(obj,fn) (mem_fn_object(obj,\
	(&std::remove_reference<decltype(*obj)>::type::fn)))

#define MEMBER_BIND_IN(fn) MEMBER_BIND(this,fn)

#endif