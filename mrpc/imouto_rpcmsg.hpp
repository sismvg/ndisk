#ifndef ISU_IMOUTO_RPCMSG_HPP
#define ISU_IMOUTO_RPCMSG_HPP

#include <serialize.hpp>
#include <xdr.hpp>
#include <serialize_more_filter.hpp>

#include <imouto_auth.hpp>

#define IMOUTO_RPC_VERS 2

enum msg_type
{
	call=0,
	replay
};

enum replay_stat
{
	msg_accepted=0,
	msg_denied
};

enum accept_stat
{
	success=0,//执行成功
	prog_unavail,//程序号不支持
	prog_mismatch,//版本号不支持
	proc_unavail,//过程号不支持
	garbage_args,//过程不能解释参数
	proc_cpp_exception,//过程遇到了C++异常
	proc_c_exception//过程遇到了C风格异常,具体与平台相关
};

const std::size_t enum_value_boundary = 8;

enum reject_stat
{
	rpc_mismatch,//rpc版本号不匹配
	auth_error//身份认证失败
};

/*
	先这么用着
*/
enum session_stat
{
	session_rpc_failed=0,
	session_io_failed,

	session_success,
	session_prog_unavail,
	session_prog_mismatch,
	session_proc_unavail,
	session_garbage_args,
	session_cpp_exception,
	session_c_exception,

	session_rpc_mismatch,
	session_auth_error,
};

enum auth_stat
{
	auth_badcred=1,//错误的证书
	auth_rejectedcred,//客户必须开始新会话
	auth_badverf,//错误的校验符
	auth_rejectedverf,//校验符过期或者重放
	auth_tooweak//安全原因拒绝
};

//rpc消息

struct call_body
{
	XDR_DEFINE_TYPE_2(call_body,
		stub_ident,ident,xdr_uint32,rpcvers)
};

struct rpc_msg
{
	xdr_uint32 xid;
};

struct accepted_replay
{
	XDR_DEFINE_TYPE_1(accepted_replay,
		opaque_auth,verf)
};

struct mismatch_info
{
	mismatch_info(xdr_uint32 high_, xdr_uint32 low_)
		:high(high_), low(low_)
	{

	}
	XDR_DEFINE_TYPE_2(
		mismatch_info,
		xdr_uint32, low, xdr_uint32, high)
};
//根据replay stat的不同用不同的结构
struct replay_data_success
{
	const_memory_block results;
};

struct replay_data_prog_unavail
{

};

struct replay_data_proc_unavail
{

};

struct replay_data_garbage_args
{

};
//服务器拒绝请求时
//rpc-mismatch:mismatch_info

//rpc-auth_error:auth_stat

//认证方式

//1-不认证

//2-unix认证

struct auth_unix
{
	typedef xdr_array<xdr_uint32, 10> gid_array;

	XDR_DEFINE_TYPE_5(auth_unix,
		xdr_uint32,stamp,xdr_string,machine_name,
		xdr_uint32,uid,xdr_uint32,gid,gid_array,gids)
};

//3-DES校验

enum authdes_namekind
{
	adn_fullname=0,
	adn_nickname=1
};

typedef xdr_fixed_length_opaque<8> des_block;

#ifndef MAXNETNAMELEN
#define MAXNETNAMELEN 255
#endif

#define IMOUTO_MAX_NAME_LENGTH MAXNETNAMELEN

struct authdes_fullname
{
	XDR_DEFINE_TYPE_3(authdes_fullname,
		xdr_string,name,
		des_block,key,xdr_uint32,window)
};

struct authdes_nickname
{
	XDR_DEFINE_TYPE_1(authdes_nickname,
	xdr_uint32,adc_nickname)
};

//时间戳
struct timestamp
{
	XDR_DEFINE_TYPE_2(
		timestamp,
		xdr_uint32,seconds,
		xdr_uint32,useconds)
};

struct authdes_verf_clnt
{
	XDR_DEFINE_TYPE_2(authdes_verf_clnt,
		timestamp,adv_timestamp,
		xdr_uint32,adv_winverf)
};

struct authdes_verf_svr
{
	XDR_DEFINE_TYPE_2(authdes_verf_svr,
		timestamp,adv_timestamp,
		xdr_uint32,adv_nickname)
};

struct rpcmsg_head
{
	XDR_DEFINE_TYPE_2(rpcmsg_head,
		xdr_uint32, xid,
		xdr_enum<msg_type>, magic)
};

struct rpcstub_ident
{
	rpcstub_ident()
	{}

	rpcstub_ident(const stub_ident& ident)
	{
		body.ident = ident;
		body.rpcvers = IMOUTO_RPC_VERS;
	}

	XDR_DEFINE_TYPE_2(
		rpcstub_ident,
		rpcmsg_head, head,
		call_body, body)
};

template<class VerifyIdent>
struct rpc_call_body
{
	typedef VerifyIdent verify_ident;
	//WAR.指针是否复制值还不明确
	XDR_DEFINE_TYPE_3(rpc_call_body,
		rpcstub_ident, body, void*, trunk, verify_ident, ident)
};

/*
	不必要和rpc_call_body的VerifyIdent一模一样.
	只要能被提供给server or client的Verify类检测即可
	.
	不提供stub_ident和rpcver.没有意义
*/
template<class VerifyIdent>
struct reply_body
{
	typedef std::size_t size_type;
	typedef reply_body<VerifyIdent> self_type;
	typedef VerifyIdent verify_ident;
	
	XDR_DEFINE_TYPE_5(
	self_type,
	rpcmsg_head,head,
	replay_stat,what,
	size_type,stat,
	verify_ident,ident,
	void*,trunk)
};

template<class TranslationKernel, class Verify>
class imouto_typedef
{
public:
	typedef std::size_t size_type;
	typedef stub_ident ident_type;

	typedef shared_memory_block<char> shared_memory;
	typedef shared_memory_block<const char> const_shared_memory;
	typedef const_shared_memory async_handle;

	typedef TranslationKernel translation_kernel;
	typedef typename translation_kernel::address_type address_type;
	typedef typename translation_kernel::error_code error_code;

	typedef Verify verify;
	typedef typename verify::verify_ident verify_ident;

	typedef rpc_call_body<verify_ident> rpc_call_body;

	//防止reply_verify是模板函数所以用了这个方法
	typedef typename real_type<decltype(
		((verify*) (nullptr))->reply_verify(
		*(verify_ident*) (nullptr))) > ::type reply_verify_type;

	typedef reply_body<reply_verify_type> reply_body;
};

/*
	reply包格式
	accept: what:msg_accpet

		stat: success-正常执行:
			[reply_body]+[result and altered_arguments]

		stat: proc_cpp_exception-C++异常:
			[reply_body]+[rpc_runtime_error]

		stat: proc_c_exception-C异常:
			[reply_body]+[平台相关异常结构,带有size]

		stat: prog_unvail or proc_unvail or ver_mimatch-未匹配桩:
			[reply_body]

		stat: garbage_args-桩无法解析参数:
			[reply_body]

	reject: what:msg_denied

		stat: rpc_mismatch-rpc版本不匹配:
			[reply_body]

		stat: auth_error-用户身份验证错误:
			[reply_body]+[Verify类在check_verify时的返回值]
*/

/*
	call包格式

	rpc_call_body-
	[rpcstub_ident]-stub|[void*]-trunk|[verify_ident]-ident

	rpcstub_ident
	[call_body]-body|[rpcmsg_head]-head

	call_body
	[stub_ident]-ident|[xdr_uint32]-rpcver

	rpcmsg_head
	[msg_type]-magic|[xdr_uint32]-xid
*/
#endif
