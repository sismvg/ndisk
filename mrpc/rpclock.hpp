#ifndef ISU_RPCLOCK_HPP
#define ISU_RPCLOCK_HPP

#ifdef _WIN32
#include "platform/windows_rpclock.hpp"
#else
#include "platform/linux_rpclock.hpp"
#endif
#include <rpc_autolock.hpp>
#endif