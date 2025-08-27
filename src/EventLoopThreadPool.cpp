#include <memory>
#include "EventLoopThread.hpp"
#include "EventLoopThreadPool.hpp"
#include "MyLog.hpp"

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop), name_(nameArg), started_(false), numThreads_(0), next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool() {}

// 在线程池中创建EventLoop线程并执行
void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    started_ = true;

    for (int i = 0; i < numThreads_; ++i)
    {
        std::string name = name_ + std::to_string(i);
        threads_.push_back(std::make_unique<EventLoopThread>(cb, name));
        loops_.push_back(threads_[i]->startLoop());
    }

    // 只有baseLoop一个线程
    if (numThreads_ == 0 && cb)
    {
        cb(baseLoop_);
    }
}

EventLoop* EventLoopThreadPool::getNextLoop()
{
    EventLoop* loop = baseLoop_; // 如果只有baseLoop一个线程，那么Channel永远分配给baseLoop

    if (!loops_.empty())
    {
        loop = loops_[next_++];
        if (next_ >= loops_.size()) next_ = 0;
    }

    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
    if (loops_.empty()) return {baseLoop_};
    return loops_;
}