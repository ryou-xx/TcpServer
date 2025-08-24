#include <cerrno>
#include <unistd.h>
#include <cstring>
#include "EpollPoller.hpp"
#include "Channel.hpp"
#include "../log_system/logs_code/MyLog.hpp"

const int NEW = -1;     // Channel还未被添加到Poller中
const int ADDED = 1;    
const int DELETED = 2;  // Channel仍然在Poller的Channel列表中，但是已经从epollfd中移除，即不再监听

EpollPoller::EpollPoller(EventLoop* loop)
    : Poller(loop), 
      epollfd_(epoll_create1(EPOLL_CLOEXEC)),
      events_(EventList{initEventListSize})
{
    if (epollfd_ < 0)
        mylog::GetLogger("asynclogger")->Fatal("epoll_create error: %s", strerror(errno));
}

EpollPoller::~EpollPoller()
{
    close(epollfd_);
}

Timestamp EpollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
#ifdef DEBUG_FLAG
    mylog::GetLogger("asynclogger")->Debug("func: %s => total count: %lu", __FUNCTION__, channels.size());
#endif

    int numEvents = epoll_wait(epollfd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now()); // 获取当前时间

    if (numEvents > 0)
    {
#ifdef DEBUG_LOG
        mylog::GetLogger("asynclogger")->Debug("%d events happends", numEvents);
#endif
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size())
            events_.resize(events_.size() * 2);
    }
    else
    {
        if (saveErrno != EINTR)
        {
            errno = saveErrno;
            mylog::GetLogger("asynclogger")->Error("epoll_wait error: %s", strerror(errno));
        }
    }
    return now;
}

void EpollPoller::updateChannel(Channel* channel)
{
    const int index = channel->index();
    mylog::GetLogger("asynclogger")->Info(
        "func: %s => fd: %d event: %d index: %d", __FUNCTION__, channel->fd(), channel->events(),index);
    
    if (index == NEW || index == DELETED)
    {
        // 将未加入的Channel加入Poller的Channel列表中
        if (index == NEW)
            channels_[channel->fd()] = channel;
        channel->set_index(ADDED);
        update(EPOLL_CTL_ADD, channel);
    }
    else // 若Channel不需要监听任何事件，则将其从epoll中删除， 否则更新状态
    {
        if (channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(DELETED);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// 从Poller中删除Channel
void EpollPoller::removeChannel(Channel* channel)
{
    int fd = channel->fd();
    channels_.erase(fd);
    mylog::GetLogger("asynclogger")->Info("func: %s => fd: %d", __FUNCTION__, fd);

    if (ADDED == channel->index()) update(EPOLL_CTL_DEL, channel);
    channel->set_index(NEW);
}

// 填写活跃的连接,即有事件需要处理的Channel,并设置Channel的revent
void EpollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const
{
    for (int i = 0; i < numEvents; ++i)
    {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
    }
}

void EpollPoller::update(int operation, Channel* channel)
{
    epoll_event event;
    bzero(&event, sizeof(event));

    int fd = channel->fd();
    event.events = channel->events();
    event.data.ptr = channel;

    if (epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
            mylog::GetLogger("asynclogger")->Error("epoll_ctl del error: %s", strerror(errno));
        else
            mylog::GetLogger("asynclogger")->Fatal("epoll_ctl add/mod error: %s", strerror(errno));
    }
}