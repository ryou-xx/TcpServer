#include "EventLoopThread.hpp"
#include "EventLoop.hpp"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr), 
      exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(),
      callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

// 创建线程并通过线程函数绑定一个新的EventLoop
EventLoop *EventLoopThread::startLoop()
{
    thread_.start(); // 创建线程

    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this](){return this->loop_ != nullptr;});
        loop = loop_;
    }
    return loop;
}

void EventLoopThread::threadFunc()
{
    EventLoop loop; // 创建一个EventLoop

    // 调用线程初始化回调函数
    if (callback_)
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }
    loop.loop(); // 启动loop的事件循环
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}