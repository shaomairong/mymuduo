#pragma once
#include "noncopyable.h"
#include "Timestamp.h"
#include <functional>
#include <memory>
class EventLoop; // channel用到了EventLoop，所以在头文件这给出前置声明;在源文件中包含具体头文件
/**
 * 理清楚  [EventLoop、Channel、Poller之间的关系?]
 * 《= Reactor模型上对应 Demultiplex
 * Channel 理解为通道，封装了sockfd和其感兴趣的event，如EPOLLIN、EPOLLOUT事件、
 * 还绑定了poller返回的具体事件
 */
class Channel : private noncopyable
{
public:
    using EventCallback = std::function<void()>;              //事件回调
    using ReadEventCallback = std::function<void(Timestamp)>; //只读事件的回调
    Channel(EventLoop *loop, int fd);                         // EventLoop这里只是定义指针，所以上面有前置声明即可；
    ~Channel();

    // fd得到poller通知以后，处理事件的: 调用相应的回调函数;
    void handleEvent(Timestamp receiveTime);
    // TimeStamp这里是定义变量，所以不能像EventLoop一定前置声明，需要包含TimeStamp.h头文件

    // [设置回调函数对象]
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // 【防止当channel被手动remove掉，channel还在执行回调操作】
    void tie(const std::shared_ptr<void> &);

    int fd() const { return fd_; }
    int events() const { return events_; }
    int set_revents(int revt) { revents_ = revt; } // poller监听后设置事件

    // [设置fd相应的事件状态]:enable使能 ，disable使不能
    void enableReading()
    {
        events_ |= kReadEvent;
        update();
    }
    // update背后调用epoll_ctl把fd感兴趣的事件添加到epoll
    void disableReading()
    {
        events_ &= ~kReadEvent;
        update();
    }
    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    }
    void disableWriting()
    {
        events_ &= ~kWriteEvent;
        update();
    }
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }

    // [返回fd当前的事件状态]
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // one loop per thread
    EventLoop *ownerLoop() { return loop_; }
    void remove(); //删除channel使用的
private:
    //[当改变channel所表示fd的events事件后，update负责在poller里面更改fd相应的事件epoll_ctl]
    void update();
    // [根据poller通知的channel发生的具体事件， 由channel负责调用具体的回调操作]
    void handleEventWithGuard(Timestamp receiveTime);
    //[这三个变量表示当前fd的状态]
    static const int kNoneEvent;  // 1.没有对任何事件感兴趣
    static const int kReadEvent;  // 2.对读事件感兴趣
    static const int kWriteEvent; // 3.对写事件感兴趣

    EventLoop *loop_; // 事件循环
    const int fd_;    // fd ( Poller监听的对象 )
    int events_;      //  注册fd感兴趣的事件
    int revents_;     // poller返回的具体发生的事件
    int index_;

    std::weak_ptr<void> tie_; /*这个弱智能指针是防止我们手动调用remove channel后，我们还在使用channel
    所以[需要跨线程的对象的生存状态的监听];
    之前说过通过shared_ptr和weak_ptr在多线程中解决共享对象的线程安全问题;
    使用的时候可以把弱智能指针提升-lock为强智能指针，如果提升成功则说明资源存在，提升失败说明资源已经释放了
    */
    bool tied_;

    // 【因为channel通道里面能够获知fd最终发生的具体的事件revents，所以它负责调用具体事件的回调操作】
    //[四个函数对象，后面可以绑定外部传进来的操作]
    ReadEventCallback readCallback_; //只读事件的回调
    EventCallback writeCallback_;    //写事件回调
    EventCallback closeCallback_;    //关闭事件回调
    EventCallback errorCallback_;    //出错事件回调
};
