[TOC]

### 1、使用线程池的原因

- 目前的大多数网络服务器，包括Web服务器、Email服务器以及数据库服务器等都具有一个共同点，就是单位时间内必须处理数目巨大的连接请求，但处理时间却相对较短。
- 多线程方案中我们采用的服务器模型则是一旦接受到请求之后，即创建一个新的线程，由该线程执行任务。任务执行完毕后，线程退出，这就是是“即时创建，即时销毁”的策略。尽管与创建进程相比，创建线程的时间已经大大的缩短，但是如果提交给线程的任务是执行时间较短，而且执行次数极其频繁，那么服务器将处于不停的创建线程，销毁线程的状态。
- 线程本身的开销所占的比例为(线程创建时间+线程销毁时间) / (程创建时间+线程执行时间+线程销毁时间)。如果线程执行的时间很短的话，这比开销可能占到20%-50%左右。如果任务执行时间很长的话，这笔开销将是忽略的。
- 线程池能够减少创建的线程个数。通常线程池所允许的并发线程是有上界的，如果同时需要并发的线程数超过上界，那么一部分线程将会等待。而传统方案中，如果同时请求数目为2000，那么最坏情况下，系统可能需要产生2000个线程。尽管这不是一个很大的数目，但是也有部分机器可能达不到这种要求。
- 线程池的出现是为了减少线程本身带来的开销。线程池采用预创建的技术，在应用程序启动之后，将立即创建一定数量的线程，放入空闲队列中。这些线程都是处于阻塞状态，不消耗CPU，但占用较小的内存空间。当任务到来后，缓冲池选择一个空闲线程，把任务传入此线程中运行。在任务执行完毕后线程也不退出，而是继续保持在池中等待下一次的任务。

### 2、线程池适合场景

-  单位时间内处理任务频繁而且任务处理时间短；
-  对实时性要求较高。如果接受到任务后在创建线程，可能满足不了实时要求，因此必须采用线程池进行预创建。

### 3、 编写 ThreadPool 类

~~~c++
#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>

class ThreadPool {
public:
	explicit ThreadPool(size_t threadCount = 8): m_pool(std::make_shared<Pool>()) {				
        assert(threadCount > 0);
        // 1.创建threadCount条线程，线程传入匿名函数
        // 2.detach： 从thread对象分离执行线程，允许执行独立地持续。一旦该线程退出，
        // 则释放任何分配的资源。调用 detach 后 *this 不再占有任何线程。
        for(size_t i = 0; i < threadCount; i++) {
            std::thread([pool = m_pool] {
                std::unique_lock<std::mutex> locker(pool->mtx); 	// 给线程池上锁
                while(true) {
                    if(!pool->tasks.empty()) {						// 任务队列有任务
                        auto task = std::move(pool->tasks.front());	// 从任务队列头部取任务
                        pool->tasks.pop();
                        locker.unlock();							// 任务取出来了，给池子解锁
                        task();										// 运行任务
                        locker.lock();								// 给池子再次上锁，因为循环取任务的
                    } 
                    else if(pool->isClosed) break; 					// 线程池没有开启
                    else pool->cond.wait(locker);					// 没有任务会阻塞，等待有任务通知了，才会唤醒
                }
            }).detach();
        }
    }

    ThreadPool() = default;
    ThreadPool(ThreadPool&&) = default;
    
    ~ThreadPool() {
        if(static_cast<bool>(m_pool)) {
            {
                // lock_guard: 作用域锁， 析构池子，把池子的状态设置为关闭
                std::lock_guard<std::mutex> locker(m_pool->mtx);
                m_pool->isClosed = true;
            }
            m_pool->cond.notify_all();	// 池子关闭了，通知所有线程关闭
        }
    }
	// 给池子的任务队列添加任务
    template<class F>
    void AddTask(F&& task) {
        {
            std::lock_guard<std::mutex> locker(m_pool->mtx);
            m_pool->tasks.emplace(std::forward<F>(task));	// 添加任务函数
        }
        m_pool->cond.notify_one();							// 有任务了，通知一个线程去抢
    }

private:
    // Pool: 存放线程的池子
    struct Pool {
        std::mutex mtx;							 // 用于保护共享数据免受从多个线程同时访问的同步原语
        std::condition_variable cond;			 // 用于阻塞一个线程，或同时阻塞多个线程，直至另一线程修改共享变量（条件）并通知cond
        bool isClosed;							 // 标记线程池是否开启状态
        std::queue<std::function<void()>> tasks; // 任务队列
    };
    std::shared_ptr<Pool> m_pool;
};

