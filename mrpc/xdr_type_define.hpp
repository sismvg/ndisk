#ifndef ISU_XDR_TYPE_DEFINE_HPP
#define ISU_XDR_TYPE_DEFINE_HPP

//很不幸,宏编程在这里没用处..and 不要看他

#define XDR_DEFINE_TYPE_1(class_name,type0,name0)\
	type0 name0;\
	XDR_DEFINE_TYPE_ARCHIVE_1(friend,class_name,type0,name0)

#define XDR_DEFINE_TYPE_ARCHIVE_1(inside,class_name,type0,name0)\
	inside size_t archived_size(const class_name& value)\
	{\
		serialize<serialize_object<xdr_filter>> core;\
		size_t size=core.archived_size(value.name0);\
		return size;\
	}\
	inside size_t archive_to_with_size(\
	memory_address buffer,size_t bufsize,const class_name& value,size_t size)\
	{\
		serialize<serialize_object<xdr_filter>> core;\
		core.archived_size(value.name0);\
		return core.archive_to(buffer,bufsize,value.name0);\
	}\
	inside size_t rarchive_with_size(\
	const_memory_address buffer,size_t bufsize,class_name& value,size_t size)\
	{\
		serialize<serialize_object<xdr_filter>> core;\
		return core.rarchive(buffer,bufsize,value.name0);\
	}
//
#define XDR_DEFINE_TYPE_2(class_name,type0,name0,type1,name1)\
	type0 name0;\
	type1 name1;\
	XDR_DEFINE_TYPE_ARCHIVE_2(friend,class_name,type0,name0,type1,name1)

#define XDR_DEFINE_TYPE_ARCHIVE_2(inside,class_name,type0,name0,type1,name1)\
inside size_t archived_size(const class_name& value)\
{\
serialize<serialize_object<xdr_filter>> core; \
size_t size = core.archived_size(value.name0, value.name1); \
return size; \
}\
inside size_t archive_to_with_size(\
memory_address buffer, size_t bufsize, const class_name& value, size_t size)\
{\
serialize<serialize_object<xdr_filter>> core; \
core.archived_size(value.name0, value.name1); \
return core.archive_to(buffer, bufsize, value.name0, value.name1); \
}\
inside size_t rarchive_with_size(\
const_memory_address buffer, size_t bufsize, class_name& value, size_t size)\
{\
serialize<serialize_object<xdr_filter>> core; \
return core.rarchive(buffer, bufsize, value.name0, value.name1); \
}

//2
#define XDR_DEFINE_TYPE_3(class_name,type0,name0,type1,name1,type2,name2)\
	type0 name0;\
	type1 name1;\
	type2 name2;\
	XDR_DEFINE_TYPE_ARCHIVE_3(friend,class_name,type0,name0,type1,name1,type2,name2)

#define XDR_DEFINE_TYPE_ARCHIVE_3(inside,class_name,\
	type0,name0,type1,name1,type2,name2)\
	inline inside size_t archived_size(const class_name& value)\
	{\
		serialize<serialize_object<xdr_filter>> core;\
		size_t size=core.archived_size(value.name0,value.name1,value.name2);\
		return size;\
	}\
	inline inside size_t archive_to_with_size(\
	memory_address buffer,size_t bufsize,const class_name& value,size_t size)\
	{\
		serialize<serialize_object<xdr_filter>> core;\
		core.archived_size(value.name0,value.name1,value.name2);\
		return core.archive_to(buffer,bufsize,value.name0,value.name1,value.name2);\
	}\
	inline inside size_t rarchive_with_size(\
	const_memory_address buffer,size_t bufsize,class_name& value,size_t size)\
	{\
		serialize<serialize_object<xdr_filter>> core;\
		return core.rarchive(buffer,bufsize,value.name0,value.name1,value.name2);\
	}
//3
#define XDR_DEFINE_TYPE_4(class_name,type0,name0,\
	type1,name1,type2,name2,type3,name3)\
	type0 name0;\
	type1 name1;\
	type2 name2;\
	type3 name3;\
	XDR_DEFINE_TYPE_ARCHIVE_4(friend,class_name,\
type0,name0,type1,name1,type2,name2,type3,name3)

#define XDR_DEFINE_TYPE_ARCHIVE_4(inside,class_name,type0,name0,\
type1, name1, type2, name2, type3, name3)\
inside size_t archived_size(const class_name& value)\
{\
serialize<serialize_object<xdr_filter>> core; \
size_t size = core.archived_size(value.name0, value.name1, value.name2, value.name3); \
return size; \
}\
inside size_t archive_to_with_size(\
memory_address buffer, size_t bufsize, const class_name& value, size_t size)\
{\
serialize<serialize_object<xdr_filter>> core; \
core.archived_size(value.name0, value.name1, value.name2, value.name3); \
return core.archive_to(buffer, bufsize, value.name0, \
value.name1, value.name2, value.name3); \
}\
inside size_t rarchive_with_size(\
const_memory_address buffer, size_t bufsize, class_name& value, size_t size)\
{\
serialize<serialize_object<xdr_filter>> core; \
return core.rarchive(buffer, bufsize, value.name0, value.name1, \
value.name2, value.name3); \
}
//4
#define XDR_DEFINE_TYPE_5(class_name,type0,name0,\
	type1,name1,type2,name2,type3,name3,type4,name4)\
	type0 name0;\
	type1 name1;\
	type2 name2;\
	type3 name3;\
	type4 name4;\
	XDR_DEFINE_TYPE_ARCHIVE_5(friend,class_name,type0,name0,\
	type1,name1,type2,name2,type3,name3,type4,name4)

