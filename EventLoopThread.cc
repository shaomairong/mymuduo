#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
                                 const std::string &name)
    : loop_(nullptr), exiting_(false), thread_(std::bind(&EventLoopThread::threadFunc, this), name)
      //下面互斥锁和条件变量都是默认构造即可
      ,
      mutex_(), cond_(), callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();  //线程退出，那么把线程绑定的事件循环也给退出
        thread_.join(); //然后等待底层的子线程结束即可
    }
}

EventLoop *EventLoopThread::startLoop() //执行stratLoop的是一个线程，starat创建的又是一个新线程
{
    thread_.start(); // 【启动底层的新线程，执行下面的threadFunc函数】
    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr)
        {
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}

// 【下面这个方法，是在单独的新线程里面运行的】
void EventLoopThread::threadFunc()
{
    EventLoop loop; //【 创建一个独立的eventloop，和上面的线程是一一对应的，one loop per thread】

    if (callback_)
    {
        callback_(&loop); //把当前线程绑定的loop对象传递给初始化线程的回调函数
    }
    // loop_的修改进行互斥访问
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one(); //通知startloop现在loop_非空了
    }

    loop.loop();
    //【执行eventloop的loop函数，开启了底层poller的epoll，进行阻塞状态监听已连接事件和读写事件了】
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr; //这里相当于是loop返回了，即底层poller返回了，那么这里loop置为空即可
    //通常会一直在loop.loop()哪里执行，执行到末尾说明服务器要关闭了，不进行事件循环了
}
