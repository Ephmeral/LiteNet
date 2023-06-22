#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory>
#include <sys/eventfd.h>

// 赋值一个线程创建多个EventLoop
__thread EventLoop *t_loopInThisThread = nullptr;

// 默认Poller IO复用接口超时时间
const int kPollTimeMs = 100000;

// 创建wakeupfd，通过notify唤醒subReactor处理新来的Channel
int createEventfd() {
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        LOG_FATAL("EventLoop::eventfd error:%d\n", error);
    }
    return evtfd;
}

EventLoop::EventLoop() 
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_))
    , currentActiveChannel_(nullptr) {

    LOG_DEBUG("EventLoop::EventLoop created %p in thread %d\n", this, threadId_);
    if (t_loopInThisThread) {
        LOG_FATAL("EventLoop::Another EventLoop %p exists in this thread %d\n", t_loopInThisThread, threadId_);
    } else {
        t_loopInThisThread = this;
    }

    // 设置wakeupfd事件类型和事件发生后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每一个EventLoop都将监听wakeupChannel的EPOLLIN读事件
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop() {
    LOG_INFO("EventLoop::EventLoop %p start looping\n", this);
    looping_ = true;
    quit_ = false;

    while (!quit_) {
        activeChannels_.clear();
        // 监听client的fd以及wakeupFD
        pollReturnTime_ = poller_->poll(kPollTimeMs, activeChannels_);
        for (Channel *channel : activeChannels_) {
            // Poller监听哪些Channel发送事件了，然后上报给EventLoop，通知Channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前EventLoop事件循环需要的回调操作
        doPendingFunctors();
    }
    LOG_INFO("EventLoop::EventLoop %p stop loop\n", this);
    looping_ = false;
}
// 退出事件循环 
// 1.loop在自己线程调用quit
// 2.在非loop的线程中，调用loop的quit
void EventLoop::quit() {
    quit_ = true;

    // 其他线程调用的quit，需要进行唤醒
    // 在一个subloop(woker)中调用了mainLoop(IO)的quit
    if (!isInLoopThread()) {
        wakeup();
    }
}


// 在当前loop中执行cb
void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        // 在当前线程中，执行cb
        cb();
    } else {
        // 在非当前线程中执行cb，需要唤醒loop所在线程，执行cb
        queueInLoop(std::move(cb));
    }
}

// 把cb放入队列中，唤醒loop所在线程，执行cb
void EventLoop::queueInLoop(Functor cb) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒相应的，需要执行上面回调操作的loop的线程
    // callingPendingFunctors_为true表示当前loop正在执行回调，但是loop又有了新的回调
    if (!isInLoopThread() || callingPendingFunctors_) {
        // 唤醒loop所在线程
        wakeup(); 
    }
}

// 唤醒loop所在线程
// 向wakeupFd_写一个数据，wakeupChannel就发生了读事件，当前的线程就会被唤醒
void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR("EvnetLoop::wakeup() writes %lu bytes instead of 8\n", n);
    }
}

void EventLoop::updateChannel(Channel *channel) {
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel) {
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel) {
    return poller_->hasChannel(channel);
}

void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR("EventLoop::wakeup() writes %ld bytes instread of 8\n", n);
    }
}

// 执行回调
void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const Functor& functor : functors) {
        functor(); // 执行当前loop需要执行的回调操作
    }
    callingPendingFunctors_ = false;
}