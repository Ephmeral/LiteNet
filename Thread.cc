#include "Thread.h"
#include "CurrentThread.h"
#include <semaphore.h>

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(std::move(name)) {
    setDefaultName();
}

Thread::~Thread() {
    if (started_ && !joined_) {
        thread_->detach(); // thread类提供的方法
    }
}

// Thread对象记录了线程对象的信息
void Thread::start() {
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);

    thread_ = std::shared_ptr<std::thread>(new std::thread([&]{
        // 获取线程的tid
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        // 开启新线程，执行线程函数
        func_(); 
    }));

    // 信号量等待上面的线程
    sem_wait(&sem);
}

void Thread::join() {
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName() {
    int num = numCreated_;
    if (name_.empty()) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Thread%d", num);
        name_ = buf;
    }
}