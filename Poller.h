#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <vector>
#include <unordered_map>

class Channel;
class EventLoop;

// muduo库中多路事件分发器的核心IO复用模块
class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel *>;

    Poller(EventLoop *loop);
    virtual ~Poller() = default; //虚析构函数

    //【 给所有IO复用保留统一的接口】
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    // 判断参数channel是否在当前Poller当中
    bool hasChannel(Channel *channel) const;

    //【 EventLoop可以通过该接口获取默认的IO复用的具体实现】
    static Poller *newDefaultPoller(EventLoop *loop); // h的cc文件不实现，在DefaultPooler.cc中单独实现

protected:
    // map的key：sockfd              value：sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int, Channel *>;
    ChannelMap channels_;
    // eventloop包含channel和poller，poller监听的就是eventloop里面保存的channel
private:
    EventLoop *ownerLoop_; // 定义Poller所属的事件循环EventLoop
};
