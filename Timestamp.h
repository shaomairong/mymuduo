#pragma once

#include <iostream>
#include <string>

// 时间类
class Timestamp
{
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch);//加上explicit限制类型隐式转换
    //如果不加explicit意味着构造函数支持int64_t类型和Timestamp类型的隐式转换
    static Timestamp now();//获取当前时间，静态方法static  now 
    std::string toString() const;//toString转成年月日的时分秒 
private:
    int64_t microSecondsSinceEpoch_;
};