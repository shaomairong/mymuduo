#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/**
 * 从fd上读取数据  Poller工作在LT模式:底层数据没有如果没读完，poller会一直触发
 * Buffer缓冲区是有大小的！ 但是从fd上读数据的时候，却不知道tcp数据最终的大小,如何处理？
 *
 */
ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    char extrabuf[65536] = {0}; //【 栈上的内存空间  64K,栈上内存分配很快，出作用域自动释放】
    //[readv和writev二个读写数据的系统调用,与read和write的区别，这里为什么使用呢？高效]
    struct iovec vec[2];                     //二块缓冲区存放数据
    const size_t writable = writableBytes(); // 这是Buffer底层缓冲区剩余的可写空间大小
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf; //[在栈上开辟的额外的空间存储数据的空间]
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    // zr设置：一次最少读64k的数据
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writable) // [Buffer的可写缓冲区已经够存储读出来的数据了]
    {
        writerIndex_ += n;
    }
    else // buffer可写缓冲区写满了，extrabuf里面也写入了数据
    {
        writerIndex_ = buffer_.size();  //设置writerIndex
        append(extrabuf, n - writable); // writerIndex_开始写 n - writable大小的数据
        // append底层会判断buffer缓冲区剩余空间够写数据吗，不够写会进行扩容
        //这样就保证了缓冲区肯定能存放写入的内容，不会浪费额外开辟的内存空间。
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int *saveErrno)
{

    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}
