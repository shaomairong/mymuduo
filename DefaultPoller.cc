#include "Poller.h"
#include "EPollPoller.h"

#include <stdlib.h>
//注意这个函数是Poller类下面的，但是没有在哪里实现，而是单独剥离出来一个文件进行实现。
Poller *Poller::newDefaultPoller(EventLoop *loop)
{
    if (::getenv("MUDUO_USE_POLL"))
    {
        return nullptr; // 生成poll的实例
    }
    else
    {
        return new EPollPoller(loop); // 生成epoll的实例
    }
}
