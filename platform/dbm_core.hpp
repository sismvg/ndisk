#ifndef ISU_NDISK_LINUX_DBM_CORE_HPP
#define ISU_NDISK_LINUX_DBM_CORE_HPP

#include <dbm.h>

#include <typedefs.hpp>

class dbm_core
{
public:
	dbm_core();
	dbm_core(const ndisk_string_t&,int flag,mode_t mode);
	int connect(const ndisk_string_t&,int flag,mode_t mode);
	int close();
	
	int store(datum key,datum content,int store_mode);
	datum fetch(datum key);
	~dbm_core();
private:
	DBM* _core;
}

#endif
