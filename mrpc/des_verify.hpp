#ifndef ISU_DES_VERIFY_HPP
#define ISU_DES_VERIFY_HPP

#include <stub_ident.hpp>

class des_verify_ident
{};

class des_verify
{
public:
	typedef des_verify_ident verify_ident;

	inline verify_ident generic_verify(const stub_ident& ident)
	{
		return verify_ident();
	}

	inline bool check_verify(const verify_ident&)
	{
		return true;
	}

	inline verify_ident reply_verify(const verify_ident)
	{
		return verify_ident();
	}
};

#endif