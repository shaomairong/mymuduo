#pragma once

#include "noncopyable.h"

#include <functional> //function
#include <thread>
#include <memory>
#include <unistd.h>
#include <string>
#include <atomic>

class Thread : noncopyable //线程不可复制，继承nocpyable
{
public:
    using ThreadFunc = std::function<void()>; //线程函数
    /* 这里线程函数用function封装的是返回值void，参数列表void，那么如果想带参数那怎么办呢？
     可以使用绑定器bind 。
     */

    explicit Thread(ThreadFunc, const std::string &name = std::string());
    ~Thread();

    void start();
    void join();

    bool started() const { return started_; }
    pid_t tid() const { return tid_; } //返回线程id
    /*pthread_selef输出的不是真正的线程id，这里实际上是类似于top命令查看的线程id
    pthread_self 是posix描述的线程ID（并非内核真正的线程id），相对于进程中各个线程之间的标识号，
    对于这个进程内是唯一的，而不同进程中，每个线程的 pthread_self() 可能返回是一样的。
    而 gettid 获取的才是内核中线程ID。
         为什么需要两个不同的ID呢？
        因为线程库实际上由两部分组成：【内核的线程支持+用户态的库支持(glibc)】
    Linux在早期内核不支持线程的时候glibc就在库中（用户态）以纤程（就是用户态线程）的方式支持多线程了，
    POSIX thread只要求了用户编程的调用接口对内核接口没有要求。

    */

    const std::string &name() const { return name_; }

    static int numCreated() { return numCreated_; }

private:
    void setDefaultName();

    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_; //把线程放到智能指针shared_ptr里面
    //直接定义一个thread线程对象，那么线程就运行起来了，所以这里使用智能指针封装它，自己掌控线程创建的时机。

    pid_t tid_;                         //线程id
    ThreadFunc func_;                   //线程函数
    std::string name_;                  //每个线程都有一个名字，方便后面打印调试使用
    static std::atomic_int numCreated_; // static变量，所有线程数量的计数
};