#endif /* _THREADPOOL_H */

~~~

### 4、 测试结果

#### 4.1  完善 webserver

~~~c++
#ifndef _WEBSERVER_H
#define _WEBSERVER_H

#include "./define.h"
#include "./epoller.h"
#include "./threadpool.h" 		// 添加threadpool.h头文件
#include <memory>

class WebServer {
public:
    WebServer(int port, bool OptLinger, int trigMode, int threadNum);	// 添加参数：线程数量
    ~WebServer();
    void Run();

private:
    int m_port;
    bool m_openLinger;
    bool m_isClose;
    int m_listenFd;
    
    uint32_t m_listenEvent;
    uint32_t m_connEvent;
    
    char m_buff[1024];
    
    std::unique_ptr<Epoller> m_epoller;
    std::unique_ptr<ThreadPool> m_threadpool;

    bool _Init_Socket();
    void _Init_EventMode(int trigMode);
    static int Set_FdNonblock(int fd);
    
    void _Deal_Listen();
    void _Deal_Read(int fd);
    void _Deal_Write(int fd);
    
    void _Thread_Read(int fd); 				// 添加线程任务函数
    void _Thread_Write(int fd); 			// 添加线程任务函数

};

#endif /* _WEBSERVER_H */

~~~

源文件只保留修改部分

~~~c++
#include "./webserver.h"

WebServer::WebServer(int port, bool OptLinger, int trigMode, int threadNum)
    : m_port(port), m_openLinger(OptLinger),  m_isClose(false),
    m_epoller(new Epoller()), m_threadpool(new ThreadPool(threadNum))
{    
    _Init_EventMode(trigMode);
    if (!_Init_Socket()) {
        m_isClose = true;
    }
}

void WebServer::_Deal_Read(int fd) {
    m_threadpool->AddTask(std::bind(&WebServer::_Thread_Read, this, fd));
}

void WebServer::_Deal_Write(int fd) {
    m_threadpool->AddTask(std::bind(&WebServer::_Thread_Write, this, fd));
}

void WebServer::_Thread_Read(int fd) {
    int len = -1;
    bzero(m_buff, sizeof(m_buff));
    do {
        len = read(fd, m_buff, 1024);
        if (len <= 0) { break; }
    } while (m_connEvent & EPOLLET);
    printf("Client: %s\n", m_buff);
    m_epoller->ModFd(fd, m_connEvent | EPOLLOUT);
}

void WebServer::_Thread_Write(int fd) { 
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

#### 4.2  完善主函数 main.cpp

~~~c++
#include "./webserver.h"

int main() {
    // 服务器端口号， 是否优雅关闭， epoll触发模式, 线程数量
    WebServer server(8092, false, 3, 8);
    server.Run();
	return 0;   
}
~~~

#### 4.3 完善Makefile

~~~makefile
CXX := g++
CFLAGS := -std=c++14 -O1 -g
OBJ_DIR := ./obj
BIN_DIR := ./bin

OBJS = ${OBJ_DIR}/main.o ${OBJ_DIR}/webserver.o ${OBJ_DIR}/epoller.o

.PHONY: mk_dir bin clean

all: mk_dir bin

mk_dir:
	if [ ! -d ${OBJ_DIR}  ]; then mkdir ${OBJ_DIR};fi
	if [ ! -d ${BIN_DIR}  ]; then mkdir ${BIN_DIR};fi
#添加线程链接库
bin: $(OBJS)
	${CXX} ${CFLAGS} ${OBJS} -o ./bin/server -pthread 
	
${OBJ_DIR}/main.o: ./main.cpp
	${CXX} ${CFLAGS} -o $@ -c $<

${OBJ_DIR}/webserver.o: ./webserver.cpp
	${CXX} ${CFLAGS} -o $@ -c $<

${OBJ_DIR}/epoller.o: ./epoller.cpp
	${CXX} ${CFLAGS} -o $@ -c $<

clean:
	rm -rf ./bin ./obj 
~~~

#### 4.4 图片展示

![img](https://s2.loli.net/2022/05/13/uAlrx6qyVvPghD8.png)

