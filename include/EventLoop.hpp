#pragma once
#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include "noncopyable.hpp"
#include "Timestamp.hpp"
#include "CurrentThread.hpp"

class Channel;
class Poller;

// 事件循环类
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();
    // 退出事件循环
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_;}

    // 在当前loop中执行
    void runInLoop(Functor cb);
    // 把上层注册的回调函数放入队列中，唤醒loop所在的线程执行回调函数
    void queueInLoop(Functor cb);

    // 通过eventfd唤醒loop所在的线程
    void wakeup();

    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    // 判断EventLoop对象是否在调用者的线程中
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid();}
private:
    // 给eventfd返回的文件描述符wakeupFd_绑定的事件回调,当wakeup()时,即有事件发生时,调用handleRead()读wakeupFd_的8字节,同时唤醒阻塞的epoll_wait
    void handleRead();
    // 执行上层回调
    void doPendingFunctions();

private:
    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;
    std::atomic_bool quit_;     // 循环退出标志

    const pid_t threadId_;      // 当前EventLoop所属线程id

    Timestamp pollReturnTime_;  // Poller返回发生事件的Channels的时间点
    std::unique_ptr<Poller> poller_;    // 一个EventLoop对应一个Poller

    int wakeupFd_;              // 用于唤醒阻塞在epoll_wait中的Loop线程，因为线程会监听wakeupChannel,在wakeupFd_中写入相当于人为制造了一个写入事件
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activecChannels_;// Poller检测到的当前有事件发生的所有Channel列表

    std::atomic_bool callingPendingFuntors_;    // 表示当前loop是否有需要执行的回调操作
    std::vector<Functor> pengdingFuntors_;      // 存储loop需要执行的所有回调操作
    std::mutex mutex_;                          // 保护vector的线程安全操作
};