#include "../include/buffer.h"

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

