#ifndef _HTTPRESPONSE_H
#define _HTTPRESPONSE_H

#include "define.h"
#include <unordered_map>

#include "./buffer.h"

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void Init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
    void Make_Response(Buffer& buff);
    void UnmapFile();
    void ErrorContent(Buffer& buff, std::string message);

    int Code() const { return m_code; }
    char* File() { return m_mmFile; }
    size_t FileLen() const { return m_mmFileStat.st_size; }

private:
    int m_code;
    bool m_isKeepAlive;

    std::string m_path;
    std::string m_srcDir;
    
    char* m_mmFile; 
    struct stat m_mmFileStat;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    static const std::unordered_map<int, std::string> CODE_STATUS;
    static const std::unordered_map<int, std::string> CODE_PATH;

    void _Add_StateLine(Buffer &buff);
    void _Add_Header(Buffer &buff);
    void _Add_Content(Buffer &buff);

    void _Error_Html();
    std::string _Get_FileType();

};


#endif /* _HTTPRESPONSE_H */

