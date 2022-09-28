#include "Timestamp.h"

#include <time.h>

Timestamp::Timestamp() : microSecondsSinceEpoch_(0) {}

Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    : microSecondsSinceEpoch_(microSecondsSinceEpoch)
{
}

Timestamp Timestamp::now()
{
    return Timestamp(time(NULL)); //调用time(NULL)获取用长整型表示的时间
}

std::string Timestamp::toString() const //讲长整型转换成年月日时分秒表示的stirng时间
{
    char buf[128] = {0};
    tm *tm_time = localtime(&microSecondsSinceEpoch_); //用localtime把长整型时间转成tm类型
    snprintf(buf, 128, "%4d/%02d/%02d %02d:%02d:%02d",
             tm_time->tm_year + 1900, //年是从1900年开始的，所以需要加上1900
             tm_time->tm_mon + 1,     //月是0-11，所以需要加上1
             tm_time->tm_mday,
             tm_time->tm_hour + 8,
             tm_time->tm_min,
             tm_time->tm_sec);
    return buf;
}

// #include <iostream>
// int main()
// {
//     std::cout << Timestamp::now().toString() << std::endl;
//     return 0;
// }
