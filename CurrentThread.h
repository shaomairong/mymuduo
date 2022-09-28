#pragma once

#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread
{
    extern __thread int t_cachedTid;

    void cacheTid();

    inline int tid()
    {
        if (__builtin_expect(t_cachedTid == 0, 0)) //还没有获取过当前线程的id
        {
            cacheTid();
        }
        return t_cachedTid;
        /* __builtin_expect(EXP,N) 意思是EXP==N的概率很大，允许程序员将最有可能执行的分支告诉编译器
            ，这样编译器可以对代码进行优化，减少指令跳转带来的性能下降。*/
    }
}
