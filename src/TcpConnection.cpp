#include <functional>
#include <string>
#include <cerrno>
#include <sys/types.h>
#include <sys/socket.h>
#include <cstring>
#include <netinet/tcp.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <unistd.h>
#include "TcpConnection.hpp"
#include "Socket.hpp"
#include "Channel.hpp"
#include "EventLoop.hpp"
#include "MyLog.hpp"

static EventLoop* CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
        mylog::GetLogger("asynclogger")->Fatal("main loop is null");
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
                             const std::string &nameArg,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop)),
      name_(nameArg),
      state_(kConnecting),
      reading_(true),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64 * 1024 * 1024) // 64M
{
    // 将TcpConnection的成员函数作为Channel的回调函数
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError, this));

    mylog::GetLogger("asynclogger")->Info("TcpConnectin::ctor[%s] at fd = %d", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    mylog::GetLogger("asynclogger")->Info("TcpConnection::dtor[%s] at fd=%d state=%d\n", name_.c_str(), channel_->fd(), (int)state_);
}

void TcpConnection::send(const std::string &buf)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->queueInLoop(
                std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
        }
    }
}

// 发送数据
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    // 断开连接则直接返回
    if (state_ == kDisconnected)
    {
        mylog::GetLogger("asynclogger")->Error("disconnected, give up writing");
        return;
    }

    // 该频道没有在监听写事件且输出缓冲区没有待发送数据，说明现在内核缓冲区有空间可以写入数据
    // 此时可以直接调用write
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = write(channel_->fd(), data, len);
        if (nwrote >= 0)
        {
            remaining -= nwrote;
            // 全部发送完毕且设置了写完成回调函数
            if (remaining == 0 && writeCompleteCallback_)
            {
                // 将写完成回调函数作为任务交给TcpConnection对应的subLoop完成
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // nwrote < 0
        {
            nwrote = 0;
            // 非正常返回
            if (errno != EWOULDBLOCK)
            {
                mylog::GetLogger("asynclogger")->Error("TcpConnection::sendInLoop error");
                if (errno == EPIPE || errno == ECONNRESET) faultError = true;
            }
        }
    }

    // 没有将数据全部发送出去，此时需要将未发送的数据添加到输出缓冲区中，等待内核缓冲区有空间后
    // 再通过Poller通知相应的Channel，Channel会调用写回调函数将输出缓冲区的数据发送出去
    if (!faultError && remaining > 0)
    {
        size_t oldLen = outputBuffer_.readableBytes(); // 当前输出缓冲区剩余的待发送数据
        // 通过高水位阈值控制数据的发送速率
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMark_)
        {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append((char*)data + nwrote, remaining);
        if (!channel_->isWriting())
        {
            channel_->enableWriting(); // 注册写事件
        }
    }
}

void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting()) // 没有待发送数据
    {
        socket_->shutdownWrite();
    }
}

void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();  // 注册读事件

    connectionCallback_(shared_from_this()); // 执行连接回调
}

void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disbaleAll();  // 注销所有channel的所有事件
        connectionCallback_(shared_from_this());
    }
    channel_->remove(); // 将TcpConnction的Channel从Poller中移除
}

// 读取客户端发送过来的数据
void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0) // 有数据到达
    {
        // 数据处理回调函数
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0) // 客户端断开
    {
        handleClose();
    }
    else
    {
        errno = savedErrno;
        mylog::GetLogger("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if (channel_->isWriting())
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if (n > 0)
        {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if (writeCompleteCallback_)
                {
                    // TcpConnection对象的channel也在loop_中，向其中加入回调任务
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnected)
                {
                    shutdownInLoop(); // 关闭TcpConnection
                }
            }
        }
        else
        {
            mylog::GetLogger("asynclogger")->Error("TcpCOnnection::handleWrite");
        }
    }
    else
    {
        mylog::GetLogger("asynclogger")->Error("TcpConnection fd=%d is down, no more writing", channel_->fd());
    }
}

void TcpConnection::handleClose()
{
    mylog::GetLogger("asynclogger")->Info("TcpConnection::handleClose fd=%d state=%d\n", channel_->fd(), (int)state_);
    channel_->disbaleAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);   // 连接回调
    closeCallback_(connPtr);        // 执行关闭连接的回调，实际调用的是TcpServer中的removeConnection
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof(optval);
    int err = 0;
    if (getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    mylog::GetLogger("asynclogger")->Error("TcpConnection::handleError name:%s - SO_ERROR:%d\n", name_.c_str(), err);
}

// 发送文件，零拷贝操作
void TcpConnection::sendFile(int fileDescriptor, off_t offset, size_t count)
{
    if (connected())
    {
        if (loop_->isInLoopThread())
        {
            sendFileInLoop(fileDescriptor, offset, count);
        }
        else // 如果调用该函数的线程与TcpConnection所在的线程不是同一个线程
        {
            loop_->runInLoop(
                std::bind(&TcpConnection::sendFileInLoop, shared_from_this(), fileDescriptor, offset, count));
        }
    }
    else
    {
        mylog::GetLogger("asynclogger")->Error("TcpConnection::sendfile - not connected");
    }
}

void TcpConnection::sendFileInLoop(int fileDescriptor, off_t offset, size_t count)
{
    ssize_t bytesSent = 0;
    size_t remaining = count;
    bool faultError = false;

    if (state_ == kDisconnected)
    {
        mylog::GetLogger("asynclogger")->Error("disconnected, give ip writing");
        return;
    }

    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        bytesSent = sendfile(socket_->fd(), fileDescriptor, &offset, count);
        if (bytesSent >= 0)
        {
            remaining -= bytesSent;
            if (remaining == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else
        {
            if (errno != EWOULDBLOCK)
            {
                mylog::GetLogger("asynclogger")->Error("TcpConnection::sendFileInLoop");
            }
            if (errno == EPIPE || errno == ECONNRESET)
            {
                faultError = true;
            }
        }
    }

    if (!faultError && remaining > 0)
    {
        // 继续发送剩余部分
        loop_->queueInLoop(
            std::bind(&TcpConnection::sendFileInLoop, shared_from_this(), fileDescriptor, offset, count));
    }
}