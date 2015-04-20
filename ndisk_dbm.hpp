#ifndef ISU_NDISK_NDISK_DBM_HPP
#define ISU_NDISK_NDISK_DBM_HPP

#include <map>
#include <memeory>
#include <typedefs.hpp>

class dbm_core;

class ndisk_dbm
{
	public:

		ndisk_dbm();
		ndisk_dbm(const ndisk_string_t&,int,int);
		int connect(const ndisk_string_t&);
		int disconnect();
		//Only serach and store

		typedef std::pair<ulong64,ulong64> block_range;
		block_range query(const ndisk_string_t&);//[first,last]
		
		int is_here(const ndisk_string_t&);//S: Return block count F: error code
		int is_here(const ndisk_string_t&,block_range);

		
		int insert(const ndisk_string_t&,block_range);//S:Return block count F:error code
		int remove(const ndisk_string_t&);//Have to remove all block
		int remove(const ndisk_string_t,block_range);//Maybe have more block

		// Not support cross data part
	private:
		std::shared_ptr<dbm_core*> _core;
}

#endif
