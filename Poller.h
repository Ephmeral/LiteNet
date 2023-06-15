#pragma once

#include <unordered_map>
#include <vector>

#include "Channel.h"
#include "EventLoop.h"
#include "Timestamp.h"
#include "nocopyable.h"

// 多路事件分发器的核心IO复用模块
class Poller : nocopyable {
public:
    using ChannelList = std::vector<Channel *>;

    Poller(EventLoop *loop);
    virtual ~Poller();

    // 给所有IO复用保留统一的接口
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removechannel(Channel *channel) = 0;

    // 判断参数Channel是否在当前Poller当中
    bool hasChannel(Channel *channel) const;

    // EventLoop可以通过该接口获取默认的IO复用的具体实现
    static Poller *newDefaultPoller(EventLoop *loop);

protected:
    // key: sockfd, value: sockfd所属的通道类型
    using ChannelMap = std::unordered_map<int, Channel *>;
    ChannelMap channels_;

private:
    EventLoop *owenrLoop_;
};