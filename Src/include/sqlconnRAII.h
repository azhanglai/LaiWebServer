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

