#ifndef ISU_NDISK_NDISK_MACHINE_HPP
#define ISU_NDISK_NDISK_MACHINE_HPP

#include <string>
#include <vector>

//user headedr

#include <typedefs.hpp>
#include <ndisk_dbm.hpp>

class ndisk_machine
{
	public:
		ndisk_machine();
		ndisk_machine(const ndisk_string_t& config);

		typedef std::vector<ndisk_string_t> strings;

		void operator()();

		class ndisk_machine_config
		{
			friend class ndisk_machine;
			public:
				ndisk_machine_config();
				ndisk_machine_config(const ndisk_string_t& config);

			public://variable
			ndisk_string_t _token;//master machine must have this token
			strings _dirs;
			ndisk_string_t _dbm_name,_log_path;
			sockaddr_in _multicast_addr;
		}
	private:
		file_descriptor _socket;
		ndisk_dbm _dbm;
		ndisk_machine_config _cfg;
}
#endif
