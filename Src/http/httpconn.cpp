#include "../include/httpconn.h"
using namespace std;

const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;
bool HttpConn::isET;

HttpConn::HttpConn() { 
    m_fd = -1;
    m_addr = {0};
    m_isClose = true;
};

HttpConn::~HttpConn() { 
    Close(); 
};

// 客户连接初始化
void HttpConn::init(int fd, const sockaddr_in& addr) {
    assert(fd > 0);
    userCount++;                // 客户连接数+1
    m_addr = addr;              // 客户socket地址
    m_fd = fd;                  // 客户TCP连接描述符
    m_writeBuff.RetrieveAll();  // 客户写缓冲区
    m_readBuff.RetrieveAll();   // 客户读缓冲区
    m_isClose = false;          // 客户是否关闭连接标记
}

// 关闭连接
void HttpConn::Close() {
    m_response.UnmapFile();     // 释放共享内存
    if(m_isClose == false){
        m_isClose = true;       // 标记关闭
        userCount--;            // 连接数-1
        close(m_fd);            
    }
}

// 把客户请求数据读进输入buffer中
ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = m_readBuff.ReadFd(m_fd, saveErrno);
        if (len <= 0) {
            break;
        }
    } while (isET);     // 边沿触发需要循环读
    return len;
}

// 把服务器响应数据集中写给客户 
ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = writev(m_fd, m_iov, m_iovCnt);
        if(len <= 0) {
            *saveErrno = errno;
            break;
        }
        // iovec 没有数据， 说明传输结束
        if (m_iov[0].iov_len + m_iov[1].iov_len == 0) { 
            break; 
        }
        // len > m_iov[0].iov_len, 说明m_iov[0]和m_iov[1]都有数据写到fd 
        else if(static_cast<size_t>(len) > m_iov[0].iov_len) {
            m_iov[1].iov_base = (uint8_t*) m_iov[1].iov_base + (len - m_iov[0].iov_len);
            m_iov[1].iov_len -= (len - m_iov[0].iov_len);
            // m_iov[0]需要初始化(因为被写空了)
            if(m_iov[0].iov_len) {
                m_writeBuff.RetrieveAll();
                m_iov[0].iov_len = 0;
            }
        }
        else {
            m_iov[0].iov_base = (uint8_t*)m_iov[0].iov_base + len; 
            m_iov[0].iov_len -= len; 
            m_writeBuff.Retrieve(len);
        }
    } while(isET || ToWriteBytes() > 10240); // 当m_iov的数据很大或者边沿触发时，需要循环写
    return len;
}

// process: 3件事
// 1. 解析客户的请求数据
// 2. 根据解析的请求数据作出响应
// 3. 将响应数据和响应文件放入m_iov，方便集中写
bool HttpConn::process() {
    m_request.Init();
    // 1.没有可读的客户请求数据
    if(m_readBuff.ReadableBytes() <= 0) {
        return false;
    }
    // 2.解析客户的请求数据
    else if(m_request.parse(m_readBuff)) {
        // 客户请求数据解析成功， 初始化正常网页响应
        m_response.Init(srcDir, m_request.path(), m_request.IsKeepAlive(), 200);
    } else {
        // 客户请求数据解析失败， 初始化错误网页响应
        m_response.Init(srcDir, m_request.path(), false, 400);
    }
    // 根据客户请求数据，作出响应, 并将响应数据写到输出buffer中
    m_response.Make_Response(m_writeBuff);
    // 将输出buffer中的响应数据 读到 m_iov[0]中
    m_iov[0].iov_base = const_cast<char*>(m_writeBuff.Peek());
    m_iov[0].iov_len = m_writeBuff.ReadableBytes();
    m_iovCnt = 1;
    
    // 将客户请求的文件(需要响应的文件)，读到m_iov[1]中
    if(m_response.FileLen() > 0  && m_response.File()) {
        m_iov[1].iov_base = m_response.File();
        m_iov[1].iov_len = m_response.FileLen();
        m_iovCnt = 2;
    }
    return true;
}

