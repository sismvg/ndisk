#ifndef ISU_IMOUTO_AUTH_HPP
#define ISU_IMOUTO_AUTH_HPP

#include <serialize.hpp>
#include <xdr.hpp>
#include <serialize_more_filter.hpp>

enum auth_flavor{
	auth_null=0,
	auth_unix,
	auth_short,
	auth_des
};

#define OPAQUE_AUTH_BODY_LENGTH 400

struct opaque_auth
{
	XDR_DEFINE_TYPE_2(opaque_auth,
	xdr_enum<auth_flavor>,flavor,
	xdr_fixed_length_opaque<OPAQUE_AUTH_BODY_LENGTH>, body)
};


#endif
