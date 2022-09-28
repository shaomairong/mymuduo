#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <unistd.h>
#include <strings.h>
//表示当前channel跟epoll的状态
// channel未添加到poller中
const int kNew = -1; //这里channel的成员index_ 初始化 是 -1对应kNew，表示channel未添加到poller中
// channel已添加到poller中
const int kAdded = 1;
// channel从poller中删除
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)), events_(kInitEventListSize) // vector<epoll_event>
//先调用基类的构造函数Poller(loop);调用epoll_create1创建红黑树结构，返回epollfd
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}
EPollPoller::~EPollPoller()
{
    ::close(epollfd_); //关闭epollfd
}
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    // 实际上应该用LOG_DEBUG输出日志更为合理
    LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(),
                                 static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno; //记录全局的errno
    //因为poll可能在多个线程里面都会被调用，一个线程是一个eventLoop，一个eventLoop底层由一个poller
    Timestamp now(Timestamp::now());
    if (numEvents > 0) //有已经发生事件的fd的个数
    {
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels); //
        if (numEvents == events_.size())               //这次vector中所有监听的fd都有事件了，那么vector就需要提前扩容
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0) //这次epoll_wait没有事件发生，只是由于timeout超时了返回的
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else //出错情况下
    {
        if (saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}

// channel update remove => EventLoop updateChannel & removeChannel => Poller updateChannel removeChannel
/**
 *            EventLoop 包含channeelList和Plller
 *     ChannelList      Poller ： ChannelMap  <fd, channel*>   epollfd
 */
void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index(); //获取当前channel在poller中的状态
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);

    if (index == kNew || index == kDeleted)
    {
        if (index == kNew)
        {
            int fd = channel->fd();  //获取channel中保存的fd
            channels_[fd] = channel; //添加到poller的channelmap中
        }

        channel->set_index(kAdded); //更改index状态
        update(EPOLL_CTL_ADD, channel);
    }
    else // channel已经在poller上注册过了
    {
        int fd = channel->fd();
        if (channel->isNoneEvent()) // channel对任何事件都不感兴趣了，就不需要poller监听发生的事件了
        {
            update(EPOLL_CTL_DEL, channel); // EPOLL_CTRL_DEL删除
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel); //修改
        }
    }
}

// 从poller中删除channel
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    channels_.erase(fd); //在channeelpmap中删除

    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);

    int index = channel->index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel); //在channellist中删除
    }
    channel->set_index(kNew);
}

// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents; ++i)
    {
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
        // EventLoop就拿到了它的poller给它返回的所有发生事件的channel列表了
    }
}

// 更新channel通道 epoll_ctl add/mod/del
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    bzero(&event, sizeof event);

    int fd = channel->fd();

    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}
