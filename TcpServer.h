#pragma once

/**
 * 用户使用muduo编写服务器程序,在tcpserver.h里面直接包含所有用到的头文件
 */
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

// 对外的服务器编程使用的类
class TcpServer : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>; //定义线程初始化回调函数类型
    enum Option
    {
        kNoReusePort,
        kReusePort,
    };
    TcpServer(EventLoop *loop,
              const InetAddress &listenAddr,
              const std::string &nameArg,
              Option option = kNoReusePort);
    ~TcpServer();
    void setThreadInitcallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }
    void setThreadNum(int numThreads); // 设置底层subloop的个数
    void start();
    /* [开启服务器监听:tcpserver的start函数其实就是开启底层的main loop 的acceptor的listen ]*/
private:
    void newConnection(int sockfd, const InetAddress &peerAddr); //处理新连接
    void removeConnection(const TcpConnectionPtr &conn);         //从connectionMap中移除connection
    void removeConnectionInLoop(const TcpConnectionPtr &conn);
    EventLoop *loop_;          //【事件循环EventLoop】 baseLoop 用户定义的loop
    const std::string ipPort_; //保存服务器的ip地址和端口号以及服务器名称
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_;              // [acceptor运行在mainLoop任务就是监听新连接事件]
    std::shared_ptr<EventLoopThreadPool> threadPool_; //[事件循环的线程池] one loop per thread
    ConnectionCallback connectionCallback_;           // 【有新连接时的回调】
    MessageCallback messageCallback_;                 // 【有读写消息时的回调】
    WriteCompleteCallback writeCompleteCallback_;     // 【消息发送完成以后的回调】
    //在callbacks.h中统一定义了回调函数的类型
    ThreadInitCallback threadInitCallback_; // [loop线程初始化的回调]
    std::atomic_int started_;
    int nextConnId_;
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
    ConnectionMap connections_; // [connectionmap保存所有的连接]
};
