#ifndef ISU_SAMPLE_REPORT_HPP
#define ISU_SAMPLE_REOPRT_HPP

class diagram
{
public:
	template<class Iter>
	void input(Iter begin, Iter end)
	{
		for (; begin != end; ++begin)
			_draw(*begin);
	}
private:
	void _draw(unsigned long long value);
};
#endif // !ISU_SAMPLE_REPORT_HPP
