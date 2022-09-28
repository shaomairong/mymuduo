#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

// [封装socket地址类型]
class InetAddress // InetAddress是可拷贝的类，muduo里面是继承copyable，我们这里省略即可
{
public: //在C里面写成struct sockaddr_in,在C++里面就可以省略struct了
        // explicit抑制构造函数的隐式类型转换
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in &addr)
        : addr_(addr)
    {
    }

    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    const sockaddr_in *getSockAddr() const { return &addr_; }
    void setSockAddr(const sockaddr_in &addr) { addr_ = addr; }

private:
    sockaddr_in addr_; // InetAddress里面成员是sockaddr_in
};
