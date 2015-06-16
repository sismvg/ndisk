
#include <magic_modules.hpp>

typedef magic_modules<std::size_t, int, int, int> modules_vector;

void test()
{
	modules_vector mod;
	auto fn = [&](int v1, int v2)->int
	{
		++v1;
		return 0;
	};
	mod.insert_modules(0, fn);
	mod.get_modules(0);
	mod.get_magic_pos(0);
}