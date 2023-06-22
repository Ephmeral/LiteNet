#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include <sys/epoll.h>

const int Channel::kNoneEvent{0};
const int Channel::kReadEvent{EPOLLIN | EPOLLPRI};
const int Channel::kWriteEvent{EPOLLOUT};

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd) {
}

Channel::~Channel() {
}

// channel的tie什么时候被调用
void Channel::tie(const std::shared_ptr<void> &obj) {
    tie_ = obj;
    tied_ = true;
}

/*
 * 改变Channel所表示fd的events事件后，update负责在poller里面更改fd相应的事件epoll_ctl
 * EventLoop => ChanelList Poller
 */
void Channel::update() {
    // 通过Channel所属的EventLoop，调用Poller的相应方法，注册fd的events事件
    loop_->updateChannel(this);
}

// 在Channel中所属的EventLoop，把当前的Channel删除
void Channel::remove() {
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime) {
    if (tied_) {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard) {
            handleEventWithGuard(receiveTime);
        }
    } else {
        handleEventWithGuard(receiveTime);
    }
}

// 根据Poller通知的Channel发生的具体事件，由Channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime) {
    LOG_INFO("Channel::handleEvent revents:%d\n", revents_);

    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (closeCallback_) {
            closeCallback_();
        }
    }

    if (revents_ & EPOLLERR) {
        if (errorCallback_) {
            errorCallback_();
        }
    }

    if (revents_ & (EPOLLIN | EPOLLPRI)) {
        if (readCallback_) {
            readCallback_(receiveTime);
        }
    }

    if (revents_ & EPOLLOUT) {
        if (writeCallback_) {
            writeCallback_();
        }
    }
}