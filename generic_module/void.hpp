#ifndef ISU_VOID_HPP
#define ISU_VOID_HPP

/*
	提供一个什么都不做的删除器
*/
void void_deleter(const void* ptr);

/*
	永远返回空指针
*/
void* void_allocer(unsigned long);

#endif