#ifndef ISU_FUNCTIONAL_MODULE_HPP
#define ISU_FUNCTIONAL_MODULE_HPP

/*
	һ������.ȫ��Ϊ-������ģ��
	���Ե���dll�ĳ���
*/

class functional_module
{
	/*
		����ֵ����,����ʱ�󶨺ʹ���
	*/
	template<class Nick,class... Arg>
	void call(const Nick&, Arg... args);
};
#endif