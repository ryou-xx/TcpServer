#pragma once
#include "noncopyable.hpp"
#include "InetAddress.hpp"

// 封装socket
class Socket : noncopyable
{
public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}
    ~Socket();

    int fd() const {return sockfd_;}
    void bindAddress(const InetAddress &localAddr);
    void Listen();
    int Accept(InetAddress* peerAddr);

    void shutdownWrite();

    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);
    
private:
    const int sockfd_;
};