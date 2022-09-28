#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>
// one loop peer thread 每个线程一个事件循环
/* 作用：防止一个线程创建多个EventLoop   thread_local
创建了一个全局的eventloop类型的指针变量，但是用__thread关键字修饰
当eventloop对象创建出来后，它指向那个对象。
那么线程再去创建eventloop对象的时候，由于这个指针不为空，就不会创建了。
加__thread关键字修饰保证了每个线程都有一个副本。
 */
__thread EventLoop *t_loopInThisThread = nullptr;

const int kPollTimeMs = 10000; // 定义默认的Poller IO复用接口的超时时间10s
// 【全局函数:创建wakeupfd，用来notify唤醒subReactor处理新来的channel 】
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d \n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false), quit_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()) //通过封装的系统调用，获取线程id
      ,
      poller_(Poller::newDefaultPoller(this)) //调用封装的poller的函数创建
      ,
      wakeupFd_(createEventfd()) //调用全局函数eventfd创建wakeupfd
      ,
      wakeupChannel_(new Channel(this, wakeupFd_))
// channel打包wakeupfd,下面设置它感兴趣的事件
//相当于每个sub reactor 都监听了wekeupchanenl，所以当main reactor去notify 那个wekeupfd，那么sub reactor就被唤醒了
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if (t_loopInThisThread) //这个线程已经有一个eventloop了，不需要创建了
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this; //当前线程第一次创建了eventloop
    }

    // 【设置wakeupfd的事件类型以及发生事件后的回调操作】
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 【每一个eventloop都将监听wakeupchannel的EPOLLIN读事件了】
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll(); //对所有事件都不感兴趣了
    wakeupChannel_->remove();     //从poller中删除wekeupfd
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

//【 开启事件循环 】
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n", this);

    while (!quit_)
    {
        activeChannels_.clear(); //每次进来vector要clear
        // eventloop调的poller监听两类fd ：  一种是client的fd ，
        // 一种wakeupfd：mainreactor和sub reactor通信的fd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel : activeChannels_)
        {
            //【Poller监听哪些channel发生事件了，然后上报给EventLoop，通知channel处理相应的事件 】
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前EventLoop事件循环需要处理的回调操作
        /**
         * IO线程(即main reactor -main loop)的工作是接受新用户的连接，acceptr返回一个与客户端
         * 通信的fd，然后封装到channel中，因为main reactor只做新用户的连接，那么连接用户的channel要
         * 分发给sub reactor(sub loop);main reactor得到新连接的channel后会唤醒某一个sub reactor，
         * 那么就需要注册这个回调函数了。
         *      mainLoop 事先注册一个回调cb（需要subloop来执行）
         *      wakeup subloop后，执行下面的方法，执行之前mainloop注册的cb操作
         */
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
}

// 退出事件循环  1.loop在自己的线程中调用quit  2.在非loop的线程中，调用loop的quit
/**
 *              mainLoop
 *
 * （mainloop和subloop可以使用生产者-消费者模型，但是这里巧妙的采用了wakeupfd解决了通信过程
 *          no ==================== 生产者-消费者的线程安全的队列
 *
 *  subLoop1     subLoop2     subLoop3
 */
void EventLoop::quit()
{
    quit_ = true;

    // 【如果是在其它线程中，调用的quit   在一个subloop(woker)中，调用了mainLoop(IO)的quit 】
    if (!isInLoopThread())
    {
        wakeup(); //唤醒
    }
}

// 【在当前loop中执行cb】
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) // 在当前的loop线程中，执行cb
    {
        cb();
    }
    else // 在非当前loop线程中执行cb , 就需要唤醒loop所在线程，执行cb
    {
        queueInLoop(cb);
    }
}
// 【把cb放入队列中，唤醒loop所在的线程，执行cb】
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb); //在vector底层内存直接构造cb
    }

    // 【唤醒相应的，需要执行上面回调操作的loop的线程了】
    // || callingPendingFunctors_的意思是：当前loop正在执行回调，但是loop又有了新的回调
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup(); // 唤醒loop所在线 程
    }
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8", n);
    }
}

//【 用来唤醒loop所在的线程的：向wakeupfd_写一个数据，wakeupChannel就发生读事件，当前loop线程就会被唤醒】
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    /*这里读到什么不重要：  重要的是每个subreactor监听了wekeup channel(wakeupfd),
    那么main reactor就可以通过给wekeupChannel  write，那么sub reactor就可以感知到了wakeup上有读事件了。
    那么sub reactor就被唤醒了，就可以从main reactor拿到新用户的连接去处理了
    */

    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}

// EventLoop的方法 =》 Poller的方法
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors() // 执行回调
{
    std::vector<Functor> functors;  //定义一个局部回调的vector，然后使用swap操作
    callingPendingFunctors_ = true; //原子变量
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
        //相当于把pendingFunctors置空了，把内容转移到上面局部的functors里面了
        //直接交换，后面去局部functors处理即可，然后loop就可以并发操作了
    }

    for (const Functor &functor : functors)
    {
        functor(); // 执行当前loop需要执行的回调操作
    }

    callingPendingFunctors_ = false;
}
