#include <functional>
#include <string.h>
#include "TcpServer.hpp"
#include "TcpConnection.hpp"
#include "MyLog.hpp"

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        mylog::GetLogger("asynclogger")->Fatal("mainLoop is null");
    }
    return loop;
}

TcpServer::TcpServer(EventLoop* loop,
                     const InetAddress &listenAddr,
                     const std::string &nameArg,
                     Option option)
    : loop_(CheckLoopNotNull(loop)),
      ipPort_(listenAddr.toIpPort()),
      name_(nameArg),
      acceptor_(new Acceptor(loop, listenAddr, option == KReusePort)),
      threadPool_(new EventLoopThreadPool(loop, name_)),
      connectionCallback_(),
      messageCallback_(),
      nextConnId_(1),
      started_(0)
{
    // 有新用户连接时，Acceptor类中绑定的acceptChannel_会有读事件发生，
    //handleRead()实际调用了TcpServer::newConnection
    acceptor_->setNewConnectionCallback(
            std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for (auto &item : connections_)
    {
        TcpConnectionPtr conn(item.second);
        item.second.reset(); // 释放item中指向TcpConnection的智能指针
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

void TcpServer::setThreadNum(int numThreads)
{
    int numThreads_ = numThreads_;
    threadPool_->setThreadNum(numThreads_);
}

// 开启服务器监听
void TcpServer::start()
{
    // 原子地将一个值加到原子对象上，并返回该原子对象在加法操作之前所持有的值
    // 也就是会所只有第一次加1的时候才会进入条件语句中
    if (started_.fetch_add(1) == 0)
    {
        threadPool_->start(threadInitCallback_);    // 启动底层的loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// acceptor处理新连接的回调函数
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    EventLoop *ioLoop = threadPool_->getNextLoop();
    std::string connName = name_ + "-" + ipPort_ + std::to_string(nextConnId_++);

    mylog::GetLogger("asynclogger")->Info("TcpServer::newConnection [%s] - new connection [%s] from %s\n",
            name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());
    
    // 通过sockfd获取其绑定的本机的ip地址和端口信息
    sockaddr_in local;
    bzero(&local, sizeof(local));
    socklen_t addrlen = sizeof(local);
    if (getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0)
    {
        mylog::GetLogger("asyclogger")->Error("getsockname error");
    }
    InetAddress localAddr(local);
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn;

    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    mylog::GetLogger("asynclogger")->Info("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
            name_.c_str(), conn->name().c_str());
    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}