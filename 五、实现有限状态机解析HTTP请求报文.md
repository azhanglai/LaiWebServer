[TOC]

### 1、简单了解HTTP协议

- HTTP协议是Hyper Text Transfer Protocol（超文本传输协议）的缩写。HTTP 协议和 TCP/IP 协议族内的其他众多的协议相同， 用于客户端和服务器之间的通信。请求访问文本或图像等资源的一端称为客户端， 而提供资源响应的一端称为服务器端。

#### 1.1 http1.0主要特点

1. 简单快速：当客户端向服务器端发送请求时，只是简单的填写请求路径和请求方法即可，然后就可以通过浏览器或其他方式将该请求发送就行了 。
2. 灵活： HTTP 协议允许客户端和服务器端传输任意类型任意格式的数据对象。
3. 无连接：无连接的含义是限制每次连接只处理一个请求。服务器处理完客户的请求，并收到客户的应答后，即断开连接，采用这种方式可以节省传输时间。(当今多数服务器支持Keep-Alive功能，使用服务器支持长连接，解决无连接的问题)
4. 无状态：无状态是指协议对于事务处理没有记忆能力，服务器不知道客户端是什么状态。即客户端发送HTTP请求后，服务器根据请求，会给我们发送数据，发送完后，不会记录信息。(使用 cookie 机制可以保持 session，解决无状态的问题)

#### 1.2 http1.1主要特点

1. 默认持久连接节省通信量：只要客户端服务端任意一端没有明确提出断开TCP连接，就一直保持连接，可以发送多次HTTP请求 。
2. 管线化：客户端可以同时发出多个HTTP请求，而不用一个个等待响应 。
3. 断点续传：就是可以将一个大数据，分段传输，客户端可以慢慢显示。

#### 1.3  http2.0主要特点

1. HTTP/2采用二进制格式而非文本格式
2. HTTP/2是完全多路复用的，而非有序并阻塞的，只需一个HTTP连接就可以实现多个请求响应
3. 使用报头压缩，HTTP/2降低了开销
4. HTTP/2让服务器可以将响应主动“推送”到客户端缓存中

#### 1.4 URL（uniform resource locator，统一资源定位器）

~~~markdown
https://www.baidu.com:80/s?wd=http&rsv_spt=1&rsv_iqid=0x9a938ccf0000cc86&issp=1&f=8&rsv_bp=1&rsv_idx=2&ie=utf-8&tn=78000241_11_hao_pg&rsv_enter=1&rsv_dl=tb&rsv_sug3=6&rsv_sug2=0&rsv_btype=i&inputT=1206&rsv_sug4=18893
~~~

一个完整的URL包括以下几部分：

1.协议部分：该URL的协议部分为“http：”，这代表网页使用的是HTTP协议。

2.域名部分：该URL的域名部分为“www.baidu.com”。一个URL中，也可以使用IP地址作为域名使用

3.端口部分：跟在域名后面的是端口，域名和端口之间使用“:”作为分隔符。端口不是一个URL必须的部分，如果省略端口部分，将采用默认端口

4.虚拟目录部分：从域名后的第一个“/”开始到最后一个“/”为止，是虚拟目录部分。虚拟目录也不是一个URL必须的部分。

5.文件名部分：从域名后的最后一个“/”开始到“？”为止，是文件名部分，如果没有“?”,则是从域名后的最后一个“/”开始到“#”为止，是文件部分，如果没有“？”和“#”，那么从域名后的最后一个“/”开始到结束，都是文件名部分。文件名部分也不是一个URL必须的部分，如果省略该部分，则使用默认的文件名

6.锚部分：从“#”开始到最后，都是锚部分。本例中的锚部分是“name”。锚部分也不是一个URL必须的部分

7.参数部分：从“？”开始到“#”为止之间的部分为参数部分，又称搜索部分、查询部分。

#### 1.5 Request 请求报文

