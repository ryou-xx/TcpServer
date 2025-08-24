#pragma once
#include <vector>
#include <sys/epoll.h>
#include "Timestamp.hpp"
#include "Poller.hpp"

class EpollPoller : public Poller
{
public:
    EpollPoller(EventLoop* loop);
    ~EpollPoller();

    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:
    static const int initEventListSize = 16;

    // 填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
    // 通过epoll_ctl更新Channel的监听事件
    void update(int operation, Channel* channel);

private:
    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_; // epoll_wait监听到的事件集合
};