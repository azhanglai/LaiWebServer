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

