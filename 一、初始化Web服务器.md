[TOC]

### 1、 WebServer 构造函数和析构函数

~~~c++
// 参数 port: 服务器的端口号
// 参数 OptLinger： 是否优雅的关闭客户端
// m_isClose: 标记服务器的状态，是否开启或关闭
WebServer::WebServer(int port, bool OptLinger)
    : m_port(port), m_openLinger(OptLinger),  m_isClose(false) {    
    // 如果服务器socket初始化失败，那么服务器是关闭状态
	if (!_Init_Socket()) {
        m_isClose = true; 
    }
}

// 关闭监听套接字，并把服务器状态标记为关闭
WebServer::~WebServer() {
    close(m_listenFd);
    m_isClose = true;
}
~~~

### 2、 _Init_Socket 初始化网络监听套接字

~~~c++
// 初始化socket 4步骤
// 1. socket函数：创建套接字文件描述符(可选TCP或UDP连接)
// 2. setsockopt函数: 设置套接字的选项（是否开启重用本地地址或延迟关闭等功能）
// 3. bind函数： 绑定服务器套接字地址（IP地址和port端口）
// 4. listen函数：监听套接字描述符（创建一个监听队列以存放待处理的客户连接）
bool WebServer::_Init_Socket() {
    int ret;
    struct sockaddr_in addr;
    // 服务器端口号要在1024 ~ 65535（因为0~1024是周知端口不可用）
    if (m_port > 65535 || m_port < 1024) {
        printf("Port:%d error!\n", m_port);
        return false;
    } 
    addr.sin_family = AF_INET;
    // 小端机器采用的是主机字节序，不管是啥机器，转成网络字节序比较保险
    // htos : host to network short
    // htonl : host to network long
    addr.sin_port = htons(m_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
	// 根据m_openLinger参数，是否启用优雅关闭功能：直到所剩数据发送完毕或超时
    struct linger optLinger = {0};
    if (m_openLinger) { 
        // l_onoff 等于0： SO_LINGER选项不起作用
        // l_linger 大于0：直到所剩数据发送完毕或超时
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }
    
    m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenFd < 0) {
        printf("socket error!\n");
        return false;
    }
	// 设置SO_LINGER选项
    ret = setsockopt(m_listenFd, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if (ret < 0) {
        close(m_listenFd);
        printf("setsockopt1 error!\n");
        return false;
    }
    // 设置复用本地地址：即使sock处于TIME_WAIT状态，与之绑定的socket地址也可以立即被重用
    int val = 1;
    ret = setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, (const void*)&val, sizeof(int));
    if (ret < 0) {
        close(m_listenFd);
        printf("setsockopt2 error!\n");
        return false;
    }
    // 绑定服务器socket地址
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
    return true;
}
~~~

### 3、 Web服务器主进程运行函数Run

~~~c++
// accept函数：从listen监听队列中接受一个客户连接
void WebServer::Run() { 
    if (!m_isClose) {
        printf("========== Server Run ==========\n");
    }
    while (!m_isClose) {
        struct sockaddr_in cli_addr;
        socklen_t len = sizeof(cli_addr);
        int fd = accept(m_listenFd, (struct sockaddr *)&cli_addr, &len);
        if (fd < 0) { break; }
        // 验证客户连接
        const char* buf = "hello client!\n";
        int n = write(fd, buf, strlen(buf));
    }
} 
~~~

### 4、 webserver 完整代码

#### 4.1 define.h 头文件

~~~c++
#ifndef _DEFINE_H
#define _DEFINE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>   
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#endif /*  _DEFINE_H */

~~~

#### 4.2 webserver.h 头文件

~~~c++
#ifndef _WEBSERVER_H
#define _WEBSERVER_H

#include "./define.h"

class WebServer {
public:
    WebServer(int port, bool OptLinger);
    ~WebServer();
    void Run();

private:
    int m_port;
    int m_listenFd;
    bool m_openLinger;
    bool m_isClose;
    
    bool _Init_Socket();
};

#endif /* _WEBSERVER_H */

~~~

#### 4.3 webserver.cpp 源文件

~~~c++
#include "./webserver.h"

WebServer::WebServer(int port, bool OptLinger)
    : m_port(port), m_openLinger(OptLinger),  m_isClose(false)
{    
    if (!_Init_Socket()) {
        m_isClose = true;
    }
}

WebServer::~WebServer() {
    close(m_listenFd);
    m_isClose = true;
}

void WebServer::Run() {
    if (!m_isClose) {
        printf("========== Server Run ==========\n");
    }
    while (!m_isClose) {
        struct sockaddr_in cli_addr;
        socklen_t len = sizeof(cli_addr);
        int fd = accept(m_listenFd, (struct sockaddr *)&cli_addr, &len);
        if (fd < 0) { break; }
        const char* buf = "hello client!\n";
        int n = write(fd, buf, strlen(buf));
    }
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
    return true;
}

~~~

#### 4.4 main.cpp 主函数

~~~c++
#include "./webserver.h"

int main() {
    WebServer server(8092, false);
    server.Run();
	return 0;   
} 

~~~

#### 4.5 Makefile 编译工具文件

~~~makefile
CXX := g++
CFLAGS := -std=c++14 -O1 -g 

OBJ_DIR := ./obj
BIN_DIR := ./bin

OBJS = ${OBJ_DIR}/main.o ${OBJ_DIR}/webserver.o 

.PHONY: mk_dir bin clean

all: mk_dir bin

mk_dir:
	if [ ! -d ${OBJ_DIR}  ]; then mkdir ${OBJ_DIR};fi
	if [ ! -d ${BIN_DIR}  ]; then mkdir ${BIN_DIR};fi

bin: $(OBJS)
	${CXX} ${CFLAGS} ${OBJS} -o ./bin/server 
	
${OBJ_DIR}/main.o: ./main.cpp
	${CXX} ${CFLAGS}  -o $@ -c $<

${OBJ_DIR}/webserver.o: ./webserver.cpp
	${CXX} ${CFLAGS}  -o $@ -c $<

clean:
	rm -rf ./bin ./obj
~~~

### 5、 测试结果

![img](https://s2.loli.net/2022/05/11/qsNl6dUOR1BGzmZ.png)