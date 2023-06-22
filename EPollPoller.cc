#include "EPollPoller.h"
#include "Logger.h"

#include <cstring>
#include <unistd.h>
#include <cassert>

// channel未添加到Poller中
const int kNew = -1;  // channel的成员index_ = -1
// channel已添加到Poller中
const int kAdded = 1;
// channel从Poller中删除
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)), events_(kInitEventListSize) {
    if (epollfd_ < 0) {
        LOG_FATAL("EPollPoller::epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller() {
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList &activeChannels) {
    LOG_INFO("EPollPoller::%s() => fd total count:%lu\n", __FUNCTION__, channels_.size());
    
    int numEvents = ::epoll_wait(epollfd_, &(*events_.begin()), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;

    Timestamp now(Timestamp::now());
    
    if (numEvents > 0) {
        LOG_INFO("EPollPoller::poll() %d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    } else if (numEvents == 0) {
        LOG_DEBUG("EPollPoller::%s timeout!\n", __FUNCTION__);
    } else {
        if (saveErrno != EINTR) {
            errno = saveErrno;
            LOG_ERROR("EPollerPoller::poll() error!");
        }
    }
    return now;
}

void EPollPoller::updateChannel(Channel *channel) {
    const int index = channel->index();
    LOG_INFO("EPollPoller::%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);

    if (index == kNew || index == kDeleted) {
        if (index == kNew) {
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    } else {
        // channel已经在Poller上注册过了
        int fd = channel->fd();
        if (channel->isNoneEvent()) {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        } else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPollPoller::removeChannel(Channel *channel) {
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("EPollPoller::%s => fd=%d \n", __FUNCTION__, channel->fd());

    int index = channel->index();
    if (index == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList &activeChannels) {
    assert(static_cast<size_t>(numEvents) <= events_.size());
    for (int i = 0; i < numEvents; ++i) {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        assert(channel != nullptr);
        channel->set_revents(events_[i].events);
        // EventLoop拿到它的poller给它返回的所有发生事件的channel列表了
        activeChannels.push_back(channel); 
    }
}

// 更新Channel通道
void EPollPoller::update(int operation, Channel *channel) {
    LOG_INFO("EPoller::update() chennel");
    epoll_event event;
    memset(&event, 0, sizeof(event));

    int fd = channel->fd();

    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
            LOG_ERROR("EPollPoller::epoll_ctr del error:%d\n", errno);
        } else {
            LOG_FATAL("EPollPoller::epoll_ctl add/mod/del error:%d\n", errno)
        }
    }
}
