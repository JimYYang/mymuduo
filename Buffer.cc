#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>


/**
 * 从fd上读取数据 Poller工作在LT模式
 * Buffer缓冲区是有大小的 但是从fd上读数据的时候 却不知道tcp数据最终的大小
 *  
*/
ssize_t Buffer::readFd(int fd, int *savedErrno)
{
    char extrabuf[65536] = {0}; // 栈上内存 64k
    struct iovec vec[2];
    const size_t writable = writableBytes(); // 这是Buffer底层缓冲区剩余的可写空间大小
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *savedErrno = errno;
    }
    else if (n <= writable) // Buffer的可写缓冲区已经足够存储fd读出来的数据
    {
        writerIndex_ += n;
    }
    else // extrabuf也写入的数据
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable); // 从writerIndex开始写n - writable个数据
    }
    return n;
}