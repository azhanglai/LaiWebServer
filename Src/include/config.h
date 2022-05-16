#ifndef _CONFIG_H
#define _CONFIG_H

#include "./define.h"

class Config {
public:
    Config();
    ~Config() = default;

    void Parse_Arg(int argc, char* argv[]);

    int m_port;
    int m_optLinger;
    int m_trigMode;
    int m_timeout;
    int m_threadPoolNum;
    int m_sqlPoolNum;

};

#endif /* _CONFIG_H */


