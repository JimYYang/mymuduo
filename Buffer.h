#pragma once

#include <vector>
#include <string>
#include <algorithm>

// 网络库底层的缓冲区类型定义
class Buffer
{
public:
    static const size_t kCheapPrepend = 8; // 可以记录数据报的长度
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize),
          readerIndex_(kCheapPrepend),
          writerIndex_(kCheapPrepend)
    {}

    size_t readableBytes() const
    { return writerIndex_ - readerIndex_;}

    size_t writableBytes() const
    { return buffer_.size() - writerIndex_;}

    size_t prependableBytes() const
    { return readerIndex_;}

    // 返回缓冲区中可读数据的起始地址
    const char* peek() const
    {
        return begin() + readerIndex_;
    }

    // onMessage string <- Buffer
    void retrieve(size_t len)
    {
        if (len < readableBytes())
        {
            readerIndex_ += len; // 只读取了可读缓冲区的一部分
        }
        else
        {
            retrieveAll();
        }
    }

    void retrieveAll()
    {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    // 把onMessage函数上报的Buffer数据 转成string类型的数据返回 
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes()); // 可读取数据的长度
    }

    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        // 上面已经把缓冲区可读数据读取 这里需要对缓冲区进行复位操作
        retrieve(len);
        return result;
    }

    // 可写的长度: buffer_.size - writerIndex_
    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len);
        }
    }

    // 把[data, data + len]内存上的数据 添加到writable缓冲区中
    void append(const char *data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    // 从fd中读取数据
    ssize_t readFd(int fd, int *savedErrno);
private:
    char* begin()
    { return &*buffer_.begin();} // vector底层数组首元素的地址 也就是数组的起始地址

    const char* begin() const
    { return &*buffer_.begin();}

    char* beginWrite()
    { return begin() + writerIndex_;}

    const char* beginWrite() const
    { return begin() + writerIndex_;}

    void makeSpace(size_t len)
    {
        /**
         * kCheapPrepend |  reader  | writer |
         * kCheapPrepend |          len        |
         *                 |prependableBytes(可能不止8字节)
         */
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_,
                      begin() + writerIndex_,
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = kCheapPrepend + readable;

        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};