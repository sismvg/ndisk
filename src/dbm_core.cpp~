#include <dbm_linux_core.hpp>

dbm_core::dbm_core()
	:_core(nullptr)
{

}

dbm_core::dbm_core(const ndisk_string_t& name,int flag,mode_t mode)
	:_core(nullptr)
{
	connect(name,flag,mode);
}

dbm_core::~dbm_core()
{
	close();
}

int dbm_core::connect(const ndisk_string_t& name,int flag,mode_t mode)
{
	_core=dbm_open(name.c_str(),flag,mode);
	return _core!=nullptr ? 1 : 0;
}

void dbm_core::close()
{
	dbm_close(_core);
}

int dbm_core::store(datum key,datum content,int store_mode)
{
	return dbm_store(_core,key,content,store_mode);
}

datum dbm_core::fetch(datum key)
{
	return dmb_fetch(_core,key);
}