~~~http
// 请求行
GET /XXX.jpg HTTP/1.1
// 请求头
Host    img.mukewang.com
User-Agent    Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.106 Safari/537.36
Accept    image/webp,image/*,*/*;q=0.8
Referer    http://www.imooc.com/
Accept-Encoding    gzip, deflate, sdch
Accept-Language    zh-CN,zh;q=0.8
// 空行

// 请求体（这里没有）
~~~

#### 1.6 Response 响应报文

~~~http
// 响应状态行
HTTP/1.1 200 OK
// 响应头
Date: Fri, 22 May 2009 06:07:21 GMT
Content-Type: text/html; charset=UTF-8
// 空行

// 响应体
<html>
      <head></head>
      <body>
            <!--body goes here-->
      </body>
</html>
~~~

### 2、 解析请求报文

#### 2.1 httprequest.h 头文件

~~~c++
#ifndef _HTTPREQUEST_H
#define _HTTPREQUEST_H

#include "./define.h"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>     

#include "./buffer.h"

class HttpRequest {
public:
    // 解析状态
    enum PARSE_STATE {
        REQUEST_LINE, 	// 解析请求行状态
        HEADERS,		// 解析请求头状态
        BODY,			// 解析请求体状态
        FINISH,         // 解析请求报文结束状态
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

    bool _Parse_RequestLine(const std::string& line);
    void _Parse_Header(const std::string& line);
    void _Parse_Body(const std::string& line);

    void _Parse_Path();
    void _Parse_Post();
    void _Parse_FromUrlencoded();

    static int Conver_Hex(char ch);
};


#endif /* _HTTPREQUEST_H */

~~~

#### 2.2 httprequest.cpp 源文件

~~~c++
#include "./httprequest.h"
using namespace std;

// 默认html文件
const unordered_set<string> HttpRequest::DEFAULT_HTML{
    "/index", "/register", "/login",
    "/welcome", "/video", "/picture" };

// http请求对象初始化
void HttpRequest::Init() {
    m_method = m_path = m_version = m_body = ""; 	// 请求方法，请求URL， 请求HTTP版本，请求体
    m_state = REQUEST_LINE;							// 解析状态，从解析请求头开始
    m_header.clear();								// 保存请求头的消息键值对
    m_post.clear();									// post请求方法时，保存其消息的键值对
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if (m_post.count(key) == 1) {
        return m_post.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if (m_post.count(key) == 1) {
        return m_post.find(key)->second;
    }
    return "";
}

// 查看是否是长连接
bool HttpRequest::IsKeepAlive() const {
    if (m_header.count("Connection") == 1) {
        return m_header.find("Connection")->second == "keep-alive" && m_version == "1.1";
    }
    return false;
}

// 解析客户请求文件
bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n";
    // buffer中没有可读出来的请求数据，直接返回false
    if(buff.ReadableBytes() <= 0) {
        return false;
    }
    // 当buffer中有可读的请求数据和请求解析状态不为结束时，一直解析下去
    while(buff.ReadableBytes() && m_state != FINISH) {
        // 在可读区域找到每一行的行尾， 并且得到一行数据
        const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        std::string line(buff.Peek(), lineEnd);
        // 根据解析的状态来解析请求文件（有限状态机）
        // 初始解析状态为 解析请求行
        switch(m_state){
            // 请求行解析成功的话，状态转到解析请求头
            case REQUEST_LINE: {
                if(!_Parse_RequestLine(line)) {
                    return false;
                }
                _Parse_Path(); // 解析URL
                break;
            }
            // 请求头解析完成，状态转到解析请求体
            case HEADERS:
                _Parse_Header(line);
                if(buff.ReadableBytes() <= 2) {
                    m_state = FINISH;
                }
                break;
            case BODY:
                _Parse_Body(line);
                break;
            default:
                break;
        }
        if(lineEnd == buff.BeginWrite()) { break; }
        buff.RetrieveUntil(lineEnd + 2);    // 下一行
    }
    return true;
}

// 解析请求行（请求行内容如: GET http://www.baidu.com/ HTTP/1.1\r\n）
bool HttpRequest::_Parse_RequestLine(const string& line) {
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {   
        m_method = subMatch[1];         // 请求方法
        m_path = subMatch[2];           // 请求URL
        m_version = subMatch[3];        // HTTP协议版本
        m_state = HEADERS;              // 解析请求行结束后，请求状态到解析请求头
        return true;
    }
    return false;
}
// 解析URL
void HttpRequest::_Parse_Path() {
    if (m_path == "/") {
        m_path = "/index.html"; 
    } else {
        for (auto &item: DEFAULT_HTML) {
            if(item == m_path) {
                m_path += ".html";
                break;
            }
        }
    }
}

// 解析请求头
void HttpRequest::_Parse_Header(const string& line) {
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    if (regex_match(line, subMatch, patten)) {
        m_header[subMatch[1]] = subMatch[2];
    } else {
        m_state = BODY;     // 解析状态转到解析请求体
    }
}

// 解析请求体 
void HttpRequest::_Parse_Body(const string& line) {
    m_body = line;
    _Parse_Post();
    m_state = FINISH;       // 请求体解析完成，结束本次解析
}

// 把十六进制转成十进制
int HttpRequest::Conver_Hex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    return ch;
}

