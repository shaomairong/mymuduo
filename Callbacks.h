#pragma once

#include <memory>
#include <functional>
//下面定义指针和引用，所以需要类的前置声明
class Buffer;
class TcpConnection;
class Timestamp;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
//指向TCP连接的unique_ptr
using ConnectionCallback = std::function<void(const TcpConnectionPtr &)>;
//[@@@@@有新连接的回调函数]
using CloseCallback = std::function<void(const TcpConnectionPtr &)>;
//
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;
//
using MessageCallback = std::function<void(const TcpConnectionPtr &, Buffer *, Timestamp)>;
//@@@@@[有读写消息时候的回调函数]
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr &, size_t)>;
//高水位的回调函数
