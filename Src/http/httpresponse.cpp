#include "../include/httpresponse.h"

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

