[TOC]

### 1、 使用数据库连接池的原因

- 数据库连接池：是程序启动时建立足够的数据库连接，并将这些连接组成一个连接池，由程序动态地对连接池中的连接进行申请，使用，释放。

- 假设网站一天有很大的访问量，数据库服务器就需要为每次连接创建一次数据库连接，极大的浪费数据库资源，并且极易造成数据库服务器内存溢出，拓机。

- 数据库连接池在初始化时创建一定数量的数据库连接放到连接池中，这些数据在连接的数量上是由最小数据库连接数来设定的，无论这些数据库连接是否被使用，连接池都将一直保证至少拥有这么多的连接数量，连接池的最大数据库连接数量限定了这个连接池能占有的最大连接数。

- 连接池的分配与释放，对系统的性能有很大的影响。合理的分配与释放，可以提高连接的复用性，从而降低建立连接的开销，同时还可以加快用户的访问速度。

- 连接池主要只有以下三个方面的优势：

  第一、 减少创建连接的时间。连接池中的连接是已准备好的，可重复使用的，获取后可以直接访问数据库，因此减少了连接创建的次数和时间。

  第二、 简化的变成方式。当使用连接池时，每一个单独的线程能够像创建一个自己的JDBC连接一样操作，允许用户直接使用的次数和时间。

  第三、 控制资源的使用。 如果不使用连接池，每次访问数据库都需要创建一个连接，这样系统的稳定性受系统连接需求影响很大，很容易产生资源浪费和高负载异常。连接池能够使性能最大化，将资源利用控制在一定的水平下。连接池能控制池中的连接数量，增强了系统在大量用户应用时的稳定性。

### 2、 数据库连接池代码实现

#### 2.1 sqlconnpool.h 头文件

~~~c++
#ifndef _SQLCONNPOOL_H
#define _SQLCONNPOOL_H

#include "./define.h"
#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>

class SqlConnPool {
public:
    // 单例模式
    static SqlConnPool *Instance(); 			 

    MYSQL *GetConn();
    void FreeConn(MYSQL * conn);
    int GetFreeConnCount();

    void Init(const char* host, int port,
              const char* user,const char* pwd, 
              const char* dbName, int connSize);
    void ClosePool();

private:
    SqlConnPool();
    ~SqlConnPool();

    int MAX_CONN;
    int m_useCount;
    int m_freeCount;

    std::queue<MYSQL *> m_connQue;
    std::mutex m_mtx;
    sem_t m_semId;
};

#endif /* _SQLCONNPOOL_H */

~~~

#### 2.2 sqlconnpool.cpp 源文件

~~~c++
#include "./sqlconnpool.h"
using namespace std;

SqlConnPool::SqlConnPool() {
    m_useCount = 0;
    m_freeCount = 0;
}

SqlConnPool::~SqlConnPool() {
    ClosePool();
}

SqlConnPool* SqlConnPool::Instance() {
    static SqlConnPool connPool;
    return &connPool;
}

// 连接池初始化
void SqlConnPool::Init(const char* host, int port,
            const char* user,const char* pwd, const char* dbName,
            int connSize = 10) {
    assert(connSize > 0);
    for (int i = 0; i < connSize; i++) {
        MYSQL *sql = nullptr;
        sql = mysql_init(sql); // 获取或初始化一个MYSQL结构
        if (!sql) {
            assert(sql);
        }
        // mysql_real_connect：连接一个mysql服务器
        sql = mysql_real_connect(sql, host,
                                 user, pwd,
                                 dbName, port, nullptr, 0);
        if (!sql) {
            printf("mysql connect error!\n");
        }
        m_connQue.push(sql);            // 将mysql连接放入连接池中（队列）
    }
    MAX_CONN = connSize;                // 连接池中的连接数
    sem_init(&m_semId, 0, MAX_CONN);    // 用信号量管理连接池资源
}

// 关闭连接池
void SqlConnPool::ClosePool() {
    lock_guard<mutex> locker(m_mtx);
    while(!m_connQue.empty()) {
        auto item = m_connQue.front();
        m_connQue.pop();
        // mysql_close: 关闭一个mysql服务器连接
        mysql_close(item);
    }
    // 结束MySql库使用
    mysql_library_end();        
}

// 从后连接池中获取一个连接 
MYSQL* SqlConnPool::GetConn() {
    MYSQL *sql = nullptr;
    if(m_connQue.empty()){
        return nullptr;
    }
    sem_wait(&m_semId);             // 信号量wait操作 -1
    {
        // 作用域锁，防止资源竞争
        lock_guard<mutex> locker(m_mtx);
        sql = m_connQue.front();    // 取得连接
        m_connQue.pop();            // 连接从资源池中弹出
    }
    return sql;
}

