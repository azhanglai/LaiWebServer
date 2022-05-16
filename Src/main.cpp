#include "./include/config.h"
#include "./include/webserver.h"


int main(int argc, char* argv[]) {
	// 端口 ET模式 timeoutMs 优雅退出  
	// Mysql配置（端口，用户名，用户密码，数据库名）
	// 连接池数量 线程池数量
    
    int sqlPort = 3306;
    const char* sqlUser = "root";
    const char* sqlPasswd = "********";
    const char* dbName = "laidb";
    
    Config cfg;
    cfg.Parse_Arg(argc, argv);

    WebServer server(cfg.m_port, 
                     cfg.m_trigMode, 
                     cfg.m_timeout, 
                     cfg.m_optLinger, 
                     cfg.m_threadPoolNum,
                     cfg.m_sqlPoolNum,
                     sqlPort,
                     sqlUser,
                     sqlPasswd,
                     dbName);

    server.Run();

	return 0;   
} 


  
