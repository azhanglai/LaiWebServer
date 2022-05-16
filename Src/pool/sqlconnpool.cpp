#include "../include/sqlconnpool.h"
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

void SqlConnPool::Init(const char* host, int port,
            const char* user,const char* pwd, const char* dbName,
            int connSize = 10) {
    assert(connSize > 0);
    for (int i = 0; i < connSize; i++) {
        MYSQL *sql = nullptr;
        sql = mysql_init(sql); // 获取或初始化一个MYSQL结构
        if (!sql) {
            printf("mysql init error!\n");
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

