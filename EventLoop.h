#pragma once

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

class Channel;
class Poller;
//【 时间循环类】 主要包含了两个大模块 Channel   Poller（epoll的抽象）
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;
    EventLoop();
    ~EventLoop();
    void loop(); //开启事件循环
    void quit(); //退出事件循环
    Timestamp pollReturnTime() const { return pollReturnTime_; }
    void runInLoop(Functor cb);   //[在当前loop中执行cb]
    void queueInLoop(Functor cb); // [把cb放入队列中，唤醒loop所在的线程，执行cb]
    void wakeup();                // [用来唤醒loop所在的线程的(main reactor唤醒sub reactor)]
    // EventLoop的方法 =》 Poller的方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);
    // [判断EventLoop对象是否在自己的线程里面]
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
    void handleRead();        // wake up
    void doPendingFunctors(); // [执行回调]
    using ChannelList = std::vector<Channel *>;
    std::atomic_bool looping_; // 原子操作，通过CAS实现的
    std::atomic_bool quit_;    // 标识退出loop循环

    const pid_t threadId_; // 【 记录当前loop所在线程的id:one loop peer thread 】

    Timestamp pollReturnTime_;       // poller返回发生事件的channels的时间点
    std::unique_ptr<Poller> poller_; //[Plloer]相当于就是epoll抽象
    // muduo库中多路事件分发器的核心IO复用模块
    /* main reactor 给sub reactor分配新连接的时候采用的轮询操作。
    sub reactor如何没有事件发生时，所在的线程都是阻塞的，那么main reactor如何唤醒呢？
        muduo库采用的是eventfd机制：看C++服务器开发精髓
        下面的wakeupFD就是用eventfd创建出来的，
    */
    int wakeupFd_;
    /* 主要作用，当mainLoop获取一个新用户的channel，
    通过轮询算法选择一个subloop，通过该成员唤醒subloop处理channel */
    std::unique_ptr<Channel> wakeupChannel_; // wakeupfd封装到wakeupchannel里面

    //[通道Channel]里面有fd、感兴趣的事件以及实际发生的事件
    ChannelList activeChannels_;              // [ChannelList保存一堆channel]
    std::atomic_bool callingPendingFunctors_; //【标识当前loop是否有需要执行的回调操作】
    std::vector<Functor> pendingFunctors_;    // [存储loop需要执行的所有的回调操作]
    std::mutex mutex_;                        // [互斥锁，用来保护上面vector容器的线程安全操作]
};
