#pragma once
#include "Channel.h"
#include "Socket.h"
#include "nocopyable.h"

class EventLoop;
class InetAddress;

class Acceptor {
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &addr)>;
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();
    void setNewConnectionCallback(const NewConnectionCallback &cb) {
        newConnectionCallback_ = std::move(cb);
    }

    bool listenning() const { return listenning_; }
    void listen();

private:
    void handleRead();

    EventLoop *loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
    int idleFd_;
};