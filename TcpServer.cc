#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include <strings.h>
#include <functional>

static EventLoop *CheckLoopNotNull(EventLoop *loop) //至少要有一个base loop
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}
/*one loop peer thread. 每个loop底层对应一个poller，底层的poller帮助loop去监听事件，
main loop也就是用户定义的base loop;loop就相当于reactor，poller就相当于多路事件分发器，底层就是epoll的操作。
acceptor运行在main loop里面，处理新用户的连接。
    acceptor就是创建listenfd然后listen,把fd封装成socket，把socket封装成acceptChannel，然后注册到poller里面监听，这样就能处理listenfd上的新连接了。



 */
TcpServer::TcpServer(EventLoop *loop,
                     const InetAddress &listenAddr,
                     const std::string &nameArg,
                     Option option)
    : loop_(CheckLoopNotNull(loop)), //至少要有base loop
      ipPort_(listenAddr.toIpPort()),
      name_(nameArg),
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)), /*acceptor是unique_ptr包裹,
       acceptor运行在main loop里面，此时正在定义tcpserver对象，还没有调用setthrenum，所以没有sub loop产生。
       acceptor是监听新用户连接的，所以要传递listenAddr。

       */
      threadPool_(new EventLoopThreadPool(loop, name_)),               //[事件循环的线程池]
      connectionCallback_(),
      messageCallback_(),
      nextConnId_(1),
      started_(0)
{
    /*[当有新用户连接时，会执行TcpServer::newConnection回调]:
    根据轮询算法选择一个sub loop，然后唤醒sub loop(通过eventfd函数创建的wakefd)，
    把当前connfd封装成channel分发给subloop*/
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
                                                  std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for (auto &item : connections_)
    {
        // 这个局部的shared_ptr智能指针对象，出右括号，可以自动释放new出来的TcpConnection对象资源了
        TcpConnectionPtr conn(item.second);
        item.second.reset();

        // 销毁连接
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

// 设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

// 开启服务器监听   loop.loop()
void TcpServer::start()
{
    if (started_++ == 0) // 防止一个TcpServer对象被start多次
    {
        threadPool_->start(threadInitCallback_); // 启动底层的loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
        //底层启动listend开始监听新用户的连接了
    }
}

// 【有一个新的客户端的连接，acceptor会执行这个回调操作newConnection】
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    // 【轮询算法，选择一个subLoop，来管理channel】
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_; // newConnection只再mainloop里面处理，不涉及线程安全问题，因此不需要是原子类型
    std::string connName = name_ + buf;
    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
             name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());
    // 通过sockfd获取其绑定的本机的ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }
    InetAddress localAddr(local);

    //[根据连接成功的sockfd，创建TcpConnection连接对象]
    TcpConnectionPtr conn(new TcpConnection(
        ioLoop,
        connName,
        sockfd, // Socket Channel
        localAddr,
        peerAddr));
    connections_[connName] = conn;
    // 下面的回调都是用户设置给TcpServer=>TcpConnection=>Channel注册到Poller，poller通知channel调用回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    // 这里是设置如何关闭连接的回调   conn->shutDown()
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
    // 【这里是直接调用TcpConnection::connectEstablished】
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
             name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn));
}
