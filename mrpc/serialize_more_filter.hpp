#ifndef ISU_SERIALIZE_MORE_FILTER_HPP
#define ISU_SERIALIZE_MORE_FILTER_HPP

//更多的用于serialize_object的filter元对象

//当对象的RealType不满足条件则返回false

#include <serialize.hpp>
/*

MetaCondition:元函数,type和value必须是std::integral_constant<bool,v>和const bool

PreLayerFilter=由于条件可以和具体的filter分离开来,所以提供了
这个模板参数用于支持更多的filter

*/

template<class MetaCondition,class PreLayerFilter=nop_filter>
struct condition_filter
{
private:
	
#define CONDITION_CALL_FILTER(name)\
	SERALIZE_OBJECT_TEPMLATE\
	static auto _call_impl_##name(T& value,std::true_type)\
		->decltype(PreLayerFilter::name##_filter SERALIZE_TEMPLATE_USE(value))\
	{\
		return PreLayerFilter::name##_filter SERALIZE_TEMPLATE_USE(value);\
	}
	
#define CONDITION_CALL_FILTER_FALSE(name)\
	SERALIZE_OBJECT_TEPMLATE\
	static std::pair<bool,T*> _call_impl_##name(T& value, std::false_type)\
	{\
		return std::make_pair(false,nullptr);\
	}

	CONDITION_CALL_FILTER(archive)
	CONDITION_CALL_FILTER(rarchive)
	CONDITION_CALL_FILTER_FALSE(archive)
	CONDITION_CALL_FILTER_FALSE(rarchive)
public:
#define CONDITION_FILTER_CALL(name,v) _call_impl_##name SERALIZE_TEMPLATE_USE \
	(value,MetaCondition::functional<RealT,v>::type())

	SERALIZE_OBJECT_TEPMLATE
	static auto archive_filter(T& value)
	->decltype(CONDITION_FILTER_CALL(archive,true))
	{
		return CONDITION_FILTER_CALL(archive,true);
	}

	SERALIZE_OBJECT_TEPMLATE
	static auto rarchive_filter(T& value)
	->decltype(CONDITION_FILTER_CALL(rarchive,false))
	{
		return CONDITION_FILTER_CALL(rarchive,false);
	}
};

struct is_archiveable
{//是否允许序列化,具体实现看下面的元函数
	template<class T,bool is_archive>
	struct functional
	{
		static const bool value = is_archive||!is_no_need_copy<T>::value;
		typedef std::integral_constant<bool, value> type;
	};
};


struct is_rarchiveable
	:public meta_functional
{
	template<class T, bool is_archive>
	struct functional
	{
		typedef typename is_archiveable::functional<T, !is_archive>::type type;
		static const bool value = type::value;
	};
	template<class T>
	struct functional < T, false >
	{
		typedef std::true_type type;
		static const bool value = true;
	};
};
#endif