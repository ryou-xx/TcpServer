#include <sys/epoll.h>
#include "Channel.hpp"
#include "EventLoop.hpp"
#include "MyLog.hpp"

const int Channel::noneEvent = 0;                  // 无事件
const int Channel::readEvent = EPOLLIN | EPOLLPRI; // 读事件和紧急数据
const int Channel::writeEvent = EPOLLOUT;          // 写事件

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false) {}

// 用于确保Channel的能在正确的时间销毁
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}

// 更新epollfd中Channel对应的事件
void Channel::update()
{
    loop_->updateChannel(this);
}

// 在Channel所属的EvenLoop中把当前的Channel删除
void Channel::remove()
{
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    mylog::GetLogger("asynclogger")->Info("channel handleEvent revents:%d\n", revents_);

    // 关闭事件, 当通过shutdown关闭Channel的写端时触发
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_ != nullptr) closeCallback_();
    }

    // 错误事件
    if (revents_ & EPOLLERR)
    {
        if (errorCallback_ != nullptr) errorCallback_();
    }

    // 读事件
    if (revents_ & readEvent)
    {
        if (readCallback_ != nullptr) readCallback_(receiveTime);
    }

    // 写事件
    if (revents_ & writeEvent)
    {
        if (writeCallback_ != nullptr) writeCallback_();
    }
}