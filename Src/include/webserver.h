#ifndef _WEBSERVER_H
#define _WEBSERVER_H

#include "./define.h"
#include <unordered_map>
#include <memory>

#include "../include/httpconn.h"
#include "../include/sqlconnRAII.h"
#include "../include/epoller.h"
#include "../include/heaptimer.h"
#include "../include/threadpool.h"

class WebServer {
public:
    WebServer(int port, int trigMode, int timeout, int OptLinger,int threadNum, int connPoolNum,
              int sqlPort, const char* sqlUser, const  char* sqlPwd, const char* dbName);

    ~WebServer();
    void Run();

private:
    int m_port;
    int m_openLinger;
    int m_timeout;
    bool m_isClose;
    int m_listenFd;
    char* m_srcDir;
    
    uint32_t m_listenEvent;
    uint32_t m_connEvent;

  
    std::unique_ptr<HeapTimer> m_timer;
    std::unique_ptr<ThreadPool> m_threadpool;
    std::unique_ptr<Epoller> m_epoller;
    std::unordered_map<int, HttpConn> m_users;


    static const int MAX_FD = 65536;
    static int SetFdNonblock(int fd);

    bool _Init_Socket(); 
    void _Init_EventMode(int trigMode);
    void _Add_Client(int fd, sockaddr_in addr);
  
    void _Deal_Listen();
    void _Deal_Write(HttpConn* client);
    void _Deal_Read(HttpConn* client);

    void _Send_Error(int fd, const char*info);
    void _Extent_Time(HttpConn* client);
    void _Close_Conn(HttpConn* client);

    void _Thread_Read(HttpConn* client);
    void _Thread_Write(HttpConn* client);

    void _On_Process(HttpConn* client);

};


#endif /* _WEBSERVER_H */

