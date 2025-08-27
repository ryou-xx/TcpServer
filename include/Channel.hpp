#pragma once

#include <functional>
#include <memory>
#include "noncopyable.hpp"
#include "Timestamp.hpp"

class EventLoop;

/*
* 封装sockfd以及其想要监听的事件类型，如EPOLLIN或EPOLLOUT，同时绑定了poller返回的具体事件
*/
class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>; // 通用事件的回调函数
    using ReadEventCallback = std::function<void(Timestamp)>; // 读事件回调函数

    Channel(EventLoop *loop, int fd);
    ~Channel(){};

    // fd收到Poller的通知后调用该函数进行相应处理，在EventLoop::loop()中被调用
    void handleEvent(Timestamp receiveTime);

    // 设置不同事件对应的回调函数
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb);}
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb);}
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb);}
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb);}

    // 用于防止channel还在进行回调操作时被手动remove
    void tie(const std::shared_ptr<void> &);

    // 返回对应类成员变量的值
    int fd() const {return fd_;}
    int events() const {return events_;}

    bool isNoneEvent() const { return events_ == noneEvent;}
    bool isWriting() const { return events_ & writeEvent;}
    bool isReading() const { return events_ & readEvent;}

    int index() {return index_;}

    // one loop per thread
    EventLoop* ownerLoop() { return loop_; }
    void remove();

    void set_index(int index) { index_ = index; }

    // 设置实际触发的事件
    void set_revents(int revents) {revents_ = revents;}

    // 设置fd监听的事件
    void enableReading() { events_ |= readEvent; update();}
    void disableReading() { events_ &= ~readEvent; update();}
    void enableWriting() { events_ |= writeEvent; update();}
    void disableWriting() { events_ &= ~writeEvent; update();}
    void disbaleAll() { events_ &= noneEvent; update();}

private:
    void update(); // 更新fd在epollfd上的状态
    void handleEventWithGuard(Timestamp receiveTime);

private:
    static const int noneEvent;
    static const int readEvent;
    static const int writeEvent;

    EventLoop *loop_;   // 事件循环
    const int fd_;      // Poller的监听对象
    int events_;        // fd想要监听的事件
    int revents_;       // Poller返回的实际发生的事件
    int index_;         // 指示Channel的状态：未插入Poller；已插入Poller；已插入Poller但不在epoll上

    std::weak_ptr<void> tie_;
    bool tied_;

    // 事件的回调函数
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};