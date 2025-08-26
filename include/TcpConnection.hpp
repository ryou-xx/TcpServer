#pragma once

#include <memory>
#include <string>
#include <atomic>
#include "noncopyable.hpp"
#include "InetAddress.hpp"
#include "Callbacks.hpp"
#include "Buffer.hpp"
#include "Timestamp.hpp"

class Channel;
class EventLoop;
class Socket;


class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
// 类中使用了shared_from_this，则应确保类的实例都是通过shared_ptr管理的
public:
    TcpConnection(EventLoop *loop,
                  const std::string &nameArg,
                  int sockfd,
                  const InetAddress &localAddr,
                  const InetAddress &peerAddr);
    ~TcpConnection();

    EventLoop *getLoop() { return loop_; }
    const std::string &name() const { return name_; }
    const InetAddress &localAddress() const { return localAddr_; }
    const InetAddress &peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    void send(const std::string &buf);
    void sendFile(int fd, off_t offset, size_t count);

    void shutdown(); // 半关闭

    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }
    void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }
    void setHighWaterMarkback(const HighWaterMarkCallback &cb, size_t highWaterMark) 
    { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark;}

    void connectEstablished();  // 建立连接
    void connectDestroyed();    // 销毁连接
private:
    enum StateE
    {
        kDisconnected,  // 已断开连接
        kConnecting,   // 正在连接
        kConnected,     // 已连接 
        KDisconnecting  // 正在断开连接
    };
    void setState(StateE state) {state_ = state;}

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* data, size_t len);
    void shutdownInLoop();
    void sendFileInLoop(int fileDescriptor, off_t offset, size_t count);
       
private:
    EventLoop* loop_;   // 单Reactor模式：指向mainloop，多Reacto：指向subloop
    const std::string name_;
    std::atomic_int state_;
    bool reading_;      // 表示连接是否在监听读事件

    // 与Acceptor类似
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;         // 有新连接
    MessageCallback messageCallback_;               // 有读写信息
    WriteCompleteCallback writeCompleteCallback_;   // 消息发送完成
    CloseCallback closeCallback_;                   // 关闭连接
    HighWaterMarkCallback highWaterMarkCallback_;   // 高水平回调
    size_t highWaterMark_;                          // 高水位阈值, 用于​​防止发送方因数据发送过快而导致接收方缓冲区溢出

    // 数据缓冲区
    Buffer inputBuffer_;
    Buffer outputBuffer_;
};