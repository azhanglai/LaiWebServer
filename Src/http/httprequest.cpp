#include "../include/httprequest.h"
using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{
    "/index", "/register", "/login",
    "/welcome", "/video", "/picture" };

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {
    {"/register.html", 0}, {"/login.html", 1}};

void HttpRequest::Init() {
    m_method = m_path = m_version = m_body = "";
    m_state = REQUEST_LINE;
    m_header.clear();
    m_post.clear();
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
        m_method = subMatch[1];        // 请求方法
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
        // 注册或登录界面
        if(DEFAULT_HTML_TAG.count(m_path)) {
            int tag = DEFAULT_HTML_TAG.find(m_path)->second;
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);
                if(User_Verify(m_post["username"], m_post["password"], isLogin)) {
                    m_path = "/welcome.html";
                } else {
                    m_path = "/error.html";
                }
            }
        }
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

// 用户验证
bool HttpRequest::User_Verify(const string &name, const string &pwd, bool isLogin) {
    if(name == "" || pwd == "") { return false; }
    MYSQL* sql;
    SqlConnRAII(&sql,  SqlConnPool::Instance());
    assert(sql);
    
    bool flag = false;
    unsigned int j = 0;
    char order[256] = {0};
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;
    
    // 登录为false, 注册为true
    if(!isLogin) { flag = true; }
    // 查询用户及密码
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    // mysql_query：成功返回0，失败返回1
    if(mysql_query(sql, order)) { 
        mysql_free_result(res);
        return false; 
    }
    // mysql_store_result:检索一个完整的结果集合给客户
    res = mysql_store_result(sql);
    // mysql_num_fields:返回一个结果集合中的列的数量
    j = mysql_num_fields(res);
    // mysql_fetch_fields:返回一个所有字段结构的数组
    fields = mysql_fetch_fields(res);
    // mysql_fetch_row:从结果集合中取得下一行
    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        string password(row[1]);
        // 登录行为, 登录成功flag为true
        if (isLogin) {
            if(pwd == password) { flag = true; }
            else { flag = false; }
        } else { 
            flag = false; 
        }
    }

    mysql_free_result(res);
    // 注册行为, 并且没有被登录过
    if(!isLogin && flag == true) {
        bzero(order, 256);
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        if(mysql_query(sql, order)) { 
            flag = false; 
        }
        flag = true;
    }
    SqlConnPool::Instance()->FreeConn(sql);
    return flag;
}

