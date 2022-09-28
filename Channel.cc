#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include <sys/epoll.h>
const int Channel::kNoneEvent = 0; //类外定义在类中声明的static 变量，不加static关键字了
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;
// EventLoop: ChannelList Poller
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{ //记下channel所属的loop
}
Channel::~Channel()
{
}
//【 channel的tie方法什么时候调用过？一个TcpConnection新连接创建的时候 TcpConnection => Channel 】
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj; // tie_是weak_ptr,用shared_ptr初始化
    tied_ = true;
}
/**
 * 当改变channel所表示fd的events事件后，update负责在poller里面更改fd相应的事件epoll_ctl
 * EventLoop => ChannelList   Poller
 *
 */
void Channel::update() // channe里面保存了指向eventLoop的指针
{
    // 【通过channel所属的EventLoop，调用poller的相应方法，注册fd的events事件】
    loop_->updateChannel(this);
}
// 【在channel所属的EventLoop中， 把当前的channel删除掉】
void Channel::remove()
{
    loop_->removeChannel(this);
}
// fd得到poller通知以后，处理事件的
void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_) //是否绑定过
    {
        std::shared_ptr<void> guard = tie_.lock(); //把weak_ptr通过lock方法提升为shread_ptr
        if (guard)                                 //提升成功表示那个channel存活，可以处理
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}
// 根据poller通知的channel发生的具体事件， 由channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvent revents:%d\n", revents_);
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) //出问题了,调用closeCallback
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
    }
    if (revents_ & EPOLLERR) //出错了，调用errorCallback
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }
    if (revents_ & (EPOLLIN | EPOLLPRI)) //可读事件，调用readCallback
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }
    if (revents_ & EPOLLOUT) //可写事件，调用writeCallback
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}
