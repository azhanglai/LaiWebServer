[TOC]

### 1、Epoll的引出

- 内核接受网络数据过程：进程调用read阻塞，进程PCB从工作队列放到等待队列，期间计算机收到了对端传送的数据，数据经由网卡传送到内存，然后网卡通过中断信号通知CPU处理器有数据到达，CPU处理器执行中断程序。此中断程序主要有两项功能：①先将网络数据写入到对应socket的接收缓冲区里面。②再唤醒被阻塞的进程，重新将进程放入工作队列中，可以被CPU处理器调度。

- 一个socket对应着一个Port，网络数据包中包含IP和Port的信息，内核可以通过Port找到对应的socket。为了提高处理速度，操作系统会维护Port到socket的索引结构，以快速读取。网卡中断CPU后，CPU的中断函数从网卡存数据的内存拷贝数据到对应socket的接收缓冲区，具体是哪一个socket，CPU会检查Port，放到对应的socket。

- 服务端需要管理多个客户端连接，而read只能监视单个socket。这时，人们开始寻找监视多个socket的方法。epoll的要义是高效的监视多个socket。

- 假如能够预先传入一个socket列表，如果列表中的socket都没有数据，挂起进程，直到有一个socket收到数据，唤醒进程。这种方法很直接，也是select的设计思想。

- select的缺点：①每次调用select都需要将进程加入到所有监视socket的等待队列，每次唤醒都需要从每个队列中移除。这里涉及了两次遍历。②而且每次都要将整个fds列表拷贝给内核，有一定的开销。正是因为遍历操作开销大，出于效率的考量，才会规定select的最大监视数量，默认只能监视1024个socket。③进程被唤醒后，程序并不知道哪些socket收到数据，还需要遍历一次（这一次遍历是在应用层）。

- 当程序调用select时，内核会先遍历一遍socket，如果有一个以上的socket接收缓冲区有数据，那么select直接返回，不会阻塞。这也是为什么select的返回值有可能大于1的原因之一。如果没有socket有数据，进程才会阻塞。

- epoll是select和poll的增强版本。epoll通过一些措施来改进效率：措施①：功能分离。select低效的原因之一是将“维护等待队列”和“阻塞进程”两个步骤合二为一，然而大多数应用场景中，需要监视的socket相对固定，并不需要每次都修改。epoll将这两个操作分开，先用epoll_ctl维护等待队列，再调用epoll_wait阻塞进程（解耦）。措施②：就绪列表。select低效的另一个原因在于程序不知道哪些socket收到数据，只能一个个遍历，如果内核维护一个“就绪列表”，引用收到数据的socket，就能避免遍历。epoll使用双向链表来实现就绪队列，双向链表是能够快速插入和删除的数据结构，收到数据的socket被就绪列表所引用，只要获取就绪列表的内容，就能够知道哪些socket收到数据。

- epoll 索引结构。既然epoll将“维护监视队列”和“进程阻塞”分离，也意味着需要有个数据结构来保存监视的socket。至少要方便的添加和移除，还要便于搜索，以避免重复添加。红黑树是一种自平衡二叉查找树，搜索、插入和删除时间复杂度都是O(log(N))，效率较好。epoll使用了红黑树作为索引结构。

  ps：因为操作系统要兼顾多种功能，以及由更多需要保存的数据，就绪列表并非直接引用socket，而是通过epitem间接引用，红黑树的节点也是epitem对象。同样，文件系统也并非直接引用着socket。

- 什么时候select优于epoll？

  如果在并发量低，socket都比较活跃的情况下，select效率更高，也就是说活跃socket数目与监控的总的socket数目之比越大，select效率越高，因为select反正都会遍历所有的socket，如果比例大，就没有白白遍历。加之于select本身实现比较简单，导致总体现象比epoll好

### 2、Epoll的原理和流程

#### 2.1 epoll_create 创建epoll对象

- 某个进程调用epoll_create方法时，内核会创建一个eventpoll对象。
- eventpoll对象也是文件系统中的一员，和socket一样，它也会有等待队列（有线程会等待其事件触发，比如调用epoll_wait的线程就会阻塞在其上）。

#### 2.2 epoll_ctl 维护监视列表

- 创建epoll对象后，可以用epoll_ctl添加或删除所要监听的socket。
- epoll_ctl添加操作，内核会将eventpoll添加到socket的等待队列中。
- 当socket收到数据后，中断程序会操作eventpoll对象，而不是直接操作进程（也就是调用epoll的进程）。

#### 2.3 接受数据

