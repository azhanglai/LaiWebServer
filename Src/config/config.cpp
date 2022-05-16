#include "../include/config.h"

Config::Config() {
    m_port = 8092;
    m_optLinger = 0;
    m_trigMode = 3;
    m_timeout = 60000;
    m_threadPoolNum = 8;
    m_sqlPoolNum = 10;

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

