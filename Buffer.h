#pragma once

#include <vector>
#include <string>
#include <algorithm>

// [网络库底层的缓冲器类型定义]
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;   //记录数据包的大小的空间
    static const size_t kInitialSize = 1024; //缓冲区的长度

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize),
          readerIndex_(kCheapPrepend),
          writerIndex_(kCheapPrepend)
    {
    }

    size_t readableBytes() const //可读数据长度
    {
        return writerIndex_ - readerIndex_;
    }

    size_t writableBytes() const //可写缓冲区长度
    {
        return buffer_.size() - writerIndex_;
    }

    size_t prependableBytes() const //
    {
        return readerIndex_;
    }

    // [返回缓冲区中可读数据的起始地址]
    const char *peek() const
    {
        return begin() + readerIndex_;
    }

    // onMessage string <- Buffer
    void retrieve(size_t len)
    {
        if (len < readableBytes())
        {
            readerIndex_ += len;
            // 应用只读取了刻度缓冲区数据的一部分，就是len，
            //还剩下readerIndex_ += len 到 writerIndex_
        }
        else // len == readableBytes()
        {
            retrieveAll();
        }
    }

    void retrieveAll() //数据读取后，缓冲区进行复位操作
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    // 把onMessage函数上报的Buffer数据，转成string类型的数据返回
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes()); // 应用可读取数据的长度
    }

    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len);
        // 上面一句把缓冲区中可读的数据，已经读取出来，这里肯定要[对缓冲区进行复位操作]
        return result;
    }

    // buffer_.size() - writerIndex_ 为可写缓冲区长度，len是即将要写入的数据长度，二者要进行比较
    void ensureWriteableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len); // 【扩容函数】
        }
    }

    // 把[data, data+len]内存上的数据，添加到writable缓冲区当中
    void append(const char *data, size_t len)
    {
        ensureWriteableBytes(len);//确保底层缓冲区空间足够写，不能写会进行扩容
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    char *beginWrite()
    {
        return begin() + writerIndex_;
    }

    const char *beginWrite() const
    {
        return begin() + writerIndex_;
    }
//把读取数据和写数据的操作封装到buffer类里面了
    // 【从fd上读取数据】
    ssize_t readFd(int fd, int *saveErrno);
    // 】通过fd发送数据】
    ssize_t writeFd(int fd, int *saveErrno);

private:
    char *begin()
    {
        // vector底层数组首元素的地址，也就是数组的起始地址
        return &*buffer_.begin(); //&与*不能抵消，背后调用二个函数
    }
    const char *begin() const
    {
        return &*buffer_.begin();
    }
    void makeSpace(size_t len) //底层vector空间不够了，需要扩容操作
    {
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            /*底层缓冲区的构成： kCheapPrepend | reader | writer
             */
            buffer_.resize(writerIndex_ + len); //调用vector底层的扩容函数
        }
        else
        {
            //把后面没有读到的数据往前挪一挪（前面空间的数据已经被读了）
            size_t readalbe = readableBytes();
            std::copy(begin() + readerIndex_,
                      begin() + writerIndex_,
                      //把前面区间的数据拷贝到后面开始的位置
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readalbe; //readalbe是还没读取数据的长度
        }
    }

    std::vector<char> buffer_; // char型的vector作为缓冲器底层的数据结构
    size_t readerIndex_;       //数据可读的下标
    size_t writerIndex_;       //数据可写的下标
};
