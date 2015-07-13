#ifndef ISU_FUNCTIONAL_MODULE_HPP
#define ISU_FUNCTIONAL_MODULE_HPP

/*
	一个抽象.全名为-函数组模块
	可以当作dll的抽象
*/

class functional_module
{
	/*
		返回值问题,运行时绑定和传参
	*/
	template<class Nick,class... Arg>
	void call(const Nick&, Arg... args);
};
#endif