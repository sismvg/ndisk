#ifndef IMOUTO_RPC_SERVER_HPP
#define IMOUTO_RPC_SERVER_HPP

#include <memory>

#include <server_stub_pool.hpp>

#include <dynamic_bitmap.hpp>
#include <rpc_autolock.hpp>
#include <imouto_rpcmsg.hpp>
#include <boost/dynamic_bitset.hpp>

template<
	class TranslationKernel,
	class Verify,
	class ThreadPool>
class imouto_rpc_server
	:public imouto_typedef<TranslationKernel,Verify>
{
public:
	typedef basic_server_stub<stub_ident> server_stub;
	typedef stub_pool<> stub_pool_type;
	typedef ThreadPool thread_pool;

	template<class Trans,class Verify,class ThreadPool,class StubPool>
	imouto_rpc_server(const Trans& trans,
		const Verify& verify,const ThreadPool& pool,const StubPool& stubs)
	{
		_init(trans, verify, pool, stubs);
	}

	void run()
	{
	}

	void stop()
	{

	}

	void blocking()
	{

	}
private:
	/*
		传输模块
	*/
	std::shared_ptr<translation_kernel> _translation;
	/*
		身份验证
	*/
	std::shared_ptr<verify> _verify;
	/*
		负责实际函数的调用还有一些其他工作
	*/
	std::shared_ptr<thread_pool> _threads;
	/*
		保存了指向实际函数的桩
	*/
	std::shared_ptr<stub_pool_type> _stubs;
	//
	boost::detail::spinlock _xidmap_lock;
	dynamic_bitmap _xidmap;

	template<class Trans,class Verify,class ThreadPool,class StubPool>
	void _init(const Trans& trans,
		const Verify& verify, const ThreadPool& pool, const StubPool& stubs)
	{
		_xidmap_lock = BOOST_DETAIL_SPINLOCK_INIT;
		_translation = trans;
		_verify = verify;
		_threads = pool;
		_stubs = stubs;
		//一个都不能少!
		if (!(trans&&verify&&pool&&stubs))
			return;//throw something
		trans->when_recv_complete([&](bool io_success, const error_code& io_code,
			const address_type& from, const shared_memory& memory)
		{
			_process_rpcmsg(io_success, io_code, from, memory);
		});
	}

	//和client长得很像..
	void _process_rpcmsg(bool io_success, const error_code& io_code,
		const address_type& from, const shared_memory& memory_ptr)
	{
		//TODO.放到同一个函数里去吧.这里和client一模一样
		try
		{
			memory_block memory = memory_ptr;
			while (memory.size)
			{
				size_type size = 0;

				advance_in(memory, rarchive(memory, size));
				memory_block msg_body(memory.buffer, size);

				_process_rpcmsg_impl(memory_ptr, io_success,
					io_code, from, msg_body);
				advance_in(memory, size);
			}
		}
		catch (std::overflow_error& msg)
		{
			//WAR.mylog太烂了,换一个
			mylog.log(log_debug, msg.what());
		}
	}

	void _process_rpcmsg_impl(
		const async_handle& handle,
		bool io_success, const error_code& io_code,
		const address_type& from, memory_block memory)
	{
		//检测io错误-检测版本-检测用户-检测桩是否存在-调用-捕捉异常和参数数据
		//-回送

		if (!io_success)
		{
			//没啥好做的
			return;
		}

		auto* body = reinterpret_cast<rpc_call_body*>(memory.buffer);
		auto& call_body = body->body.body;

		reply_body reply;

		reply.head = body->body.head;
		reply.head.magic = msg_type::replay;
		reply.trunk = body->trunk;

		advance_in(memory, archived_size(*body));
		//只要遇到一次失败就会由函数发送回馈
		_check_xid(*body)
		&&
		_check_version(from, reply, call_body.rpcvers)
		&&
		_check_user(from, reply, body->ident)
		&&
		_check_stub(from, handle, reply, memory, body);
	}

	void _set(size_type xid, bool value)
	{
		auto lock = ISU_AUTO_LOCK(_xidmap_lock);
		_xidmap.set(xid, value);
	}

	bool _check_xid(const rpc_call_body& body)
	{
		auto lock = ISU_AUTO_LOCK(_xidmap_lock);
		auto xid = body.body.head.xid;
		return _xidmap.test(xid) == false;
	}

	bool _check_version(const address_type& from,
		reply_body& reply, const xdr_uint32& vers)
	{
		bool ret = true;
		if (vers != IMOUTO_RPC_VERS)
		{
			ret = false;
			reply.what = msg_denied;
			reply.stat = reject_stat::rpc_mismatch;
			mismatch_info info(0, IMOUTO_RPC_VERS);
			_reply(from, reply, info);
		}
		return ret;
	}

	bool _check_user(const address_type& from,
		reply_body& reply, verify_ident& ident)
	{
		bool ret = true;
		auto check_result = _verify->check_verify(ident);
		if (!check_result)
		{
			reply.what = msg_denied;
			reply.stat = reject_stat::auth_error;
			ret = false;
			_reply(from, reply, check_result);
		}
		const auto& reply_ident = _verify->reply_verify(ident);
		reply.ident = reply_ident;
		return ret;
	}

	bool _check_stub(
		const address_type& from,
		const async_handle& handle,reply_body& reply,
		const memory_block& memory,rpc_call_body* body)
	{
		find_stat ret = find_success;
		auto& call_body = body->body;
		auto pair = _stubs->find(body->body.body.ident);
		memory_block arguments;
		rarchive(memory, arguments);
		if ((ret = pair.second)==find_success)
		{
			_set(body->body.head.xid, true);
			_threads->schedule([&,pair,body,arguments,from,reply,handle]()
			{
				_call_stub(from, reply, *pair.first->second,
					handle, arguments, body);
			});
		}
		else
		{
			reply.what = msg_accepted;
			reply.stat = pair.second;
			_reply(from, reply);
		}
		return ret == find_success;
	}

	void _call_stub(const address_type& from,reply_body reply,const server_stub& stub,
		const async_handle& handle,const memory_block& memory, rpc_call_body* body)
	{
		memory_block exdata;
		reply.what = msg_accepted;
#ifdef _WINDOWS
		__try
		{
#endif
			try
			{
				auto result_and_arg = stub(memory);
				delete memory.buffer;
				exdata = result_and_arg;
				reply.stat = success;
				_reply(from, reply, exdata);
				return;
			}
			catch (std::runtime_error& error)
			{
				xdr_string str(error.what());
				exdata = archive(cpp_style_runtime_error(str));
			}
			catch (...)
			{
				exdata = archive(cpp_style_runtime_error(
					"stub call has unknow runtime error"));
			}
			//WAR.enum值不对
			reply.stat = proc_cpp_exception;
#ifdef _WINDOWS
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			EXCEPINFO info;
			GetExceptionInformation(&info);
			exdata = archive(info);
			reply.stat = proc_c_exception;
		}
#endif
		_reply(from, reply, exdata);
	}

	template<class... Arg>
	void _reply(const address_type& from, const Arg&... args)
	{
		auto mem = archive(archived_size(args...), args...);
		_translation->send(from, mem.buffer, mem.size);
		delete mem.buffer;
	}

};

#endif