// POST请求方法时，需要解析请求体
void HttpRequest::_Parse_Post() {
    if(m_method == "POST" && m_header["Content-Type"] == "application/x-www-form-urlencoded") {
        _Parse_FromUrlencoded();
    }   
}

// 请求体格式（name=lai&password=123&realName=alai）
void HttpRequest::_Parse_FromUrlencoded() {
    if(m_body.size() == 0) { return; }

    string key, value;
    int num = 0, n = m_body.size();
    int i = 0, j = 0;
    // 解析等号左右的数据，并作成post键值对
    for(; i < n; i++) {
        char ch = m_body[i];
        switch (ch) {
            case '=': {
                key = m_body.substr(j, i - j);
                j = i + 1;
                break;
            }
            case '+': {
                m_body[i] = ' ';
                break;
            }
            case '%': {
                num = Conver_Hex(m_body[i + 1]) * 16 + Conver_Hex(m_body[i + 2]);
                m_body[i + 2] = num % 10 + '0';
                m_body[i + 1] = num / 10 + '0';
                i += 2;
                break;
            }
            case '&': {
                value = m_body.substr(j, i - j);
                j = i + 1;
                m_post[key] = value;
                break;
            }
            default:
                break;
        }
    }
    assert(j <= i);
    if(m_post.count(key) == 0 && j < i) {
        value = m_body.substr(j, i - j);
        m_post[key] = value;
    }
}
~~~

### 3、 根据客户请求作出响应报文

#### 3.1 httpresponse.h 头文件

~~~c++
#ifndef _HTTPRESPONSE_H
#define _HTTPRESPONSE_H

#include "./define.h"
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

~~~

#### 3.2 httpresponse.cpp 源文件

~~~c++
#include "./httpresponse.h"
using namespace std;

// 后缀类型与文本类型键值对
const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

// 响应状态码与状态描述键值对
const unordered_map<int, string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

// 响应状态码与错误网页键值对
const unordered_map<int, string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

// http响应对象
HttpResponse::HttpResponse() {
    m_code = -1;
    m_path = m_srcDir = "";
    m_isKeepAlive = false;
    m_mmFile = nullptr; 
    m_mmFileStat = {0};
};

