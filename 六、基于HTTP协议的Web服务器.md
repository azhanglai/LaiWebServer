[TOC]

### 1、结合HttpRequest 和 HttpResponse 编写HttpConn 类

#### 1.1 httpconn.h 头文件

~~~c++
#ifndef _HTTPCONN_H
#define _HTTPCONN_H

#include "./define.h"
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

~~~

#### 1.2 httpconn.cpp 源文件

~~~c++
#include "./httpconn.h"
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
~~~

### 2、完善基于HTTP协议的 WebServer

~~~c++
#ifndef _WEBSERVER_H
#define _WEBSERVER_H

#include "./define.h"
#include "./epoller.h"
#include "./threadpool.h"
#include "./httpconn.h" 		// 添加httpconn.h 头文件
#include <memory>
#include <unordered_map>

class WebServer {
public:
    WebServer(int port, bool OptLinger, int trigMode, int threadNum);
    ~WebServer();
    void Run();

private:
    int m_port;
    bool m_openLinger;
    bool m_isClose;
    int m_listenFd;
    
    uint32_t m_listenEvent;
    uint32_t m_connEvent;
    
    char* m_srcDir;									// http服务器资源文件目录
    static const int MAX_FD = 65536;				// 最多的客户连接数
    
    std::unique_ptr<Epoller> m_epoller;				
    std::unique_ptr<ThreadPool> m_threadpool;
    std::unordered_map<int, HttpConn> m_users;		// 客户连接fd与客户的键值对

    bool _Init_Socket();
    void _Init_EventMode(int trigMode);
    static int Set_FdNonblock(int fd);
    
    void _Deal_Listen();
    void _Deal_Read(HttpConn*);					 	// 处理客户读事件 
    void _Deal_Write(HttpConn*);					// 处理客户写事件
    
    void _Thread_Read(HttpConn*); 					// 真正执行线程，处理客户读函数
    void _Thread_Write(HttpConn*);   				// 真正执行线程，处理客户写函数

    void _On_Process(HttpConn*);					// 处理客户请求
    void _Close_Conn(HttpConn*);					// 关闭客户连接
};

#endif /* _WEBSERVER_H */

~~~

只保留修改后的代码

~~~c++
#include "./webserver.h"

WebServer::WebServer(int port, bool OptLinger, int trigMode, int threadNum)
    : m_port(port), m_openLinger(OptLinger),  m_isClose(false),
    m_epoller(new Epoller()), m_threadpool(new ThreadPool(threadNum))
{    

    m_srcDir = getcwd(nullptr, 256);		// 获取当前目录
    strncat(m_srcDir, "/resources/", 16); 	// 拼接目录，得到http资源文件的目录
    HttpConn::userCount = 0; 				// 客户连接数
    HttpConn::srcDir = m_srcDir;			

    _Init_EventMode(trigMode);
    if (!_Init_Socket()) {
        m_isClose = true;
    }
}

WebServer::~WebServer() {
    close(m_listenFd);
    m_isClose = true;
    free(m_srcDir);
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
    HttpConn::isET = (m_connEvent & EPOLLET); 	// 判断客户连接是否是边沿触发模式 
}


void WebServer::_Deal_Listen() {
    struct sockaddr_in cli_addr;
    socklen_t len = sizeof(cli_addr);
    do {
        int fd = accept(m_listenFd, (struct sockaddr*)&cli_addr,&len);
        if (fd <= 0) { return ; }
        // 当客户连接数大于MAX_FD，满了，已经不能再连接了
        else if (HttpConn::userCount >= MAX_FD) {
            close(fd);
            return ;
        }   
        m_users[fd].init(fd, cli_addr); 				// 客户连接初始化
        m_epoller->AddFd(fd, m_connEvent | EPOLLIN); 	// 将客户连接fd注册到epoll
        Set_FdNonblock(fd);
    } while (m_listenEvent & EPOLLET);
}

void WebServer::_Deal_Read(HttpConn* client) {
    assert(client);
    // 将任务函数放入任务队列，由线程池里面的线程去执行
    m_threadpool->AddTask(std::bind(&WebServer::_Thread_Read, this, client));
}

void WebServer::_Deal_Write(HttpConn* client) {
    assert(client);
    // 将任务函数放入任务队列，由线程池里面的线程去执行
    m_threadpool->AddTask(std::bind(&WebServer::_Thread_Write, this, client));
}

void WebServer::_Thread_Read(HttpConn* client) {
    assert(client);
    int readErrno = 0;
    int ret = client->read(&readErrno); 	// 把客户请求数据读进输入buffer中
    if (ret <= 0 && readErrno != EAGAIN) {
        _Close_Conn(client);
        return ;
    }
    _On_Process(client);  		// 处理客户请求
}

void WebServer::_Thread_Write(HttpConn* client) {
    assert(client);
    int writeErrno = 0; 
    int ret = client->write(&writeErrno);  // 把服务器响应数据集中写给客户 
    // m_iov写空，说明传输完成
    if (client->ToWriteBytes() == 0) {
        // 如果是长连接，继续处理客户请求
        if (client->IsKeepAlive()) {
            _On_Process(client);
            return ;
        }
    }
    else if (ret < 0) {
        // 说明这次响应数据写给客户，还需要监听写事件
        if (writeErrno == EAGAIN) {
            m_epoller->ModFd(client->GetFd(), m_connEvent | EPOLLOUT);
            return ;
        }
        
    }
    _Close_Conn(client);
}

