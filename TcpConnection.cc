#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <functional>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <string>

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
                             const std::string &nameArg,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop)), name_(nameArg), state_(kConnecting), reading_(true), socket_(new Socket(sockfd)) //把sockfd打包成socket
      ,
      channel_(new Channel(loop, sockfd)) //把sockfd和所在的loop打包成channel
      ,
      localAddr_(localAddr), peerAddr_(peerAddr), highWaterMark_(64 * 1024 * 1024) // 设置高水位标记: 64M
//之前acccptor只设置了readcallback，这里tcpconnection要设置很多的回调如下:
{
    // 下面给channel设置相应的回调函数，poller给channel通知感兴趣的事件发生了，channel会回调相应的操作函数
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true); //启动tcp的保活机制
}

TcpConnection::~TcpConnection()
{
    // tcpconnection开辟的额外资源是使用智能指针管理的，所以这里不需要处理资源回收的操作。
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n",
             name_.c_str(), channel_->fd(), (int)state_);
}

void TcpConnection::send(const std::string &buf)
{
    //执行sendinloop
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread()) //[当前loop是不是在在他对应的线程里面]
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop,
                this,
                buf.c_str(),
                buf.size()));
        }
    }
}

/**
 * 发送数据：应用写的快，而内核发送数据慢，需要把待发送数据写入缓冲区，而且设置了水位回调
 */
void TcpConnection::sendInLoop(const void *data, size_t len)
{
    ssize_t nwrote = 0;     //写的数据
    size_t remaining = len; //没发送的数据
    bool faultError = false;
    // 【之前调用过该connection的shutdown，不能再进行发送了】
    if (state_ == kDisconnected) // state是原子类型
    {
        LOG_ERROR("disconnected, give up writing!");
        return;
    }
    /* 刚开始注册的都是socket的读事件，写事件我们没有注册。
    1)所以这里刚开始对写事件不感兴趣，取反就是true了 && channel是第一次写数据，而且缓冲区没有待发送数据,那就直接发送数据。
        直接发送数据:能发完，就不需要给channel注册EPOLLOUT事件了。
                    :不能发完，

     */
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), data, len); //发送数据
        if (nwrote >= 0)                             //发送成功了
        {
            remaining = len - nwrote; //剩余待发送数据
            if (remaining == 0 && writeCompleteCallback_)
            {
                //[既然在这里数据全部发送完成，就不用再给channel设置epollout事件了]
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // nwrote < 0,出错
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET) // SIGPIPE  RESET
                {
                    faultError = true;
                }
            }
        }
    }

    /* [说明当前这一次write，并没有把数据全部发送出去，剩余的数据需要保存到缓冲区当中，然后给channel
     注册epollout事件，poller发现tcp的发送缓冲区有空间，会通知相应的sock-channel，调用writeCallback_回调方法
     也就是调用TcpConnection::handleWrite方法，把发送缓冲区中的数据全部发送完成] */
    if (!faultError && remaining > 0) //没有出错，数据没有发送完成。
    {
        // 目前发送缓冲区剩余的待发送数据的长度
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
        {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
            //调用高水位回调函数
        }
        outputBuffer_.append((char *)data + nwrote, remaining); //把剩余没发送的数据拷贝到缓冲区
        if (!channel_->isWriting())
        // channel没有对写事件感兴趣,那么需要注册channel的写事件，后面才能发送缓冲区的数据。
        {
            channel_->enableWriting(); // [这里一定要注册channel的写事件，否则poller不会给channel通知epollout]
        }
    }
}
//【每个loop执行的方法都要在loop对应的线程里面处理】
// 关闭连接
void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        setState(kDisconnecting); // conncted -> disconnecting
        /*下面调用shutdowninloop函数，会判断数据释放发送完，发送完就直接关闭。
        没发送完就不执行了，但是当数据发送完成后那个函数会判断state_状态已经变了，
        所以又会调用shutdwoninloopw去关闭连接，调用socket的shutdwonwrite关闭socket的写端，
        底层的poller就会给channel上报EPOLLHUP事件，就会调用closeCallback回调函数。
        底层poller通知channel调用closeCallback回调函数->TcpConnection::handleClose
        
         */
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting()) // [说明outputBuffer中的数据已经全部发送完成]
    {
        socket_->shutdownWrite(); // 关闭写端
        /* 关闭写端会触发socket的EPOLLHUP事件，EPOLLHUP事件是不用专门去向epoll注册的，
        本身epoll给所有sockfd注册过这个事件。*/
    }
}

// [连接建立]
void TcpConnection::connectEstablished()
{
    setState(kConnected); //设置连接状态:connecting->conneted
    /*tie是channel里面的一个void型的weak_ptr ;
    tcpconnection底层管理了一个channel，channel被poller通知后调用相应的回调函数，这些回调也都是tcpconnection里面的函数。
    假设channel在poller上面还注册着，得到poller的事件通知调用响应的回调函数，但是tcpconnection对象没有了，而那些成员函数bind时候绑定的就是tcpconnection对象，所以再调用就会产生未知效果。
    所以：channel里面有一个void型的weakptr，有一个成员函数tie，再tcp connection新连接创建的时候，调用tie函数，channel的弱智能指就会指向tecpconnection对象。
    这样就防止了底层channel被poller通知执行回调函数时候，channel上层的tcpconnection对象不会被remove。
    */
    channel_->tie(shared_from_this());
    //建立连接的时候调用，tceconneciton的connectionEstablisehd函数中调用channel的tie函数。
    channel_->enableReading(); // 【向poller注册channel的epollin读事件】

    // 新连接建立，执行connectionCallback回调
    connectionCallback_(shared_from_this());
}

//[连接销毁]
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected) // connected->disconnected
    {
        setState(kDisconnected);
        channel_->disableAll(); // [把channel的所有感兴趣的事件，从poller中del掉]
        connectionCallback_(shared_from_this());
        //调用connectionCallback
    }
    channel_->remove(); // 把channel从poller中删除掉
}

void TcpConnection::handleRead(Timestamp receiveTime) //处理数据可读
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    // channel->fd和socket->fd是相同的，这里选择channel->fd
    if (n > 0)
    {
        // [已建立连接的用户，有可读事件发生了，调用用户传入的回调操作onMessage]
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
        //这里shared_from_this就是获取了当前tcpconnection对象的智能指针
    }
    else if (n == 0) //读到0表示对端关闭，那么调用handleColose处理即可
    {
        handleClose();
    }
    else //出错了
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if (channel_->isWriting()) //可写
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if (n > 0) //发送了n个数据
        {
            outputBuffer_.retrieve(n);              // [n个数据已经处理过了，重置outputBuffer的readindex]
            if (outputBuffer_.readableBytes() == 0) //发送完成，
            {
                channel_->disableWriting(); //[设置为不可写，因为上面可写的时候已经写完数据了]
                if (writeCompleteCallback_) //写完成回调
                {
                    // [唤醒loop_对应的thread线程，执行回调]
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting) //正在关闭状态
                {
                    shutdownInLoop();
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n", channel_->fd());
    }
}

// poller => channel::closeCallback => TcpConnection::handleClose
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected); //设置连接状态为关闭
    channel_->disableAll();  // channel对所有事件都不感兴趣了，从poller中删除
    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr); // 执行连接关闭的回调
    closeCallback_(connPtr);
    // 关闭连接的回调  执行的是TcpServer::removeConnection回调方法
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}
