
#include <double_ulong32.hpp>

double_ulong32::double_ulong32(size_t high, size_t low)
	:_high(high), _low(low)
{}

double_ulong32::double_ulong32(const ulong64 val)
{
#ifdef _WINDOWS
	_high = val >> 32;
	_low = val & 0xFFFFFFFF;
#else
#endif
}

double_ulong32::operator ulong64() const
{
	return to_ulong64();
}

double_ulong32::ulong64 
	double_ulong32::to_ulong64() const
{
#ifdef _WINDOWS
	ulong64 ret = _high;
	ret <<= 32;
	ret |= _low;
#else
#endif
	return ret;
}

double_ulong32::ulong32 
	double_ulong32::high() const
{
	return _high;
}

double_ulong32::ulong32
	double_ulong32::low() const
{
	return _low;
}

double_ulong32::ulong32&
	double_ulong32::high()
{
	return _high;
}

double_ulong32::ulong32&
	double_ulong32::low()
{
	return _low;
}


bool operator<(const double_ulong32& lhs, const double_ulong32& rhs)
{
	return lhs.to_ulong64() < rhs.to_ulong64();
}

bool operator==(const double_ulong32& lhs, const double_ulong32& rhs)
{
	return lhs.to_ulong64() == rhs.to_ulong64();
}

bool operator!=(const double_ulong32& lhs, const double_ulong32& rhs)
{
	return lhs.to_ulong64() != rhs.to_ulong64();
}

bool operator>(const double_ulong32& lhs, const double_ulong32& rhs)
{
	return lhs.to_ulong64() > rhs.to_ulong64();
}