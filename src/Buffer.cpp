#include <cerrno>
#include <sys/uio.h>
#include <unistd.h>
#include "Buffer.hpp"

ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    // 额外栈空间，当从套接字读取的数据超过buffer_当前大小时，先将数据暂存
    // 待buffer_重新分配足够的空间后，在把数据添加到buffer_中
    // 这么做利用了临时栈上空间，避免开巨大Buffer造成的内存浪费，也避免反复调用 read() 的系统开销
    char extrabuf[65536] = {0}; // 64KB

    iovec vec[2];
    const size_t writable = writableBytes(); // 获取剩余可写空间大小

    // 第一块缓冲区指向buffer_的可写空间
    vec[0].iov_base = beginWrite();
    vec[0].iov_len = writable;
    // 第二块缓冲区指向额外栈空间
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    const ssize_t n = readv(fd, vec, iovcnt);

    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writable)
    {
        writerIndex_ += n;
    }
    else
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable); // 扩容并将额外栈区的数据追加到buffer中
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int *saveErrno)
{
    ssize_t n = write(fd, peek(), readableBytes());
    if (n < 0) *saveErrno = errno;
    return n;
}