void WebServer::_On_Process(HttpConn* client) {
    // client->process: 
    // 1. 解析客户的请求数据 
    // 2. 根据解析的请求数据作出响应
    // 3. 将响应数据和响应文件放入m_iov，方便集中写
    if (client->process()) {
        // 如果process成功，不应该在监听读事件了，需要改成监听写事件
        m_epoller->ModFd(client->GetFd(), m_connEvent | EPOLLOUT);
    } else {
        m_epoller->ModFd(client->GetFd(), m_connEvent | EPOLLIN);
    }
}

void WebServer::_Close_Conn(HttpConn* client) {
    assert(client);
    m_epoller->DelFd(client->GetFd());
    client->Close();
}
~~~

### 3、 测试结果

#### 3.1 完善Makefile

~~~makefile
CXX := g++
CFLAGS := -std=c++14 -O1 -g
OBJ_DIR := ./obj
BIN_DIR := ./bin
# 添加编译所需的源文件
OBJS = ${OBJ_DIR}/main.o ${OBJ_DIR}/webserver.o ${OBJ_DIR}/epoller.o ${OBJ_DIR}/buffer.o \
	   ${OBJ_DIR}/httprequest.o ${OBJ_DIR}/httpresponse.o ${OBJ_DIR}/httpconn.o 

.PHONY: mk_dir bin clean

all: mk_dir bin

mk_dir:
	if [ ! -d ${OBJ_DIR}  ]; then mkdir ${OBJ_DIR};fi
	if [ ! -d ${BIN_DIR}  ]; then mkdir ${BIN_DIR};fi

bin: $(OBJS)
	${CXX} ${CFLAGS} ${OBJS} -o ./bin/server -pthread  
	
${OBJ_DIR}/main.o: ./main.cpp
	${CXX} ${CFLAGS} -o $@ -c $<

${OBJ_DIR}/webserver.o: ./webserver.cpp
	${CXX} ${CFLAGS} -o $@ -c $<

${OBJ_DIR}/epoller.o: ./epoller.cpp
	${CXX} ${CFLAGS} -o $@ -c $<

${OBJ_DIR}/buffer.o: ./buffer.cpp 
	${CXX} ${CFLAGS} -o $@ -c $<

${OBJ_DIR}/httprequest.o: ./httprequest.cpp 
	${CXX} ${CFLAGS} -o $@ -c $<

${OBJ_DIR}/httpresponse.o: ./httpresponse.cpp 
	${CXX} ${CFLAGS} -o $@ -c $<

${OBJ_DIR}/httpconn.o: ./httpconn.cpp
	${CXX} ${CFLAGS} -o $@ -c $<

clean:
	rm -rf ./bin ./obj 
~~~

#### 3.2 在http资源文件的目录下添加一个  index.html 网页文件

是一个代码流星雨

~~~html
<!doctype html>
<html>
	<head>
		<meta charset="utf-8" />
		<title>流星雨</title>
		<meta name="keywords" content="关键词，关键字">
		<meta name="description" content="描述信息">
		<style>
			body {
				margin: 0;
				overflow: hidden;
			}
		</style>
	</head>
 
	<body>
 
		<!--
			<canvas>画布 画板 画画的本子
		-->
		<canvas width=400 height=400 style="background:#000000;" id="canvas"></canvas>
 
		<!--
			javascript
			画笔
		--> 
		<script>
					
			//获取画板
			//doccument 当前文档
			//getElement 获取一个标签
			//ById 通过Id名称的方式
			//var 声明一片空间
			//var canvas 声明一片空间的名字叫做canvas
			var canvas = document.getElementById("canvas");
			//获取画板权限 上下文
			var ctx = canvas.getContext("2d");
			//让画板的大小等于屏幕的大小
			/*
				思路：
					1.获取屏幕对象
					2.获取屏幕的尺寸
					3.屏幕的尺寸赋值给画板
			*/
			// 获取屏幕对象
			var s = window.screen;
			// 获取屏幕的宽度和高度
			var w = s.width;
			var h = s.height;
			// 设置画板的大小
			canvas.width = w;
			canvas.height = h;
 
			// 设置文字大小 
			var fontSize = 14;
			// 计算一行有多少个文字 取整数 向下取整
			var clos = Math.floor(w/fontSize);
			// 思考每一个字的坐标
			// 创建数组把clos 个 0 （y坐标存储起来）
			var drops = [];
			var str = "qwertyuiopasdfghjklzxcvbnm";
			// 往数组里面添加 clos 个 0
			for(var i = 0; i < clos; i++) {
				drops.push(0);
			}
 
			// 绘制文字
			function drawString() {
				ctx.fillStyle = "rgba(0,0,0,0.05)"        // 给矩形设置填充色
				ctx.fillRect(0,0,w,h);                    // 绘制一个矩形
				ctx.font = "600 "+fontSize+"px 微软雅黑"; // 添加文字样式	
				ctx.fillStyle = "#00ff00";                // 设置文字颜色
 
				for(var i = 0;i<clos;i++) {
					var x = i*fontSize;         // x坐标
					var y = drops[i]*fontSize;  // y坐标
					// 设置绘制文字
					ctx.fillText(str[Math.floor(Math.random()*str.length)],x,y);
					if(y>h&&Math.random()>0.99){
						drops[i] = 0;
					}
					drops[i]++;
				}
					
			}
			// 定义一个定时器，每隔30毫秒执行一次
			setInterval(drawString,30);
		</script>
	</body>
</html>

~~~

#### 3.3 结果图片

![img](https://s2.loli.net/2022/05/13/zLhAD7HvxtF3u5R.png)

#### 3.3 用之前改造过的 webbech(web压力测试工具)，去测试一下网站的负载能力

先用 webbech测试一下http://www.sina.com/

![img](https://s2.loli.net/2022/05/13/dXSJotbxu9OIVcE.png)

再测一下自己写的web服务器

![img](https://s2.loli.net/2022/05/16/HSI3BdihCulJqg8.png)