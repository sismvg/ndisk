
#include "../ndisk_dbm.hpp"
#include <dbm_core.hpp>

#ifdef _LINUX

datum make_key(const ndisk_string_t& key)
{
	return make_key(key.c_str());
}

void del_key(datum& key)
{
	delete key.dptr;
}

datum make_key(const ndisk_string_t::value_type* str,ulong64 len)
{
	datum ret;
	typedef ndisk_stirng_t::value_type char_t;
	ret.dsize=len*sizeof(char_t);
	ret.dptr=new char_t[ret.disze];
	strncpy(ret.dptr,str,ret.dsize);
	return datum;
}

#else

#endif

ndisk_dbm::ndisk_dbm()
{
	_init();
}

ndisk_dbm::ndisk_dbm(const ndisk_string_t& name,int flag,int mode)
{
	_init();
	connect(name,flag,mode);
}

int ndisk_dbm::connect(const ndisk_string_t& name,int flag,int mode)
{
	_core->connect(name,flag,mode);
}

int ndisk_dbm::disconnect()
{

}

int 
