#include <mymuduo/TcpServer.h>
#include <mymuduo/Logger.h>

#include <string>
#include <functional>

class EchoServer
{
public:
    EchoServer(EventLoop *loop,
               const InetAddress &addr,
               const std::string &name)
        : server_(loop, addr, name), loop_(loop)
    {
        // 【注册回调函数】
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1));

        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this,
                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 【设置合适的loop线程数量 loopthread】
        server_.setThreadNum(3);
    }
    void start()
    {
        server_.start();
    }

private:
    // 【连接建立或者断开的回调】
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            LOG_INFO("Connection UP : %s", conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("Connection DOWN : %s", conn->peerAddress().toIpPort().c_str());
        }
    }

    // 【可读写事件回调】
    void onMessage(const TcpConnectionPtr &conn,
                   Buffer *buf,
                   Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        std::transform(msg.begin(), msg.end(), msg.begin(), ::toupper);
        /*将string的大小写进行转换，string类没有提供这个方法，可以自己写个函数，
        STL的algorithm库可以使用transfrom解决，提供函数对象:
        比如char转换成大写的toupper或者小写的tolower函数。
         */
        conn->send(msg);
        // conn->shutdown(); // 写端关闭   EPOLLHUP =》 closeCallback_
        //如果有耗时的操作可以单独开线程处理，比如传输文件等。
    }

    EventLoop *loop_;  // main loop(base loop)用户创建的
    TcpServer server_; // TcpServer对象
};

int main()
{
    EventLoop loop;
    InetAddress addr(8000); // ip默认127.0.0.1
    EchoServer server(&loop, addr, "EchoServer-01");
    // Acceptor -> non-blocking listenfd   -> create bind
    server.start();
    // listen  -> loopthread  listenfd => acceptChannel => mainLoop =>
    loop.loop();
    // 启动mainLoop的底层Poller

    return 0;
}
