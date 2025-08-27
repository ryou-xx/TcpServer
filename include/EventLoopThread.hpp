#pragma once
#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>
#include "noncopyable.hpp"
#include "Thread.hpp"

class EventLoop;

// 存储EventLoop及其对应的线程信息
class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop *startLoop();

private:
    // EventLoop对应线程的线程函数
    void threadFunc();

private:
    EventLoop* loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};