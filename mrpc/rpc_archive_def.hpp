#ifndef RPC_ARCHIVE_DEF_HPP
#define RPC_ARCHIVE_DEF_HPP

#include <type_traits>

template<class T>
struct is_no_need_copy
{
	typedef typename std::remove_cv<T>::type raw_type;
	typedef typename
		std::remove_pointer < typename
		std::remove_reference<T>::type > ::type cvtype;
	static const bool is_consted = std::is_const<cvtype>::value;
	static const bool is_local_var =
		!std::is_reference<raw_type>::value&&
		!std::is_pointer < raw_type >::value;
	static const bool value = is_consted || is_local_var;
	typedef std::integral_constant<bool, value> type;
};

#endif