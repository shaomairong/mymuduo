#pragma once //[这个是防止头文件包含的编译器级别的语法]
//#ifndef那个防止头文件包含是语言级别的

/**
 * noncopyable被继承以后，派生类对象可以正常的构造和析构，但是派生类对象
 * 无法进行拷贝构造和赋值操作
 */
class noncopyable
{
public:
    noncopyable(const noncopyable &) = delete; //删除构造函数和构造赋值运算符
    noncopyable &operator=(const noncopyable &) = delete;

protected:
    noncopyable() = default; //默认构造函数和析构函数
    ~noncopyable() = default;
    // protected 是派生类可以访问，但是外部不能访问
};
