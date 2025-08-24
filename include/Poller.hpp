#pragma once

#include <vector>
#include <unordered_map>
#include "noncopyable.hpp"
#include "Timestamp.hpp"

class Channel;
class EventLoop;

/*
* muduo网络库的核心组件，主要作用是​​封装 I/O 多路复用机制​，用于高效地监听和管理多个文件描述符（File Descriptor）上的事件。
* Poller 是 muduo 实现 ​​Reactor 模式​​的关键部分，负责将就绪的 I/O 事件分发给对应的处理单元（Channel）。  
*/
class Poller
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop *loop);
    virtual ~Poller() = default;

    // IO复用接口
    // timeoutMs为epoll_wait的超时时间
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    // 判断参数的Channel是否在当前的Poller中
    bool hasChannel(Channel* Channel) const;

    // EventLoop可以通过该接口获取默认的IO复用的具体实现
    static Poller *newDefaultPoller(EventLoop* loop);

protected:
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;

private:
    EventLoop* ownerLoop_; // Poller所属的事件循环EventLoop;
};