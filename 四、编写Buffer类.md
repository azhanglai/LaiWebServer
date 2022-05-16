[TOC]

### 1、设计应用层buffer的原因

- 非阻塞IO 的核心思想是避免阻塞在 read() 或 write() 或其他 IO 系统调用上，这样可以最大限度地复用控制线程，让一个线程能服务于多个 socket 连接；
- IO 线程只能阻塞在 IO多路复用 函数上，如 select()/poll()/epoll_wait()。应用层的缓冲区是必须的，每个socket都要有带状态的输入和输出缓冲区
- 当通过TCP连接发送10K字节数据时，在write()调用中，只接受了8K字节，此时不应该在等待，因为不知道会等多久，进程应该尽快交出控制器。但是剩余的2K数据怎么办，些应该由网络库来操心，网络库应该接管这剩余的 2K 字节数据，把它保存在该 TCP连接 的 输出缓冲区里，然后注册 POLLOUT 事件，一旦 socket 变得可写就立刻发送数据。要让程序在 write 操作上非阻塞，网络库必须要给每个TCP连接配置输出缓冲区。
- 网络库在处理“socket 可读”事件的时候，必须一次性把 socket 里的数据读完，否则会反复触发 POLLIN 事件，造成 busy-loop。那么网络库必然要应对“数据不完整”的情况，收到的数据先放到输入缓冲区里，等构成一条完整的消息再通知程序的业务逻辑。在 tcp 网络编程中，网络库必须要给每个TCP连接配置输入缓冲区

### 2、 Buffer类的设计要点

- 对外表现为一块连续的内存, 以方便客户代码的编写;
- 内部以 vector of char 来保存数据，并提供相应的访问函数;
- 其 size() 可以自动增长，以适应不同大小的消息。
- Buffer 其实像是一个 queue，从末尾写入数据，从头部读出数据。

~~~c++
std::vector<char> m_buffer;
Buffer::Buffer(int Size) 
    : m_buffer(Size), m_readPos(0), m_writePos(0) 
{    
}
~~~

### 3、 Buffer类的结构

- Buffer 的内部是一个 vector of char，它是一块连续的内存。
- Buffer 有writePos 和 readPos 两个成员，指向该 buffer中的元素。它们 的类型是 int，不是 char*，目的是应对迭代器失效。
- 两个下标索引把buffer的内容分为三块 Prependable、Readable、Writable区域

~~~c++
// buffer可读区域（写 - 读）
size_t Buffer::ReadableBytes() const {
    return m_writePos - m_readPos;
}
// buffer可写区域（总空间 - 写）
size_t Buffer::WritableBytes() const {
    return m_buffer.size() - m_writePos;
}

// buffer已读区域（读）
size_t Buffer::PrependableBytes() const {
    return m_readPos;
}
~~~

### 4、 Buffer::ReadFd()

在栈上准备一个 65536 字节的buff，然后利用 readv() 来读取数据，iovec 有两块，第一块指向 Buffer 中的 writable 字节，另一块指向栈上的 buuf。这样如果读入的数据不多，那么全部都读到 Buffer 中去了；如果长度超过 Buffer 的 writable 字节数，就会读到栈上的 stackbuf 里，然后程序再把 stackbuf 里的数据 append 到 Buffer 中。利用了临时栈上空间，避免开巨大 Buffer 造成的内存浪费，也避免反复调用 read() 的系统开销

~~~c++
// 将fd的数据读到 buffer中(相当于把fd的数据写进bufffer中)
ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    char buff[65535];
    struct iovec iov[2];
    const size_t writable = WritableBytes();
    // 分散读， 保证数据全部读完 
    // 若len > writable, 则会把一部分数据写进栈中的buff
    iov[0].iov_base = _Begin_Ptr() + m_writePos;
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2);
    if(len < 0) {
        *saveErrno = errno;
    }
    else if(static_cast<size_t>(len) <= writable) {
        m_writePos += len;
    }
    // 将写进栈中buff的数据写回到buffer中，Append会扩容
    else {
        m_writePos = m_buffer.size();
        Append(buff, len - writable);
    }
    return len;
}
~~~

### 5、 buffer 完整代码

#### 5.1 buffer.h 头文件

~~~c++
#ifndef _BUFFER_H
#define _BUFFER_H

#include "./define.h"
#include <vector> 
#include <string>
#include <atomic>

class Buffer {
public:
    Buffer(int Size = 1024);
    ~Buffer() = default;

    size_t WritableBytes() const;       
    size_t ReadableBytes() const ;
    size_t PrependableBytes() const;

    const char* Peek() const;
    void EnsureWriteable(size_t len);
    void HasWritten(size_t len);

