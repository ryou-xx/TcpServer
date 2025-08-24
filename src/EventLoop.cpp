#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <memory>
#include "EventLoop.hpp"
#include "Channel.hpp"
#include "Poller.hpp"
#include "MyLog.hpp"

// 避免一个线程创建多个EventLoop实例
thread_local EventLoop *t_loopInThisThread = nullptr;

// 定义默认的Poller IO复用接口的超时时间
const int POLLTIMEMS = 10000; // 10s

// 创建eventfd用于唤醒subReactor处理新来的channel
int createEventfd()
{
    int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        mylog::GetLogger("asynclogger")->Fatal("eventfd error: %s", strerror(errno));
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false), 
      callingPendingFuntors_(false), 
      threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_))
{
#ifdef DEBUG_FLAG
    mylog::GetLogger("asynclogger")->Debug("EventLoop created %p in thread %d", this, threadId_);
#endif
    if (t_loopInThisThread == nullptr)
    {
        mylog::GetLogger("asynclogger")->Fatal("another eventloop %p exists in this thread %d", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disbaleAll();
    wakeupChannel_->remove();
    close(wakeupFd_);
}

// 开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    mylog::GetLogger("asynclogger")->Info("EventLoop %p start looping", this);

    while (!quit_)
    {
        activecChannels_.clear(); // 清空活跃事件列表
        pollReturnTime_ = poller_->poll(POLLTIMEMS, &activecChannels_); // 调用epoll_wait获取活跃事件
        for (auto channel : activecChannels_)
        {
            // 通知channel处理事件
            channel->handleEvent(pollReturnTime_);
        }
        /**
         * 执行当前EventLoop事件循环需要处理的回调操作 对于线程数 >=2 的情况 IO线程 mainloop(mainReactor) 主要工作：
         * accept接收连接 => 将accept返回的connfd打包为Channel => TcpServer::newConnection通过轮询将TcpConnection对象分配给subloop处理
         *
         * mainloop调用queueInLoop将回调加入subloop（该回调需要subloop执行 但subloop还在poller_->poll处阻塞） queueInLoop通过wakeup将subloop唤醒
         **/
        doPendingFunctions();
    }
    mylog::GetLogger("asynclogger")->Info("EventLoop %p stop looping", this);
    looping_ = false;
}

/*
* 退出事件循环
* 1. 如果loop在自己的线程中调用quit成功了 说明当前线程已经执行完毕了loop()函数的poller_->poll并退出
* 2. 如果不是当前EventLoop所属线程中调用quit退出EventLoop 需要唤醒EventLoop所属线程的epoll_wait
*/
void EventLoop::quit()
{
    quit_ = true;

    if (!isInLoopThread())
    {
        wakeup();
    }
}

// 在当前loop中执行回调函数
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread())
    {
        cb();
    }
    else
    {
        // 与EventLoop实例不在同一线程，则需要唤醒该EventLoop所在线程执行cb
        queueInLoop(cb);
    }
}

// 把cb放入队列中，并唤醒loop所在的线程执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pengdingFuntors_.emplace_back(cb);
    }

    // 如果Loop所在的线程正在执行其他回调函数，此时为了避免执行完当前回调后再一次循环进入epoll_wait的阻塞等待状态，
    // 导致回调函数无法及时执行，需要唤醒线程进入下一次循环后立即执行回调
    if (!isInLoopThread || callingPendingFuntors_)
    {
        wakeup();
    }
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
        mylog::GetLogger("asynclogger")->Error("wakeup write error");
}

void EventLoop::updateChannel(Channel* channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel)
{
    return poller_->hasChannel(channel);
}

// 执行所有回调函数
void EventLoop::doPendingFunctions()
{
    std::vector<Functor> functors;
    callingPendingFuntors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pengdingFuntors_);
    }

    for (const Functor &functor : functors)
    {
        functor();
    }

    callingPendingFuntors_ = false;
}