- 当socket收到数据后，中断程序会给eventpoll的“就绪列表”添加socket引用。
- eventpoll对象相当于是socket和进程之间的中介，socket的数据接收并不直接影响进程，而是通过改变eventpoll的就绪列表来改变进程状态。
- 当程序执行到epoll_wait时，如果就绪列表已经引用了socket，那么epoll_wait直接返回，如果就绪列表为空，阻塞进程。

#### 2.4 epoll_wait 阻塞进程

- 假设计算机中正在运行进程A和进程B，在某时刻进程A运行到了epoll_wait语句。内核会将进程A放入eventpoll的等待队列中，阻塞进程。
- 当socket接收到数据，中断程序一方面修改就绪队列，另一方面唤醒eventpoll等待队列中的进程，进程A再次进入运行状态。也因为就绪列表的存在，进程A可以知道哪些socket发生了变化。

### 3、 编写 Epoller 类

#### 3.1 Epoller.h 头文件

~~~c++
#ifndef _EPOLLER_H
#define _EPOLLER_H

#include "./define.h"
#include <vector>
#include <errno.h>

class Epoller {
public:
    explicit Epoller(int maxEvent = 1024);
    ~Epoller();

    bool AddFd(int fd, uint32_t events);
    bool ModFd(int fd, uint32_t events);
    bool DelFd(int fd);
    int Wait(int timeout = -1);
    int GetEventFd(size_t i) const;
    uint32_t GetEvents(size_t i) const;

private:
    int m_epollFd;
    std::vector<struct epoll_event> m_events;    
};

#endif /* _EPOLLER_H */

~~~

#### 3.2  Epoller.cpp 源文件

~~~c++
#include "./epoller.h"

// epoll_create: 创建epoll红黑树索引结构
// 并且初始化epoll_event 数组
Epoller::Epoller(int maxEvent):m_epollFd(epoll_create(1)), m_events(maxEvent){
    assert(m_epollFd >= 0 && m_events.size() > 0);
}

Epoller::~Epoller() {
    close(m_epollFd);
}

// 将fd的events事件 注册到 epoll
bool Epoller::AddFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(m_epollFd, EPOLL_CTL_ADD, fd, &ev);
}

// 更改epoll中 fd的events事件
bool Epoller::ModFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(m_epollFd, EPOLL_CTL_MOD, fd, &ev);
}

// 将注册到epoll中的fd删除
bool Epoller::DelFd(int fd) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    return 0 == epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, &ev);
}

// epoll_wait: 等待timeout时间，注册到epoll里面的事件如果有就绪的会添加到m_events中 
int Epoller::Wait(int timeout) {
    return epoll_wait(m_epollFd, &m_events[0], static_cast<int>(m_events.size()), timeout);
}

// 得到m_events[i]中的fd 
int Epoller::GetEventFd(size_t i) const {
    assert(i < m_events.size() && i >= 0);
    return m_events[i].data.fd;
}

// 得到m_events[i]中的事件
uint32_t Epoller::GetEvents(size_t i) const {
    assert(i < m_events.size() && i >= 0);
    return m_events[i].events;
}

~~~

### 4、完善 webserver

#### 4.1 webserver.h

~~~c++
#ifndef _WEBSERVER_H
#define _WEBSERVER_H

#include "./define.h"
#include "./epoller.h" 		// 添加epoller.h 头文件
#include <memory>

class WebServer {
public:
    WebServer(int port, bool OptLinger, int trigMode); 	// 参数添加epoll触发模式
    ~WebServer();
    void Run();

private:
    int m_port;
    bool m_openLinger;
    bool m_isClose;
    int m_listenFd;
    
    uint32_t m_listenEvent; 				// 监听套接字epoll监听事件 
    uint32_t m_connEvent;					// 客户连接套接字epoll监听事件
    
    char m_buff[1024];						// 临时收发缓冲区
    
    std::unique_ptr<Epoller> m_epoller;		// epoll指针对象

    bool _Init_Socket();
    void _Init_EventMode(int trigMode);		// 初始化epoll触发模式
    static int Set_FdNonblock(int fd);		// 设置非阻塞模式
    
    void _Deal_Listen();
    void _Deal_Read(int fd);				// 处理客户端读事件
    void _Deal_Write(int fd);				// 处理客户端写事件
};

#endif /* _WEBSERVER_H */

~~~

#### 4.2 webserver.cpp

~~~c++
#include "./webserver.h"

