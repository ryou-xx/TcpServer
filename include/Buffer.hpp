#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <stddef.h>

// 底层缓冲区类型
class Buffer
{
public:
    // 初始预留的prependable空间大小, 用于后续可能添加的数据信息，比如数据长度
    static const size_t CHEAP_PREPEND = 8; 
    static const size_t INIT_SIZE = 1024;

    explicit Buffer(size_t initialSize = INIT_SIZE)
        : buffer_(CHEAP_PREPEND + INIT_SIZE), readerIndex_(CHEAP_PREPEND), writerIndex_(CHEAP_PREPEND){}
    
    // 查看可读字节数
    size_t readableBytes() const { return writerIndex_ - readerIndex_;}
    // 查看剩余可写空间
    size_t writableBytes() const { return buffer_.size() - writerIndex_;}
    // 返回可覆盖空间的后一个位置
    size_t prependableBytes() const { return readerIndex_;}

    // 查看可读数据的起始地址
    const char* peek() const { return begin() + readerIndex_;}

    // 根据已读取的数据长度移动Index，如果还有未读取的数据，则将readerIndex
    // 向后移动len个单位，指向剩余可读数据的起始位置，若数据已全部读取，则重置两个Index
    void retrieve(size_t len)
    {
        if (len < readableBytes())
        {
            readerIndex_ += len;
        }
        else
        {
            retrieveAll();
        }
    }
    void retrieveAll() { readerIndex_ = writerIndex_ = CHEAP_PREPEND; }

    // 将onMessage函数上报的Buffer数据转化成string
    std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); }
    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }

    // 如果写空间不足，则进行扩容
    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len);
        }
    }

    // 把指定地址的长度为len的数据添加到可写缓冲区中
    void append(const char* data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }
    const char* beginWrite() const { return begin() + writerIndex_; }
    char* beginWrite() { return begin() + writerIndex_; }

    // 从fd上读取数据
    ssize_t readFd(int fd, int *saveErrno);
    // 将数据写入fd
    ssize_t writeFd(int fd, int *saveErrno);

private:
    char *begin() {return &buffer_[0];} // 返回Buffer的首地址
    const char* begin() const { return &buffer_[0];}
    // 扩容函数
    void makeSpace(size_t len)
    {
        // 可覆盖空间 + 剩余可写空间 < 所需空间
        if (writableBytes() + prependableBytes() < len + CHEAP_PREPEND)
        {
            buffer_.resize(writerIndex_ + len);
        }
        else // 可覆盖空间 + 剩余可写空间 >= 所需空间
        {
            // 把可读数据移动到起始位置，空出来的空间即可写入数据
            // 这么做的原因是，如果重新分配内存，反正也是要把数据拷到新分配的内存区域，代价只会更大
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + CHEAP_PREPEND);
            readerIndex_ = CHEAP_PREPEND;
            writerIndex_ = readerIndex_ + readable;
        }
    }
private:
    std::vector<char> buffer_;
    size_t readerIndex_;    // 指向可读数据的第一个下标索引号
    size_t writerIndex_;    // 指向可写空间的第一个下标索引号
};