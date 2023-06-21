#include "Buffer.h"

#include <errno.h>
#include <unistd.h>
#include <sys/uio.h>
/*
 * 从fd中读取数据， Poller工作在LT模式
 * Buffer缓冲区有大小，但是从fd读取数据，不知道最终的大小
*/
ssize_t Buffer::readFd(int fd, int* saveErrno) {
    char extrabuf[65536];
    struct iovec vec[2];
    const size_t writable = writableBytes();
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt = (writable < sizeof(extrabuf) ? 2 : 1);
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0) {
        *saveErrno = errno;
    } else if (n <= writable) {
        // Buffer的可写缓冲区已经够存储读出来的数据
        writerIndex_ += n;
    } else {
        // extrabuf里面也写入了数据
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }
    return n;
}


ssize_t Buffer::writeFd(int fd, int* saveErrno) {
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0) {
        *saveErrno = errno;
    }
    return n;
}