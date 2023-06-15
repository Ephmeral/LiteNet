#include "Poller.h"

Poller::Poller(EventLoop *loop)
    : owenrLoop_(loop) {}

bool Poller::hasChannel(Channel *channel) const {
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}