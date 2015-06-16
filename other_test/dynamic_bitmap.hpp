#ifndef ISU_DYNAMIC_BITMAP_HPP
#define ISU_DYNAMIC_BITMAP_HPP

#include <boost/dynamic_bitset.hpp>

class dynamic_bitmap
{
public:
	typedef std::size_t size_t;

	/*
		reserve:预留的位数量
	*/
	dynamic_bitmap(size_t reserve = 0);

	/*
		测试pos的值
	*/
	bool test(size_t pos);
	/*
		如果test(pos)为0则设置这个pos为1然后retrun false
		否则return true
	*/
	bool test_and_set(size_t pos);

	/*
		不扩展内存的test_and_set
	*/
	bool just_test_and_set(size_t pos);

	/*
		设置pos为1,并返回原来的值
	*/
	void set(size_t pos, bool val);

	/*
		向最后添加一个val
	*/
	void push_back(bool val);

	/*
		是否有这个bit,或该bit是否被设置为1
	*/
	bool have(size_t pos) const;

	/*
		不扩展内存大小的test和set
	*/
	bool just_test(size_t pos) const;
	void just_set(size_t pos, bool val);
	/*
		有多少个位
	*/
	size_t size() const;
	size_t count() const;
	/*
		去掉预留的size.
	*/
	size_t arrange();
private:

	void _try_extern(size_t size);
	boost::dynamic_bitset<unsigned long> _set;
};
#endif