    void Retrieve(size_t len);
    void RetrieveUntil(const char* end);

    void RetrieveAll() ;
    std::string RetrieveAllToStr();

    const char* BeginWriteConst() const;
    char* BeginWrite();

    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const void* data, size_t len);
    void Append(const Buffer& buff);

    ssize_t ReadFd(int fd, int* Errno);
    ssize_t WriteFd(int fd, int* Errno);

private:
    char* _Begin_Ptr();
    const char* _Begin_Ptr() const;
    void _Make_Space(size_t len);

    std::vector<char> m_buffer;
    std::atomic<std::size_t> m_readPos;
    std::atomic<std::size_t> m_writePos;
};

#endif /* _BUFFER_H */

~~~

#### 5.2 buffer.cpp 源文件

~~~c++
#include "./buffer.h"

Buffer::Buffer(int Size) : m_buffer(Size), m_readPos(0), m_writePos(0) {}

// buffer可读区域（写 - 读）
size_t Buffer::ReadableBytes() const {
    return m_writePos - m_readPos;
}
// buffer可写区域（总空间 - 写）
size_t Buffer::WritableBytes() const {
    return m_buffer.size() - m_writePos;
}

// buffer已读区域（读）
size_t Buffer::PrependableBytes() const {
    return m_readPos;
}

// buffer起始点
char* Buffer::_Begin_Ptr() {
    return &*m_buffer.begin();
}

const char* Buffer::_Begin_Ptr() const {
    return &*m_buffer.begin();
}

// buffer已读起始点
const char* Buffer::Peek() const {
    return _Begin_Ptr() + m_readPos;
}

// 从buffer中读len长度数据
void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    m_readPos += len;
}

// 从buffer已读起始点读到buffer的结尾
void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end );
    Retrieve(end - Peek());
}

// （读完）清空buffer，读写头回到起点
void Buffer::RetrieveAll() {
    bzero(&m_buffer[0], m_buffer.size());
    m_readPos = 0;
    m_writePos = 0;
}

// 把可读区域移到str, 并清空
std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

// buffer已写起始点
char* Buffer::BeginWrite() {
    return _Begin_Ptr() + m_writePos;
}

const char* Buffer::BeginWriteConst() const {
    return _Begin_Ptr() + m_writePos;
}

// 从buffer中和写len长度数据
void Buffer::HasWritten(size_t len) {
    m_writePos += len;
} 

// buffer空间扩容
void Buffer::_Make_Space(size_t len) {
    // 可写区域+已读区域 不满足len长度，那么需要buffer直接扩容
    if(WritableBytes() + PrependableBytes() < len) {
        m_buffer.resize(m_writePos + len + 1);
    } 
    // 可写区域+已读区域 满足 len长度，将已写未读数据，拷贝到起始点
    // 读头回到起始点，写头去到已写终点
    else {
        size_t readable = ReadableBytes();
        std::copy(_Begin_Ptr() + m_readPos, _Begin_Ptr() + m_writePos, _Begin_Ptr());
        m_readPos = 0;
        m_writePos = m_readPos + readable;
        assert(readable == ReadableBytes());
    }
}

// 判断bubffer是否能写下len长度数据，不能则需要扩容
void Buffer::EnsureWriteable(size_t len) {
    if(WritableBytes() < len) {
        _Make_Space(len);
    }
    assert(WritableBytes() >= len);
}
// 将str中len长度数据 写入 buffer中
void Buffer::Append(const char* str, size_t len) {
    assert(str);
    EnsureWriteable(len);
    std::copy(str, str + len, BeginWrite());
    HasWritten(len);
}

void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());
}

void Buffer::Append(const void* data, size_t len) {
    assert(data);
    Append(static_cast<const char*>(data), len);
}

void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}

// 将fd的数据读到 buffer中(相当于把fd的数据写进bufffer中)
ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    char buff[65535];
    struct iovec iov[2];
    const size_t writable = WritableBytes();
    // 分散读， 保证数据全部读完 
    // 若len > writable, 则会把一部分数据写进栈中的buff
    iov[0].iov_base = _Begin_Ptr() + m_writePos;
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2);
    if(len < 0) {
        *saveErrno = errno;
    }
    else if(static_cast<size_t>(len) <= writable) {
        m_writePos += len;
    }
    // 将写进栈中buff的数据写回到buffer中，Append会扩容
    else {
        m_writePos = m_buffer.size();
        Append(buff, len - writable);
    }
    return len;
}

// 将buffer的数据读到 fd中(相当于把buffer的数据写进fd中)
ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);
    if(len < 0) {
        *saveErrno = errno;
        return len;
    } 
    m_readPos += len;
    return len;
}

~~~
