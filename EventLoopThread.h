#pragma once

#include "noncopyable.h"
#include "Thread.h"

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>

class EventLoop;
/* 前面编写了线程类thread，现在是eventLoopThread事件循环线程类。
eventLoopThread事件循环线程类就是绑定了eventLoop和thread，让loop运行在这个thread里面。
就是one loop  peer thread。所以eventLoopThread里面的成员有thread和loop，一个线程对应一个loop。


 */
class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop *startLoop(); //开启事件循环

private:
    void threadFunc(); // eventloopthread的线程函数

    EventLoop *loop_; // eventloop事件循环对象
    bool exiting_;
    Thread thread_;    //线程对象
    std::mutex mutex_; //互斥锁和条件变量
    std::condition_variable cond_;
    ThreadInitCallback callback_; //线程初始化的回调函数
};
