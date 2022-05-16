#include "../include/webserver.h"
#include <iostream>
using namespace std;

WebServer::WebServer(int port, int trigMode, int timeout, int OptLinger, int threadNum, int connPoolNum,
                    int sqlPort, const char* sqlUser, const  char* sqlPwd,
                    const char* dbName)
    : m_port(port), m_openLinger(OptLinger), m_timeout(timeout), m_isClose(false),
    m_epoller(new Epoller()), m_timer(new HeapTimer()), m_threadpool(new ThreadPool(threadNum))
{
    m_srcDir = getcwd(nullptr, 256);
    assert(m_srcDir);
    strncat(m_srcDir, "/resource/", 16);
    
    HttpConn::userCount = 0;
    HttpConn::srcDir = m_srcDir;

    SqlConnPool::Instance()->Init("127.0.0.1", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    _Init_EventMode(trigMode);
    if (!_Init_Socket()) {
        m_isClose = true;
    }
	// 打印webserver服务器初始化信息
    if (m_isClose) {
        printf("========== Server Init Error ==========\n");
    } else {
        printf("========== Server Init ==========\n");
        printf("Port: %d, OpenLinger: %s\n", 
               m_port, m_openLinger ? "true" : "false");
        printf("ListenEvent: %s, ConnEvent: %s\n",
               (m_listenEvent & EPOLLET ? "ET" : "LT"),
               (m_connEvent & EPOLLET ? "ET" : "LT"));
        printf("srcDir: %s\n", HttpConn::srcDir);
        printf("ThreadPool Num: %d, SqlConnPool Num: %d\n\n",
              threadNum, connPoolNum);
    }
}

WebServer::~WebServer() {
    close(m_listenFd);
    m_isClose = true;
    free(m_srcDir);
    SqlConnPool::Instance()->ClosePool();
}

void WebServer::Run() {
    int timeout = -1;
    if (!m_isClose) {
        printf("========== Server Run ==========\n");
    }
    while (!m_isClose) {
        if (m_timeout > 0) {
            timeout = m_timer->GetNextTick(); 
        }
        int nfd = m_epoller->Wait(timeout);
        for (int i = 0; i < nfd; ++i) {
            int fd = m_epoller->GetEventFd(i);
            uint32_t events = m_epoller->GetEvents(i);
            // 监听套接字事件， 有客户连接
            if (fd == m_listenFd) {
                _Deal_Listen();
            }
            // 监听事件挂起或者出错
            else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(m_users.count(fd) > 0);
                _Close_Conn(&m_users[fd]);
            }
            // 监听读事件
            else if (events & EPOLLIN) {
                assert(m_users.count(fd) > 0);
                _Deal_Read(&m_users[fd]);
            }
            // 监听写事件
            else if (events & EPOLLOUT) {
                assert(m_users.count(fd) > 0);
                _Deal_Write(&m_users[fd]);
            } else {
                printf("Unexpected event!\n");
            }
        }
    }
} 

void WebServer::_Init_EventMode(int trigMode) {
    m_listenEvent = EPOLLRDHUP;
    m_connEvent = EPOLLONESHOT | EPOLLRDHUP;

    switch (trigMode) {
        case 0: { break; }
        case 1: {
            m_connEvent |= EPOLLET;
            break;
        }
        case 2: {
            m_listenEvent |= EPOLLET;
            break;
        }
        case 3: {
            m_listenEvent |= EPOLLET;
            m_connEvent |= EPOLLET;
            break;
        }
        default: {
            m_listenEvent |= EPOLLET;
            m_connEvent |= EPOLLET;
            break; 
        }
    }
    HttpConn::isET = (m_connEvent & EPOLLET);
}

int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