WebServer::WebServer(int port, bool OptLinger, int trigMode)
    : m_port(port), m_openLinger(OptLinger),  m_isClose(false),
    m_epoller(new Epoller())
{    
    _Init_EventMode(trigMode);
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
        // m_epoller->Wait: 阻塞等待就绪事件，返回就绪事件的个数
        int nfd = m_epoller->Wait();
        for (int i = 0; i < nfd; ++i) {
            int fd = m_epoller->GetEventFd(i);			// 取得就绪事件的fd
            uint32_t events = m_epoller->GetEvents(i); 	// 取得就绪事件
            // 如果是监听套接字的事件就绪，说明有客户要连接
            if (fd == m_listenFd) {
                _Deal_Listen();
            }
            // 事件挂起或出错
            else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(fd > 0);
                close(fd);
            }
            // 客户读事件就绪
            else if (events & EPOLLIN) {
                assert(fd > 0);
                _Deal_Read(fd);
            }
            // 客户写事件就绪
            else if (events & EPOLLOUT) {
                assert(fd > 0);
                _Deal_Write(fd);
            }
            else {
                printf("Unexpected event\n");
            }
        }
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
    // 把监听fd注册到epoll
    ret = m_epoller->AddFd(m_listenFd, m_listenEvent | EPOLLIN);
    if (ret == 0) {
        close(m_listenFd);
        return false;
    }
    // 把监听fd 设置成非阻塞 
    Set_FdNonblock(m_listenFd);
    return true;
}

void WebServer::_Init_EventMode(int trigMode) {
    // EPOLLONESHOT: 只监听一次事件，如还需要监听的话，需要再次把fd注册到epoll
    m_listenEvent = EPOLLRDHUP;
    m_connEvent = EPOLLRDHUP | EPOLLONESHOT;
    // 0：默认水平触发LT
    // 1：listen 为 LT， conn 为ET(边沿触发)
    // 2：listen 为 ET， conn 为LT
    // 3：listen 为 ET， conn 为ET
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
        defalut: {
            m_listenEvent |= EPOLLET; 
            m_connEvent |= EPOLLET;
            break;
        }
    }
}

int WebServer::Set_FdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

void WebServer::_Deal_Listen() {
    struct sockaddr_in cli_addr;
    socklen_t len = sizeof(cli_addr);
    do {
        int fd = accept(m_listenFd, (struct sockaddr*)&cli_addr,&len);
        if (fd <= 0) { return ; }
        m_epoller->AddFd(fd, m_connEvent | EPOLLIN);
        Set_FdNonblock(fd);
    } while (m_listenEvent & EPOLLET);
}

void WebServer::_Deal_Read(int fd) {
    int len = -1;
	bzero(m_buff, sizeof(m_buff));
    do {
        len = read(fd, m_buff, 1024);
        if (len <= 0) { break; }
    } while (m_connEvent & EPOLLET);
	printf("Client: %s\n", m_buff);
    m_epoller->ModFd(fd, m_connEvent | EPOLLOUT);
}
// 将客户端发过来数据，把小写字母转成大写字母 回显回去
void WebServer::_Deal_Write(int fd) {
    int len = -1;
    for (int i = 0; m_buff[i]; ++i) {
        if (m_buff[i] >= 'a' && m_buff[i] <= 'z') {
            m_buff[i] -= 32;
        }
    }
	printf("Server: %s\n", m_buff);
    len = write(fd, m_buff, sizeof(m_buff));
    bzero(m_buff, sizeof(m_buff));
    m_epoller->ModFd(fd, m_connEvent | EPOLLIN);
}

~~~

### 5、 测试结果

#### 5.1 完善主函数 main.cpp

~~~c++
#include "./webserver.h"

int main() {
    // 服务器端口号， 是否优雅关闭， epoll触发模式
    WebServer server(8092, false, 3);
    server.Run();
	return 0;   
}
~~~

#### 5.2 完善Makefile

~~~makefile
CXX := g++
CFLAGS := -std=c++14 -O1 -g
OBJ_DIR := ./obj
BIN_DIR := ./bin
#添加epoller文件的编译
OBJS = ${OBJ_DIR}/main.o ${OBJ_DIR}/webserver.o ${OBJ_DIR}/epoller.o

.PHONY: mk_dir bin clean

all: mk_dir bin

mk_dir:
	if [ ! -d ${OBJ_DIR}  ]; then mkdir ${OBJ_DIR};fi
	if [ ! -d ${BIN_DIR}  ]; then mkdir ${BIN_DIR};fi

bin: $(OBJS)
	${CXX} ${CFLAGS} ${OBJS} -o ./bin/server  
	
${OBJ_DIR}/main.o: ./main.cpp
	${CXX} ${CFLAGS} -o $@ -c $<

${OBJ_DIR}/webserver.o: ./webserver.cpp
	${CXX} ${CFLAGS} -o $@ -c $<

${OBJ_DIR}/epoller.o: ./epoller.cpp
	${CXX} ${CFLAGS} -o $@ -c $<

clean:
	rm -rf ./bin ./obj 
~~~

#### 5.3 结果图片

![img](https://s2.loli.net/2022/05/13/zgAnMe1vBOh8No2.png)