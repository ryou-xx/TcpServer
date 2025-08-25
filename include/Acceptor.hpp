#pragma once
#include <functional>
#include "noncopyable.hpp"
#include "Channel.hpp"
#include "Socket.hpp"
#include "EventLoop.hpp"

class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();
    // 设置处理新连接的回调函数
    void setNewConnectionCallback(const NewConnectionCallback &cb)
    {
        NewConnectionCallback_ = cb;
    }
    // 查看监听状态
    bool listenning() const {return listenning_;}
    // 监听本地端口
    void listen();
private:
    void handleRead(); // 处理新用户的连接, 如果有新用户连接会调用NewConnectionCallback成员

private:
    EventLoop* loop_;       // mainLoop
    Socket acceptSocket_;   // 监听socket
    Channel acceptChannel_;  // 监听Channel
    NewConnectionCallback NewConnectionCallback_; // 处理新连接的回调函数, 该成员由TcpServer提供
    bool listenning_;       // 监听状态
};