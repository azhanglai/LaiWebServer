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