#define XDR_DEFINE_TYPE_ARCHIVE_5(inside,class_name,type0,name0,\
type1,name1,type2,name2,type3,name3,type4,name4)\
	inside size_t archived_size(const class_name& value)\
	{\
		serialize<serialize_object<xdr_filter>> core;\
		size_t size=core.archived_size(value.name0,\
	value.name1,value.name2,value.name3,value.name4);\
		return size;\
	}\
	inside size_t archive_to_with_size(\
	memory_address buffer,size_t bufsize,const class_name& value,size_t size)\
	{\
		serialize<serialize_object<xdr_filter>> core;\
		core.archived_size(value.name0,\
	value.name1,value.name2,value.name3,value.name4);\
		return core.archive_to(buffer,bufsize,value.name0,\
	value.name1,value.name2,value.name3,value.name4);\
	}\
	inside size_t rarchive_with_size(\
	const_memory_address buffer,size_t bufsize,class_name& value,size_t size)\
	{\
		serialize<serialize_object<xdr_filter>> core;\
		return core.rarchive(buffer,bufsize,value.name0,\
	value.name1,value.name2,value.name3,value.name4);\
	}
//5
#define XDR_DEFINE_TYPE_6(class_name,type0,name0,\
	type1,name1,type2,name2,type3,name3,type4,name4,type5,name5)\
	type0 name0;\
	type1 name1;\
	type2 name2;\
	type3 name3;\
	type4 name4;\
	type5 name5;\
	XDR_DEFINE_TYPE_ARCHIVE_6(friend,class_name,type0,name0,\
	type1,name1,type2,name2,type3,name3,type4,name4,type5,name5)

#define XDR_DEFINE_TYPE_ARCHIVE_6(inside,class_name,type0,name0,\
	type1,name1,type2,name2,type3,name3,type4,name4,type5,name5)\
	inside size_t archived_size(const class_name& value)\
	{\
		serialize<serialize_object<xdr_filter>> core;\
		size_t size=core.archived_size(value.name0,\
value.name1,value.name2,value.name3,value.name4,value.name5);\
		return size;\
	}\
	inside size_t archive_to_with_size(\
	memory_address buffer,size_t bufsize,const class_name& value,size_t size)\
	{\
		serialize<serialize_object<xdr_filter>> core;\
		core.archived_size(value.name0,\
value.name1,value.name2,value.name3,value.name4,value.name5);\
		return core.archive_to(buffer,bufsize,value.name0,\
value.name1,value.name2,value.name3,value.name4,value.name5);\
	}\
	inside size_t rarchive_with_size(\
	const_memory_address buffer,size_t bufsize,class_name& value,size_t size)\
	{\
		serialize<serialize_object<xdr_filter>> core;\
		return core.rarchive(buffer,bufsize,value.name0,\
value.name1,value.name2,value.name3,value.name4,value.name5);\
	}
//6
#define XDR_DEFINE_TYPE_7(class_name,type0,name0,\
	type1,name1,type2,name2,type3,name3,type4,name4,type5,name5,type6,name6)\
	type0 name0;\
	type1 name1;\
	type2 name2;\
	type3 name3;\
	type4 name4;\
	type5 name5;\
	type6 name6;\
	XDR_DEFINE_TYPE_ARCHIVE_7(friend,class_name,type0,name0,\
	type1,name1,type2,name2,type3,name3,type4,name4,type5,name5,type6,name6)

#define XDR_DEFINE_TYPE_ARCHIVE_7(inside,class_name,type0,name0,\
	type1,name1,type2,name2,type3,name3,type4,name4,type5,name5,type6,name6)\
	inside size_t archived_size(const class_name& value)\
	{\
		serialize<serialize_object<xdr_filter>> core;\
		size_t size=core.archived_size(value.name0,\
value.name1,value.name2,value.name3,value.name4,value.name5,value.name6);\
		return size;\
	}\
	inside size_t archive_to_with_size(\
	memory_address buffer,size_t bufsize,const class_name& value,size_t size)\
	{\
		serialize<serialize_object<xdr_filter>> core;\
		core.archived_size(value.name0,\
value.name1,value.name2,value.name3,value.name4,value.name5,value.name6);\
		return core.archive_to(buffer,bufsize,value.name0,\
value.name1,value.name2,value.name3,value.name4,value.name5,value.name6);\
	}\
	inside size_t rarchive_with_size(\
	const_memory_address buffer,size_t bufsize,class_name& value,size_t size)\
	{\
		serialize<serialize_object<xdr_filter>> core;\
		return core.rarchive(buffer,bufsize,value.name0,\
value.name1,value.name2,value.name3,value.name4,value.name5,value.name6);\
	}
#endif