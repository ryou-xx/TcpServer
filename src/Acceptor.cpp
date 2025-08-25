#include <sys/types.h>
#include <sys/socket.h>
#include <cerrno>
#include <unistd.h>
#include "Acceptor.hpp"
#include "InetAddress.hpp"
#include "MyLog.hpp"

// 创建一个非阻塞的socket
static int createNonblocking()
{
    int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0)
        mylog::GetLogger("asynclogger")->Fatal("creatNonblocking error: %s", strerror(errno));
    return sockfd;
}

Acceptor::Acceptor(EventLoop* loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop), acceptSocket_(createNonblocking()), acceptChannel_(loop, acceptSocket_.fd()), listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disbaleAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.Listen();
    acceptChannel_.enableReading(); // 将监听socket注册到epoll中
}

void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.Accept(&peerAddr);
    if (connfd >= 0)
    {
        if (NewConnectionCallback_)
        {
            NewConnectionCallback_(connfd, peerAddr);
        }
        else // 没有处理连接的回调函数，直接关闭新链接
        {
            close(connfd);
        }
    }
    else
    {
        mylog::GetLogger("asynclogger")->Error("accept error: %s", strerror(errno));
    }
}