bool WebServer::_Init_Socket() {
    int ret;
    struct sockaddr_in addr;
    if (m_port > 65535 || m_port < 1024) {
        printf("Port:%d error!\n", m_port);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    struct linger optLinger = {0};
    if (m_openLinger) {
        // 优雅关闭: 直到所剩数据发送完毕或超时
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenFd < 0) {
        printf("socket error!\n");
        return false;
    }

    ret = setsockopt(m_listenFd, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if (ret < 0) {
        close(m_listenFd);
        printf("setsockopt1 error!\n");
        return false;
    }
    // 设置端口复用
    int val = 1;
    ret = setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, (const void*)&val, sizeof(int));
    if (ret < 0) {
        close(m_listenFd);
        printf("setsockopt2 error!\n");
        return false;
    }
    // 绑定端口
    ret = bind(m_listenFd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0) {
        printf("bind error!\n");
        close(m_listenFd);
        return false;
    }
    // 监听套接字 
    ret = listen(m_listenFd, 10);
    if (ret < 0) {
        printf("listen error!\n");
        close(m_listenFd);
        return false; 
    }
    // 将监听套接字注册到epoll 
    ret = m_epoller->AddFd(m_listenFd, m_listenEvent | EPOLLIN);
    if (ret == 0) {
        printf("m_listenFd Add epoll error!\n");
        close(m_listenFd);
        return false;
    }
    // 将监听套接字设置成非阻塞
    SetFdNonblock(m_listenFd);
    return true;
}

void WebServer::_Send_Error(int fd, const char*info) {
	assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        printf("send error to client[%d] error!", fd);
    }
    close(fd); 
}

// 关闭客户连接 (定时器用回调函数关闭)
void WebServer::_Close_Conn(HttpConn* client) {
    assert(client);
    m_epoller->DelFd(client->GetFd());
    client->Close();
}

// 添加客户连接（初始化客户的fd和addr, 给客户加上定时器，把客户注册到epoll）
void WebServer::_Add_Client(int fd, sockaddr_in addr) {
    assert(fd > 0);
    m_users[fd].init(fd, addr);     // 初始化客户的fd 和 addr
    // 给客户加上定时器
    if (m_timeout > 0) {
        m_timer->add(fd, m_timeout, std::bind(&WebServer::_Close_Conn, this, &m_users[fd]));
    }
    m_epoller->AddFd(fd, EPOLLIN | m_connEvent);
    SetFdNonblock(fd);
}

// 处理客户连接事件
void WebServer::_Deal_Listen() {
	struct sockaddr_in cli_addr;
	socklen_t len = sizeof(cli_addr);
	do {
        // 接受一个客户连接
		int fd = accept(m_listenFd, (struct sockaddr *)&cli_addr, &len);
        if (fd < 0) { return ; }
        else if (HttpConn::userCount >= MAX_FD) {
            _Send_Error(fd, "Server busy!\n");
            printf("client is full!\n");
            return ;
        }
        _Add_Client(fd, cli_addr);
	} while (m_listenEvent & EPOLLET); // 边沿触发的话需要循环
}

// 处理客户读事件
void WebServer::_Deal_Read(HttpConn* client) {
    assert(client);
    _Extent_Time(client);   // 重新调整时间
    m_threadpool->AddTask(std::bind(&WebServer::_Thread_Read, this, client));
}


void WebServer::_Deal_Write(HttpConn* client) {
    assert(client);
    _Extent_Time(client);   // 重新调整时间
    m_threadpool->AddTask(std::bind(&WebServer::_Thread_Write, this, client));
}

// 调整定时器时间 
void WebServer::_Extent_Time(HttpConn* client) {
    assert(client);
    if (m_timeout > 0) {m_timer->adjust(client->GetFd(), m_timeout); }
}

// 线程的客户读任务
void WebServer::_Thread_Read(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readError = 0;
    ret = client->read(&readError);
    if (ret <= 0 && readError != EAGAIN) {
        _Close_Conn(client);
        return ;
    }
    _On_Process(client); // 处理请求
}


void WebServer::_Thread_Write(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if (client->ToWriteBytes() == 0) {
        // 传输完成
        if (client->IsKeepAlive()) {
            _On_Process(client); // 处理响应
            return ;
        }
    } else if (ret < 0) {
        if (writeErrno == EAGAIN) {
            // 继续传输 
            m_epoller->ModFd(client->GetFd(), m_connEvent | EPOLLOUT);
            return ;
        }
    }
    _Close_Conn(client);
}

void WebServer::_On_Process(HttpConn* client) {
    // 如果客户请求 处理成功，那么将该客户从监听读事件改成监听写事件
    if (client->process()) {
        m_epoller->ModFd(client->GetFd(), m_connEvent | EPOLLOUT);
    } else {
        m_epoller->ModFd(client->GetFd(), m_connEvent | EPOLLIN);
    }
}


