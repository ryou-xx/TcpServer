#include "Thread.hpp"
#include "CurrentThread.hpp"
#include <semaphore.h> // 信号量

std::atomic_int Thread::numCreated_(0);

// 仅初始化将要创建的线程的部分信息，并不实际创建线程
Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false), joined_(false), tid_(0), func_(std::move(func)), name_(name)
{
    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_)
    {
        thread_->detach();
    }
}

// 创建线程
void Thread::start()
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0); // 线程间共享，初始值为0

    thread_ = std::make_shared<std::thread>([&](){
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        func_();
    });

    sem_wait(&sem); // 等待线程创建并成功获取tid
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
        name_ = "Thread" + std::to_string(num);
}


