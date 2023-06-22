#include "TCPServer.h"

#include <string.h>


EventLoop *CheckLoopNotNull(EventLoop *loop) {
    if (loop == nullptr) {
        LOG_FATAL("%s:%s:%d mainLoop is null!\n", __FILE__, __FUNCTION__, _LINES__);
    }
    return loop;
}

TCPServer::TCPServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg, Option option)
    : loop_(CheckLoopNotNull(loop))
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option))
    , threadPool_(new EventLoopThreadPool(loop_, name_))
    , connectionCallback_()
    , messageCallback_()
    , nextConnId_(1)
    , started_(0) {
    acceptor_->setNewConnectionCallback(std::bind(&TCPServer::newConnection, this,
                                                  std::placeholders::_1, std::placeholders::_2));
}

TCPServer::~TCPServer() {
    for (auto &item : connections_) {
        // 局部对象的智能指针，作用域结束后会自动释放new出来的TCPConnection对象资源
        TCPConnectionPtr conn(item.second);
        item.second.reset();
        // 销毁连接
        conn->getLoop()->runInLoop(
            std::bind(&TCPConnection::connectDestoryed, conn));
    }
}

// 设置底层subloop的个数
void TCPServer::setThreadNum(int numThreads) {
    threadPool_->setThreadNum(numThreads);
}

// 开启服务器监听
void TCPServer::start() {
    // 赋值一个TCPServer对象被start多次
    if (started_++ == 0) {
        threadPool_->start(threadInitCallback_);
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

void TCPServer::newConnection(int sockfd, const InetAddress &peerAddr) {
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64];
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TCPServer::newConnection [%s] - new connection [%s] from %s\n",
             name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 通过sockfd获取绑定的本地IP地址和端口信息
    sockaddr_in local;
    ::bzero(&local, sizeof(local));
    socklen_t addrlen = static_cast<socklen_t>(sizeof(local));
    if (::getsockname(sockfd, (struct sockaddr *)&local, &addrlen) < 0) {
        LOG_ERROR("sockets::getLocalAddr");
    }

    InetAddress localAddr(local);
    TCPConnectionPtr conn(new TCPConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn;
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    // 设置关闭连接的回调
    conn->setCloseCallback(
        std::bind(&TCPServer::removeConnection, this, std::placeholders::_1));
    // 直接调用TCPConnection::connectEstablished
    ioLoop->runInLoop(std::bind(&TCPConnection::connectEstablished, conn));
}

void TCPServer::removeConnection(const TCPConnectionPtr &conn) {
    loop_->runInLoop(std::bind(&TCPServer::removeConnectionInLoop, this, conn));
}

void TCPServer::removeConnectionInLoop(const TCPConnectionPtr &conn) {
    LOG_INFO("TCPServer::removeConnectionInLoop [%s] - connection %s\n", name_.c_str(), conn->name().c_str());
    size_t n = connections_.erase(conn->name());

    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TCPConnection::connectDestoryed, conn));
}