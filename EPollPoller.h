#pragma once

#include "Poller.h"
#include "Timestamp.h"

#include <vector>
#include <sys/epoll.h>

class Channel;

/**
 * epoll的使用
 * epoll_create   创建红黑树结构，返回epollfd  （构造函数里面创建，析构函数里面释放）
 * epoll_ctl   【add/mod/del】  将fd添加到红黑树结构上面 (updateChannel和removeChannel)
 * epoll_wait    （poll函数里面）
 */
class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    // 【重写基类Poller的抽象方法】
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override; // override覆盖，派生类重写基类的虚函数
    void removeChannel(Channel *channel) override;

private:
    static const int kInitEventListSize = 16; // epollevent类型的vector初始的长度

    // 填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    // 更新channel通道，底层就是调用epoll_ctrl
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;
    //【epoll_wait第二个参数需要一个数组，这里使用vector方便扩容】

    int epollfd_; // epollfd代表底层的红黑树句柄
    EventList events_;
};
