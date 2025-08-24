#include <cstdlib>
#include "Poller.hpp"
#include "EpollPoller.hpp"

Poller* Poller::newDefaultPoller(EventLoop* loop)
{
    // 获取环境变量名对应的值
    if (getenv("MUDUO_USE_POLL"))
        return nullptr;
    else
        return new EpollPoller(loop);
}