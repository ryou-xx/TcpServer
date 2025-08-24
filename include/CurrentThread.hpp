#pragma once
#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread
{
    //__thread是 GCC 编译器提供的一个扩展关键字，用于定义线程局部存储（Thread-Local Storage, TLS）变量。​
    //这意味着每个线程都拥有该变量的一个独立副本，一个线程对它的修改不会影响其他线程中的同名变量。
    extern __thread int t_cachedTie; // 保存tid缓存 因为系统调用非常耗时 拿到tid后将其保存

    void cacheTid();

    inline int tid()
    {
        // __builtin_expect 是一种底层优化 此语句意思是如果还未获取tid,则通过cacheTid()系统调用获取tid
        if (__builtin_expect(t_cachedTie == 0, 0))
            cacheTid();
        return t_cachedTie;
    }
}