HttpResponse::~HttpResponse() {
    UnmapFile();
}
// 初始化http响应对象
void HttpResponse::Init(const string& srcDir, string& path, bool isKeepAlive, int code){
    assert(srcDir != "");
    if (m_mmFile) { UnmapFile(); }
    m_code = code;                  // 响应状态码
    m_isKeepAlive = isKeepAlive;    // 是否长连接标记
    m_path = path;                  // 请求URL文件路径
    m_srcDir = srcDir;              // 源文件目录 
    m_mmFile = nullptr;             // 客户请求文件(后续映射到共享内存)
    m_mmFileStat = {0};             // 客户请求文件信息
}

// 释放共享内存
void HttpResponse::UnmapFile() {
    if(m_mmFile) {
        munmap(m_mmFile, m_mmFileStat.st_size);
        m_mmFile = nullptr;
    }
}

// 根据客户的请求，作出响应文件
void HttpResponse::Make_Response(Buffer& buff) {
    // stat: 获取文件信息，放到m_mmFileStat 
    // 如果客户请求文件是目录文件的话，客户找不到网页
    if(stat((m_srcDir + m_path).data(), &m_mmFileStat) < 0 || S_ISDIR(m_mmFileStat.st_mode)) {
        m_code = 404;
    }
    // 如果客户请求文件，其他用户没有可读权限，客户禁止访问
    else if(!(m_mmFileStat.st_mode & S_IROTH)) {
        m_code = 403;
    }
    else if(m_code == -1) { 
        m_code = 200; 
    }
    _Error_Html();          // 网页出错(如果响应状态码是错误码的话才会真正执行)
    _Add_StateLine(buff);   // 添加响应行 
    _Add_Header(buff);      // 添加响应头
    _Add_Content(buff);     // 添加响应体 
}

void HttpResponse::_Error_Html() {
    // 如果响应状态码是网页错误码的话，将客户请求文件改成网页出错文件
    if (CODE_PATH.count(m_code) == 1) {
        m_path = CODE_PATH.find(m_code)->second;
        stat((m_srcDir + m_path).data(), &m_mmFileStat);
    }
}

// 添加响应行 （格式如：HTTP/1.1 200 OK）
void HttpResponse::_Add_StateLine(Buffer& buff) {
    string status;
    if(CODE_STATUS.count(m_code) == 1) {
        status = CODE_STATUS.find(m_code)->second;
    }
    buff.Append("HTTP/1.1 " + to_string(m_code) + " " + status + "\r\n");
}

// 添加响应头
void HttpResponse::_Add_Header(Buffer& buff) {
    buff.Append("Connection: ");
    // 判断是否是长连接
    if(m_isKeepAlive) {
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    } else{
        buff.Append("close\r\n");
    }
    // 文本类型
    buff.Append("Content-type: " + _Get_FileType() + "\r\n");
}

// 添加响应体
void HttpResponse::_Add_Content(Buffer& buff) {
    // 以只读方式打开可和请求文件
    int srcFd = open((m_srcDir + m_path).data(), O_RDONLY);
    if(srcFd < 0) { 
        ErrorContent(buff, "File NotFound!");
        return; 
    }

    // 将文件映射到共享内存， MAP_PRIVATE 建立一个写入时拷贝的私有映射
    int* mmRet = (int*)mmap(0, m_mmFileStat.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(*mmRet == -1) {
        ErrorContent(buff, "File NotFound!");
        return; 
    }
    m_mmFile = (char*)mmRet;
    close(srcFd);

    // 添加文本信息长度
    buff.Append("Content-length: " + to_string(m_mmFileStat.st_size) + "\r\n\r\n");
}

string HttpResponse::_Get_FileType() {
    // 判断文件类型 
    string::size_type idx = m_path.find_last_of('.');
    if(idx == string::npos) {
        return "text/plain";
    }
    // 根据文件后缀名 得到 文本类型
    string suffix = m_path.substr(idx);
    if(SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}

// 添加错误文本 
void HttpResponse::ErrorContent(Buffer& buff, string message) {
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(m_code) == 1) {
        status = CODE_STATUS.find(m_code)->second;
    } else {
        status = "Bad Request";
    }
    body += to_string(m_code) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}

~~~

