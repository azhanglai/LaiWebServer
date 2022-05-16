#ifndef _HTTPCONN_H
#define _HTTPCONN_H

#include "./define.h"
#include "./sqlconnRAII.h"
#include "./buffer.h"
#include "./httprequest.h"
#include "./httpresponse.h"

class HttpConn {
public:
    HttpConn();
    ~HttpConn();

    void init(int sockFd, const sockaddr_in& addr);
    void Close();

    ssize_t read(int* saveErrno);
    ssize_t write(int* saveErrno);
    bool process();

    int GetFd() const { return m_fd; }
    struct sockaddr_in GetAddr() const { return m_addr; }
    int GetPort() const { return m_addr.sin_port; }
    const char* GetIP() const { return inet_ntoa(m_addr.sin_addr); }
    
    int ToWriteBytes() { return m_iov[0].iov_len + m_iov[1].iov_len; }

    bool IsKeepAlive() const { return m_request.IsKeepAlive(); }

    static bool isET;
    static const char* srcDir;
    static std::atomic<int> userCount;
    
private:
   
    int m_fd;
    struct  sockaddr_in m_addr;
    bool m_isClose;
    
    int m_iovCnt;
    struct iovec m_iov[2];
    
    Buffer m_readBuff;  
    Buffer m_writeBuff; 

    HttpRequest m_request;
    HttpResponse m_response;
};


#endif /* _HTTPCONN_H */

