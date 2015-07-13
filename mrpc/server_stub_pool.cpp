
#include <server_stub_pool.hpp>

stub_ident_allocer::stub_ident_allocer(
	const size_type& progid, const size_type& default_ver)
	:_progid(progid), _ver(default_ver)
{

}

stub_ident_allocer::alloc_result 
	stub_ident_allocer::try_alloc(const size_type& procid)
{
	return try_alloc(procid, _ver);
}

stub_ident_allocer::alloc_result 
	stub_ident_allocer::try_alloc(const size_type& procid, const size_type& ver)
{
	return _alloced.emplace(_progid, procid, ver);
}

stub_ident_allocer::local_size_type stub_ident_allocer::size() const
{
	return _alloced.size();
}

void stub_ident_allocer::erase(const size_type& procid)
{
	erase(procid, _ver);
}

void stub_ident_allocer::erase(const size_type& procid, const size_type& ver)
{
	_alloced.erase(stub_ident(_progid, procid, ver));
}

stub_ident_allocer::const_iterator stub_ident_allocer::ident(
	const size_type& procid, const size_type& ver) const
{
	return _alloced.find(stub_ident(_progid, procid, ver));
}

stub_ident_allocer::const_iterator 
	stub_ident_allocer::ident(const size_type& procid) const
{
	return ident(procid, _ver);
}

stub_ident_allocer::const_iterator stub_ident_allocer::begin() const
{
	return _alloced.begin();
}

stub_ident_allocer::const_iterator stub_ident_allocer::end() const
{
	return _alloced.end();
}

bool stub_ident_allocer::empty() const
{
	return _alloced.empty();
}