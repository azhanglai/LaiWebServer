[TOC]

### 1、 命令行参数

~~~shell
./server [-p port] [-o optLinger] [-m trigMode] [-T timeout] [-t threadPoolNum] [-s sqlPoolNum]

1. -p: 端口号，默认8092
2. -m: listenfd和connfd的模式组合，0表示LT+LT，1表示LT+ET，2表示ET+LT，3表示ET+ET，默认为3
3. -o: 优雅关闭连接，0表示不使用，1表示使用，默认为0
4. -T: 客户连接超时时间，默认60s
5. -t: 线程数量，默认为8
6. -s: 数据库连接数量，默认为10
~~~

### 2、 命令行解析函数getopt

~~~c++
// 例子：optstr: “xy:z::” 表示x选项没有选项参数，y选项必须有选项参数，中间可有空格可没有，z参数可有参数选项，也可没有参数选项，但是如果有选项参数的话必须紧跟在选项之后不能有空格。

int opt;
const char* str = "p:o:m:T:t:s:";
while (~(opt = getopt(argc, argv, str))) {
	switch(opt) {
    	case 'p': m_port = atoi(optarg); break;
        case 'o': m_optLinger = atoi(optarg); break;
        case 'm': m_trigMode = atoi(optarg); break;
        case 'T': m_timeout = atoi(optarg); break;
        case 't': m_threadPoolNum = atoi(optarg); break;
        case 's': m_sqlPoolNum = atoi(optarg); break;
    }
}
~~~

### 3、 config头文件

~~~c++
#ifndef _CONFIG_H
#define _CONFIG_H

#include "./define.h"

class Config {
public:
    Config();
    ~Config() = default;

    void Parse_Arg(int argc, char* argv[]);

    int m_port;					// 端口号
    int m_optLinger;			// 优雅关闭连接
    int m_trigMode;				// 触发组合模式
    int m_timeout;				// 客户连接超时时间		
    int m_threadPoolNum;		// 线程池内线程数量
    int m_sqlPoolNum;			// 数据库连接池数量

};

#endif /* _CONFIG_H */

~~~

### 4、 config.cpp文件

~~~c++
#include "./config.h"

Config::Config() {
    m_port = 8092;			// 端口号           -p, 默认8090
    m_optLinger = 0;		// 优雅关闭连接      -o, 默认不使用
    m_trigMode = 3;			// 触发组合模式      -m, 默认ET + ET
    m_timeout = 60000;      // 客户连接超时时间   -T, 默认60秒
    m_threadPoolNum = 8; 	// 线程池内线程数量   -t, 默认为8
    m_sqlPoolNum = 10;	 	// 数据库连接池数量   -s, 默认为10

}
void Config::Parse_Arg(int argc, char* argv[]) {
    int opt;
    const char* str = "p:o:m:T:t:s:";
    while (~(opt = getopt(argc, argv, str))) {
        switch(opt) {
            case 'p': m_port = atoi(optarg); break;
            case 'o': m_optLinger = atoi(optarg); break;
            case 'm': m_trigMode = atoi(optarg); break;
            case 'T': m_timeout = atoi(optarg); break;
            case 't': m_threadPoolNum = atoi(optarg); break;
            case 's': m_sqlPoolNum = atoi(optarg); break;
        }
    }
}

~~~









