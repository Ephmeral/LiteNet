#include "TCPConnection.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Socket.h"

TCPConnection::TCPConnection(EventLoop *loop,
                             const std::string &nameArg,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(loop), name_(nameArg), state_(kConnecting), reading_(true), socket_(new Socket(sockfd)), channel_(new Channel(loop, sockfd)), localAddr_(localAddr), peerAddr_(peerAddr), highWaterMark_(64 * 1024 * 1024) {
    // 给channel设置相应的回调函数，Poller通知Channel感兴趣的事件发生了，Channel会回调相应的操作
    channel_->setReadCallback(std::bind(&TCPConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TCPConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TCPConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TCPConnection::handleError, this));

    LOG_INFO("TCPConnection::ctor[%s] at fd = %d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TCPConnection::~TCPConnection() {
    LOG_DEBUG("TCPConnection::dtor[%s] at fd = %d, state = %d\n", name_.c_str(), channel_->fd(), int(state_));
}

void TCPConnection::send(const std::string &buf) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(buf.c_str(), buf.length());
        } else {
            loop_->runInLoop(std::bind(
                &TCPConnection::sendInLoop,
                this,
                buf.c_str(),
                buf.size()));
        }
    }
}

void TCPConnection::sendInLoop(const void *data, size_t len) {
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    // 之前调用过connection的shutdown，不能在发送数据了
    if (state_ == kDisconnected) {
        LOG_ERROR("disconnected, give up writing!");
        return;
    }

    // channel第一次发送数据，并且缓冲区没有数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0) {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_) {
                // 
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        } else { // nwrote < 0
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                LOG_ERROR("TCPConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET) {
                    faultError = true;
                }
            }
        }
    } 

    // 当前这一次write，没有将数据全部发送出去，剩余数据需要保存到缓冲区中
    // channel注册epollout事件，poller发送tcp的发送缓冲区有空间
    // 会通知相应的sock channel，调用handleWrite回调方法
    // 即调用TCPConnection::handleWrite方法，把发送缓冲区数据全部发送完成
    if (!faultError && remaining > 0) {
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_
            && oldLen < highWaterMark_
            && highWaterMarkCallback_) {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, shared_from_this(), oldLen - remaining)
            );
        }
        outputBuffer_.append((char*)data + nwrote, remaining);
        if (!channel_->isWriting()) {
            // 注册channel的写事件
            channel_->enableWriting();
        }
    }
}

// 关闭连接
void TCPConnection::shutdown() {
    if (state_ == kConnected) {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TCPConnection::shutdownInLoop, this));
    }
}

void TCPConnection::shutdownInLoop() {
    if (!channel_->isWriting()) {
        socket_->shutdownWrite();
    }
}

// 连接建立
void TCPConnection::connectEstablished() {
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading(); // 向poller注册channel的epollin事件

    connectionCallback_(shared_from_this());
}

// 连接销毁
void TCPConnection::connectDestoryed() {
    if (state_ == kConnected) {
        setState(kDisconnected);
        channel_->disableAll();

        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}

void TCPConnection::handleRead(Timestamp receiveTime) {
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0) {
        // 已建立连接的用户，有可读事件发生了，调用用户传入的处理回调
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    } else if (n == 0) {
        handleClose();
    } else {
        errno = savedErrno;
        LOG_ERROR("TCPConnection::handleRead\n");
        handleError();
    }
}

void TCPConnection::handleWrite() {
    if (channel_->isWriting()) {
        int saveErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);
        if (n > 0) {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0) {
                channel_->disableWriting();
                if (writeCompleteCallback_) {
                    // 唤醒loop_对应thread线程，执行回调
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting) {
                    shutdownInLoop();
                }
            }
        } else {
            LOG_ERROR("TCPConnection::handleWrite\n");
        }
    } else {
        LOG_ERROR("TCOConnection fd=%d is down, no more writing\n", channel_->fd());
    }
}

void TCPConnection::handleClose() {
    LOG_INFO("fd = %d state = %d\n", channel_->fd(), int(state_));
    setState(kDisconnected);
    channel_->disableAll();

    TCPConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);  // 执行连接关闭的回调
    closeCallback_(connPtr);       // 关闭连接的回调
}

void TCPConnection::handleError() {
    int optval, err = 0;
    socklen_t optlen = sizeof(optval);
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen)) {
        err = errno;
    } else {
        err = optval;
    }
    LOG_ERROR("TCPConnection::hanlerError name:%s - SO_ERROR:%d\n", name_.c_str(), err);
}
