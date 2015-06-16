
#include <dynamic_bitmap.hpp>
#include <iostream>
#include <atomic>
dynamic_bitmap::dynamic_bitmap(size_t reserve)
	:_set(reserve)
{}

bool dynamic_bitmap::have(size_t pos) const
{
	if (_set.size() <= pos)
		return false;
	return just_test(pos) == true;
}

bool dynamic_bitmap::just_test(size_t pos) const
{
	return _set.test(pos);
}

bool dynamic_bitmap::test(size_t pos)
{
	_try_extern(pos);
	return just_test(pos);
}

bool dynamic_bitmap::test_and_set(size_t pos)
{
	_try_extern(pos);
	return just_test_and_set(pos);
}

bool dynamic_bitmap::just_test_and_set(size_t pos)
{
	bool result = just_test(pos);
	if (!result)
	{
		_set.set(pos, true);
	}
	return result;
}

void dynamic_bitmap::just_set(size_t pos, bool val)
{
	_set.set(pos, val);
}

void dynamic_bitmap::set(size_t pos, bool val)
{
	_try_extern(pos);
	just_set(pos, val);
}

void dynamic_bitmap::push_back(bool val)
{
	_set.push_back(val);
}

dynamic_bitmap::size_t dynamic_bitmap::size() const
{
	return _set.size();
}

dynamic_bitmap::size_t dynamic_bitmap::count() const
{
	return _set.count();
}

dynamic_bitmap::size_t dynamic_bitmap::arrange()
{
	size_t size = this->size();
	boost::dynamic_bitset<unsigned long> tmp(size);
	for (unsigned int index = 0; index != size; ++index)
		tmp.set(index, _set.test(index));
	_set = tmp;
	return size;
}
//impl

void dynamic_bitmap::_try_extern(size_t size)
{
	size_t now = this->size();
	if (now <= size)
	{
		//Ö¸Êý¹æ±Ü
		size_t block = (now * 2) / _set.bits_per_block + 1;
		for (unsigned int start = 0; start != block; ++start)
		{
			_set.append(0);
		}
	}
}
