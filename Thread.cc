#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0); //静态成员变量记录线程数量，在类外进行定义

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false), joined_(false), tid_(0), func_(std::move(func))
      //线程函数这里使用std::move将func底层资源窃取
      ,
      name_(name)
{
    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_)
    {
        thread_->detach(); // thread类提供的设置分离线程的方法
    }
}

void Thread::start() // 【一个Thread对象，记录的就是一个新线程的详细信息】
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0); // pshared=false，不是在进程间共享的信号量; value=0信号量初是0

    // 开启线程
    thread_ = std::shared_ptr<std::thread>(new std::thread([&]()
                                                           {
    //这里是创建了子线程，子线程第一行才获取了线程id，而外部调用者线程阻塞在信号量那里阻塞等待新创建线程的tid值
        // 获取线程的tid值
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        // 开启一个新线程，专门执行该线程函数
        func_(); }));

    // 【这里必须等待获取上面新创建的线程的tid值】 -使用信号量处理这个逻辑
    sem_wait(&sem); //调用start函数的线程，这里wait发现value为0，这里阻塞，直到value为1.
    //上面新创建的线程会post会给信号量值加1，那么这里阻塞的线程就会被唤醒。
    //即：后面调用start函数的线程就可以放心访问tid了，因为tid保证已经初始化了
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName()
{
    int num = ++numCreated_; //线程数量加1，线程名字就设置为thread id
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}
