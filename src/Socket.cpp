#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <cstring>
#include "Socket.hpp"
#include "InetAddress.hpp"
#include "MyLog.hpp"

Socket::~Socket() { close(sockfd_);}

void Socket::bindAddress(const InetAddress &localAddr)
{
    if (0 != bind(sockfd_, (sockaddr*)localAddr.getSockAddr(), sizeof(sockaddr_in)))
    {
        mylog::GetLogger("asynclogger")->Fatal("bind sockfd: %d fail", sockfd_);
    }
}

void Socket::Listen()
{
    if (0 != listen(sockfd_, 1024))
    {
        mylog::GetLogger("asynclogger")->Fatal("listen sockfd: %d fail", sockfd_);
    }
}

// 连接客户端并将客户端信息填入peerAddr中,同时返回连接fd
int Socket::Accept(InetAddress* peerAddr)
{
    sockaddr_in client;
    bzero(&client, sizeof(client));
    socklen_t client_len;
    int connfd = accept4(sockfd_, (sockaddr*)&client, &client_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd >= 0) peerAddr->setSockAddr(client);
    return connfd;
}

void Socket::shutdownWrite()
{
    if (shutdown(sockfd_, SHUT_WR) < 0)
        mylog::GetLogger("asynclogger")->Error("shudownWrite error");
}

// 设置是否禁用Nagle算法，Nagle算法通过合并小数据包来减少网络上的小包数量，提高效率，但会增加延迟
// 1为禁止，0为启用
void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

// 设置地址复用
void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)); 
}

void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)); 
}

// 设置TCP保活机制，启用后，TCP会定期发送“保活”探测包来检查空闲连接的另一端是否仍然存活。
//如果对方无响应，连接会被关闭
void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)); 
}