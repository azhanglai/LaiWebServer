#ifndef _HTTPREQUEST_H
#define _HTTPREQUEST_H

#include "./define.h"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>     
#include <mysql/mysql.h>

#include "./buffer.h"
#include "./sqlconnpool.h"
#include "./sqlconnRAII.h"

class HttpRequest {
public:
    enum PARSE_STATE {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,        
    };

    HttpRequest() { Init(); }
    ~HttpRequest() = default;

    void Init();
    bool parse(Buffer& buff);

    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;
    bool IsKeepAlive() const;

    std::string path() const { return m_path; }
    std::string& path() { return m_path; }
    std::string method() const { return m_method; }
    std::string version() const { return m_version; }

private:
    PARSE_STATE m_state;
    std::string m_method, m_path, m_version, m_body;
    std::unordered_map<std::string, std::string> m_header;
    std::unordered_map<std::string, std::string> m_post;

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;

    bool _Parse_RequestLine(const std::string& line);
    void _Parse_Header(const std::string& line);
    void _Parse_Body(const std::string& line);

    void _Parse_Path();
    void _Parse_Post();
    void _Parse_FromUrlencoded();

    static bool User_Verify(const std::string& name, const std::string& pwd, bool isLogin);
    static int Conver_Hex(char ch);
};


#endif /* _HTTPREQUEST_H */