// 空闲连接
void SqlConnPool::FreeConn(MYSQL* sql) {
    assert(sql);
    lock_guard<mutex> locker(m_mtx);
    m_connQue.push(sql);        // 空闲连接放入连接池 
    sem_post(&m_semId);         // 信号量post操作 +1
}

// 获取连接池中空闲的连接数
int SqlConnPool::GetFreeConnCount() {
    lock_guard<mutex> locker(m_mtx);
    return m_connQue.size();
}

~~~

### 3、连接池RAII(Resource Acquisition Is Initialization,资源获取即初始化）

~~~c++
#ifndef _SQLCONNRAII_H
#define _SQLCONNRAII_H
#include "sqlconnpool.h"

// 资源在对象构造初始化 资源在对象析构时释放 
class SqlConnRAII {
public:
    // 从连接池中获取一个mysql服务器连接
    SqlConnRAII(MYSQL** sql, SqlConnPool *connpool) {
        assert(connpool);
        *sql = connpool->GetConn(); // 从连接池中获取一个连接 
        m_sql = *sql;               // mysql服务器连接
        m_connpool = connpool;      // 连接池
    }
    // 回收一个mysql服务器连接到连接池中
    ~SqlConnRAII() {
        if(m_sql) {
            m_connpool->FreeConn(m_sql); 
        }
    }
    
private:
    MYSQL *m_sql;
    SqlConnPool* m_connpool;
};

#endif /* _SQLCONNRAII_H */
~~~

### 4、 完善webserver

~~~c++
#ifndef _WEBSERVER_H
#define _WEBSERVER_H

#include "./define.h"
#include "./epoller.h"
#include "./threadpool.h"
#include "./httpconn.h"
#include "./heaptimer.h"
#include "./sqlconnRAII.h" 		// 添加sqlconnRAII
#include <memory>
#include <unordered_map>

class WebServer {
public:
    // 添加参数：sql端口， sql用户名， sql用户密码， sql数据库名， sql数据库池连接数量
    WebServer(int port, bool OptLinger, int trigMode, int threadNum, int timeout, 
             int sqlPort, const char* sqlUser, const char* sqlPwd, const char* dbName, int connPoolNum);
    ~WebServer();
    void Run();

private:
    int m_port;
    bool m_openLinger;
    bool m_isClose;
    int m_listenFd;
    int m_timeout;
    
    uint32_t m_listenEvent;
    uint32_t m_connEvent;
    
    char* m_srcDir;

    static const int MAX_FD = 65536;
    
    std::unique_ptr<Epoller> m_epoller;
    std::unique_ptr<ThreadPool> m_threadpool;
    std::unique_ptr<HeapTimer> m_timer;
    std::unordered_map<int, HttpConn> m_users;

    bool _Init_Socket();
    void _Init_EventMode(int trigMode);
    static int Set_FdNonblock(int fd);
    
    void _Deal_Listen();
    void _Deal_Read(HttpConn*);
    void _Deal_Write(HttpConn*);
    
    void _Thread_Read(HttpConn*);
    void _Thread_Write(HttpConn*);

    void _On_Process(HttpConn*);
    void _Close_Conn(HttpConn*);
    
    void _Extent_Time(HttpConn*);
};

#endif /* _WEBSERVER_H */
~~~

~~~c++
#include "./webserver.h"

WebServer::WebServer(int port, bool OptLinger, int trigMode, int threadNum, int timeout, 
                    int sqlPort, const char* sqlUser, const char* sqlPwd, const char* dbName, int connPoolNum)
    : m_port(port), m_openLinger(OptLinger),  m_isClose(false), m_timeout(timeout), 
    m_epoller(new Epoller()), m_threadpool(new ThreadPool(threadNum)), m_timer(new HeapTimer())
{    

    m_srcDir = getcwd(nullptr, 256);
    strncat(m_srcDir, "/resources/", 16);
    HttpConn::userCount = 0;
    HttpConn::srcDir = m_srcDir;
	// 初始化数据库连接池
    SqlConnPool::Instance()->Init("127.0.0.1",
                                 sqlPort,
                                 sqlUser, 
                                 sqlPwd, 
                                 dbName, 
                                 connPoolNum);

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
    SqlConnPool::Instance()->ClosePool(); 	// 关闭连接池
}
~~~



