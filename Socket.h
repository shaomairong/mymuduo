#pragma once

#include "noncopyable.h"

class InetAddress;

// socket就是对sockfd和对fd的函数进行面向对象的封装
class Socket : noncopyable
{
public:
    explicit Socket(int sockfd) // explicit抑制构造函数的隐式类型转换
        : sockfd_(sockfd)
    {
    }

    ~Socket();

    int fd() const { return sockfd_; }
    void bindAddress(const InetAddress &localaddr); //绑定传进来的inteaddress(ip+port)
    void listen();                                  //封装listen系统调用
    int accept(InetAddress *peeraddr);              // acceptr提取新连接从全连接队列里面

    void shutdownWrite(); //封装shutdown半关闭,关闭写端

    void setTcpNoDelay(bool on); //直接发送数据，不进行tcp缓冲
    void setReuseAddr(bool on);  //设置地址重用
    void setReusePort(bool on);  //设置端口重用
    void setKeepAlive(bool on);  //设置保活

private:
    const int sockfd_; // socket类封装sockfd
};
