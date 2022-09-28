#pragma once
#include "noncopyable.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; } //设置线程数量

    void start(const ThreadInitCallback &cb = ThreadInitCallback()); //开启事件循环线程，根据指定数量创建线程池

    // [如果工作在多线程中，baseLoop_默认以轮询的方式分配channel给subloop]
    EventLoop *getNextLoop();

    std::vector<EventLoop *> getAllLoops(); //返回池里面所有loop

    bool started() const { return started_; }
    const std::string name() const { return name_; }

private:
    EventLoop *baseLoop_; // ...用户最开始创建的eventLoop，最少得有一个eventLoop
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop *> loops_;
};
