#include "CurrentThread.hpp"

namespace CurrentThread
{
    __thread int t_cachedTid = 0;

    void cacheTie()
    {
        if (t_cachedTid == 0)
            t_cachedTid = static_cast<pid_t>(syscall(SYS_gettid));
    }
}