#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer => Acceptor => 有一个新用户连接，通过accept函数拿到connfd
 * =》 打包成TcpConnection，设置回调函数 =》设置Channel ，注册到Poller
 * -》poller监听事件发生，调用Channel的回调操作
 *
 */
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop,
                  const std::string &name,
                  int sockfd,
                  const InetAddress &localAddr,
                  const InetAddress &peerAddr);
    ~TcpConnection();

    EventLoop *getLoop() const { return loop_; } //返回tcpconnection所在的事件循环
    const std::string &name() const { return name_; }
    const InetAddress &localAddress() const { return localAddr_; }
    const InetAddress &peerAddress() const { return peerAddr_; }
    bool connected() const { return state_ == kConnected; } //设置tcpconnection的连接状态
    void send(const std::string &buf);                      // 发送数据
    void shutdown();                                        //调用shutdown关闭连接
    void setConnectionCallback(const ConnectionCallback &cb)
    {
        connectionCallback_ = cb;
    }
    void setMessageCallback(const MessageCallback &cb)
    {
        messageCallback_ = cb;
    }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    {
        writeCompleteCallback_ = cb;
    }
    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark)
    {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }
    void setCloseCallback(const CloseCallback &cb)
    {
        closeCallback_ = cb;
    }
    void connectEstablished(); // [连接建立]
    void connectDestroyed();   // [连接销毁]
private:
    enum StateE //枚举变量表示连接的状态
    {
        kDisconnected, //断开:底层socket关闭了，已经断开连接了
        kConnecting,   //正在连接:初始的时候
        kConnected,    //已经连接:连接成功时候
        kDisconnecting //正在断开:调用shutdown断开连接的时候
    };
    void setState(StateE state) { state_ = state; } //设置tcpconnection的状态

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void *message, size_t len);
    void shutdownInLoop();
    EventLoop *loop_;
    // 这里绝对不是baseLoop， 【因为TcpConnection都是在subLoop里面管理的】
    const std::string name_; //连接的名字
    std::atomic_int state_;  //[连接的状态]
    bool reading_;

    /* 这里和Acceptor类似:   Acceptor在mainLoop里;TcpConenction在subLoop里面 ;
    他们都需要把底层的listenfd和connfd封装成channel，然后channel注册到poller里面监听。
    */

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;       // 有新连接时的回调
    MessageCallback messageCallback_;             // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;

    Buffer inputBuffer_;  // 接收数据的缓冲区
    Buffer outputBuffer_; // 发送数据的缓冲区
};
