#pragma once
#include <functional>
#include <string>
#include <vector>
#include <memory>
#include "noncopyable.hpp"

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    EventLoop* getNextLoop(); // 轮询为将新Channel分配给subLoop

    std::vector<EventLoop*> getAllLoops();

    bool started() const { return started_; }
    const std::string name() const { return name_; }

private:
    EventLoop *baseLoop_;
    std::string name_;
    bool started_;
    int numThreads_;        //线程池中线程的数量
    int next_;              //指向下一个将被分配Channel的线程
    std::vector<std::unique_ptr<EventLoopThread>> threads_; // EventLoop线程列表
    std::vector<EventLoop*> loops_;                         // EventLoop列表，与EventLoop线程对应
};