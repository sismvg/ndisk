#ifndef ISU_DOUBLE_ULONG_32_HPP
#define ISU_DOUBLE_ULONG_32_HPP

#include <platform_config.hpp>

struct double_ulong32
{
	typedef unsigned long ulong32;
	typedef unsigned long long ulong64;
	typedef ulong32 size_t;

	double_ulong32(size_t high = 0, size_t low = 0);
	double_ulong32(const ulong64 val);
	operator ulong64() const;
	ulong64 to_ulong64() const;

	ulong32 high() const;
	ulong32 low() const;
	ulong32& high();
	ulong32& low();
private:
#ifdef _WINDOWS
	ulong32 _high, _low;
#else
#endif
};

bool operator<(const double_ulong32& lhs, const double_ulong32& rhs);
bool operator==(const double_ulong32& lhs, const double_ulong32& rhs);
bool operator!=(const double_ulong32& lhs, const double_ulong32& rhs);
bool operator>(const double_ulong32& lhs, const double_ulong32& rhs);
#endif