#pragma once

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>
#include "EventLoop.hpp"
#include "Acceptor.hpp"
#include "InetAddress.hpp"
#include "noncopyable.hpp"
#include "EventLoopThreadPool.hpp"
#include "Callbacks.hpp"
#include "TcpConnection.hpp"
#include "Buffer.hpp"

// 实际的对外接口类
class TcpServer
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option
    {
        kNoReusePort,
        KReusePort,
    };
    
    TcpServer(EventLoop *loop,
              const InetAddress &listenAddr,
              const std::string &nameAeg,
              Option option = kNoReusePort);
    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallbakc(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

    void setThreadNum(int numThreads);

    void start(); // 启动监听
    
private:
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);    

private:
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop *loop_;  //baseLoop

    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;

    std::shared_ptr<EventLoopThreadPool> threadPool_;

    ConnectionCallback connectionCallback_;         //有新连接时的回调函数
    MessageCallback messageCallback_;               //数据处理回调函数
    WriteCompleteCallback writeCompleteCallback_;   //数据发送完成回调函数

    ThreadInitCallback threadInitCallback_;         //线程初始化回调函数
    int numThreads_;                                //线程池线程数量
    std::atomic_int started_;
    int nextConnId_;
    ConnectionMap connections_;                     //保存所有连接 
};