#ifndef ISU_STUB_IDENT_HPP
#define ISU_STUB_IDENT_HPP

#include <serialize.hpp>
#include <xdr.hpp>
#include <serialize_more_filter.hpp>

struct stub_ident
{
	typedef std::size_t size_t;
	stub_ident()
	{}
	stub_ident(const xdr_uint32& prog_,
		const xdr_uint32& vers_, const xdr_uint32& proc_)
		:prog(prog_), vers(vers_), proc(proc_)
	{}

	stub_ident(const stub_ident& rhs)
	{
		prog = rhs.prog;
		proc = rhs.proc;
		vers = rhs.vers;
	}
	//xdr_uint32 prog, vers, proc;

	XDR_DEFINE_TYPE_3(stub_ident,
		xdr_uint32, prog, xdr_uint32, vers, xdr_uint32, proc)
};


inline unsigned long long hash_value(const stub_ident& id)
{
	uint32_t high = std::hash_value(id.proc.get());
	uint32_t low = std::hash_value(id.vers.get());
	unsigned long long result = high;
	result <<= 32;
	result += low;
	return result;
}

inline bool operator==(const stub_ident& lhs, const stub_ident& rhs)
{
	//memcmp's ret==0. is mean lhs equal to rhs
	return memcmp(&lhs, &rhs, sizeof(stub_ident)) == 0;
}

inline bool operator!=(const stub_ident& lhs, const stub_ident& rhs)
{
	return !(lhs == rhs);
}

inline bool operator<(const stub_ident& lhs, const stub_ident& rhs)
{
	//when memcmp result <0
	return memcmp(&lhs, &rhs, sizeof(stub_ident)) < 0;
}
#endif