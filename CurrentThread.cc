#include "CurrentThread.h"

namespace CurrentThread {
    void cacheTid() {
        if (t_cachedTid == 0) {
            // 通过系统调用，获取当前线程的tid
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}