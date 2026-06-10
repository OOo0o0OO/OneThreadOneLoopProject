#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cctype>
#include <sys/stat.h>
#include <regex>
#include <deque>
#include "server.hpp"

/// @brief 工具类模块
// 状态码-描述信息集合
static const std::unordered_map<int, std::string> status_code_description = {
    {100, "Continue"},
    {101, "Switching Protocols"},
    {102, "Processing"},
    {103, "Early Hints"},
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    {207, "Multi-Status"},
    {208, "Already Reported"},
    {226, "IM Used (HTTP Delta encoding)"},
    {300, "Multiple Choices"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {306, "Unused"},
    {307, "Temporary Redirect"},
    {308, "Permanent Redirect"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Timeout"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Content Too Large"},
    {414, "URI Too Long"},
    {415, "Unsupported Media Type"},
    {416, "Range Not Satisfiable"},
    {417, "Expectation Failed"},
    {418, "I'm a teapot"},
    {421, "Misdirected Request"},
    {422, "Unprocessable Content"},
    {423, "Locked"},
    {424, "Failed Dependency"},
    {425, "Too Early"},
    {426, "Upgrade Required"},
    {428, "Precondition Required"},
    {429, "Too Many Requests"},
    {431, "Request Header Fields Too Large"},
    {451, "Unavailable For Legal Reasons"},
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"},
    {506, "Variant Also Negotiates"},
    {507, "Insufficient Storage"},
    {508, "Loop Detected"},
    {510, "Not Extended"},
    {511, "Network Authentication Required"}};

// 扩展名-Mime类型集合
static const std::unordered_map<std::string, std::string> extension_mime_type = {
    {".aac", "audio/aac"},
    {".abw", "application/x-abiword"},
    {".apng", "image/apng"},
    {".arc", "application/x-freearc"},
    {".avif", "image/avif"},
    {".avi", "video/x-msvideo"},
    {".azw", "application/vnd.amazon.ebook"},
    {".bin", "application/octet-stream"},
    {".bmp", "image/bmp"},
    {".bz", "application/x-bzip"},
    {".bz2", "application/x-bzip2"},
    {".cda", "application/x-cdf"},
    {".csh", "application/x-csh"},
    {".css", "text/css"},
    {".csv", "text/csv"},
    {".doc", "application/msword"},
    {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".eot", "application/vnd.ms-fontobject"},
    {".epub", "application/epub+zip"},
    {".gz", "application/gzip"},
    {".gif", "image/gif"},
    {".htm", "text/html"},
    {".html", "text/html"},
    {".ico", "image/vnd.microsoft.icon"},
    {".ics", "text/calendar"},
    {".jar", "application/java-archive"},
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpeg"},
    {".js", "text/javascript"},
    {".json", "application/json"},
    {".jsonld", "application/ld+json"},
    {".md", "text/markdown"},
    {".mid", "audio/midi"},
    {".midi", "audio/midi"},
    {".mjs", "text/javascript"},
    {".mp3", "audio/mpeg"},
    {".mp4", "video/mp4"},
    {".mpeg", "video/mpeg"},
    {".mpkg", "application/vnd.apple.installer+xml"},
    {".odp", "application/vnd.oasis.opendocument.presentation"},
    {".ods", "application/vnd.oasis.opendocument.spreadsheet"},
    {".odt", "application/vnd.oasis.opendocument.text"},
    {".oga", "audio/ogg"},
    {".ogv", "video/ogg"},
    {".ogx", "application/ogg"},
    {".opus", "audio/ogg"},
    {".otf", "font/otf"},
    {".png", "image/png"},
    {".pdf", "application/pdf"},
    {".php", "application/x-httpd-php"},
    {".ppt", "application/vnd.ms-powerpoint"},
    {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {".rar", "application/vnd.rar"},
    {".rtf", "application/rtf"},
    {".sh", "application/x-sh"},
    {".svg", "image/svg+xml"},
    {".tar", "application/x-tar"},
    {".tif", "image/tiff"},
    {".tiff", "image/tiff"},
    {".ts", "video/mp2t"},
    {".ttf", "font/ttf"},
    {".txt", "text/plain"},
    {".vsd", "application/vnd.visio"},
    {".wav", "audio/wav"},
    {".weba", "audio/webm"},
    {".webm", "video/webm"},
    {".webmanifest", "application/manifest+json"},
    {".webp", "image/webp"},
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".xhtml", "application/xhtml+xml"},
    {".xls", "application/vnd.ms-excel"},
    {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".xml", "application/xml"},
    {".xul", "application/vnd.mozilla.xul+xml"},
    {".zip", "application/zip"},
    {".3gp", "video/3gpp"},
    {".3g2", "video/3gpp2"},
    {".7z", "application/x-7z-compressed"}};

class Util
{
private:
    static int HexadecimalToDecimal(char hex_char)
    {
        int dec_val;
        // 0 1 2 3 4 5 6 7 8 9 A B C D E F
        if (hex_char >= '0' && hex_char <= '9')
        {
            dec_val = hex_char - '0';
        }
        else if (hex_char >= 'A' && hex_char <= 'F')
        {
            dec_val = hex_char - 'A' + 10;
        }
        else if (hex_char >= 'a' && hex_char <= 'f')
        {
            dec_val = hex_char - 'a' + 10;
        }
        else
        {
            dec_val = -1;
        }
        return dec_val;
    }

public:
    // 分割字符串
    static size_t Split(const std::string symbols, const std::string &str, std::vector<std::string> *sub_strs)
    {
        if (str.empty() || symbols.empty() || sub_strs == nullptr)
            return 0;
        size_t offset = 0;
        while (offset < str.size())
        {
            // 查找符号
            size_t end = str.find(symbols, offset);
            // 没找到---全部截取
            if (end == std::string::npos)
            {
                // 确保不越界
                if (offset < str.size())
                    sub_strs->push_back(str.substr(offset));
                break;
            }
            // 找到了---截取部分后继续查找
            size_t sub_str_size = end - offset;
            // 不插入空字符串
            if (end != offset)
            {
                sub_strs->push_back(str.substr(offset, sub_str_size));
            }
            // 移动偏移量
            offset = (end + symbols.size());
        }
        return sub_strs->size();
    }
    // 从文件中读取内容
    static bool ReadFromFile(std::string file_name, std::string *out)
    {
        std::ifstream istrm(file_name, std::ios::binary);
        // 1. 打开失败
        if (!istrm.is_open())
        {
            return false;
        }
        // 2. 打开成功
        else
        {
            // a) 获取文件大小
            istrm.seekg(0, std::ios::end);
            size_t file_size = istrm.tellg();
            istrm.seekg(0, std::ios::beg);
            // b) 读取文件内容
            out->resize(file_size);
            istrm.read(&(*out)[0], file_size);
            // c) 检查状态
            if (istrm.good() == false)
            {
                istrm.close();
                return false;
            }
        }
        // 3. 关闭文件
        istrm.close();
        return true;
    }
    // 向文件中写入内容
    static bool WriteToFile(std::string file_name, const std::string &in)
    {
        std::ofstream ostrm(file_name, std::ios::binary | std::ios::trunc);
        if (!ostrm.is_open())
        {
            return false;
        }
        else
        {
            ostrm.write(&in[0], in.size());
            if (ostrm.good() == false)
            {
                ostrm.close();
                return false;
            }
        }
        ostrm.close();
        return true;
    }
    // URL 编码
    static void URLEncode(const std::string &raw_rul, std::string *encode_url, bool is_covert_space_to_plus)
    {
        if (raw_rul.empty() || encode_url == nullptr)
            return;
        // 将特殊的 ASCII 值，转换为：%+16进制值，eg. C++(+ -> O:43 H: 2B) -> C%2B%2B
        // 不转换的 ASCII: A~Z  a~z  0~9  .  -  ~  _
        // 关于空格的特殊处理: RFC 标准规定，编码为 %HH  W3C 标准规定，编码为 +
        for (int i = 0; i < raw_rul.size(); i++)
        {
            // 字符类型默认是有符号的，不需要关系符号的正负，负数会导致范围 00~FF 溢出
            unsigned char c = static_cast<unsigned char>(raw_rul[i]);
            // 1. 不参与编码的字符
            if (c == '.' || c == '-' || c == '~' || c == '_' || isalnum(c))
            {
                *encode_url += raw_rul[i];
            }
            // 2. 参与编码的字符
            else
            {
                //  a) 空格转 +
                if (is_covert_space_to_plus && c == ' ')
                {
                    *encode_url += '+';
                }
                //  b) 空格正常转
                else
                {
                    char tmp[4]; // 一个字节 8 个比特位，范围 0~255 -> 00~FF
                    snprintf(tmp, sizeof(tmp), "%%%02X", c);
                    *encode_url += tmp;
                }
            }
        }
    }
    // URL 解码
    static void URLDecode(const std::string &encode_url, std::string *decode_url, bool is_covert_plus_to_space)
    {
        if (encode_url.empty() || decode_url == nullptr)
            return;
        for (int i = 0; i < encode_url.size(); i++)
        {
            char c = static_cast<char>(encode_url[i]);
            // 1. + 转空格
            if (is_covert_plus_to_space && c == '+')
            {
                *decode_url += ' ';
            }
            // 2. 遇到 % 要同时处理后面的两个字符
            else if (c == '%' && i + 2 < encode_url.size())
            {
                // %2B
                // 2
                int high = HexadecimalToDecimal(encode_url[i + 1]);
                // B
                int low = HexadecimalToDecimal(encode_url[i + 2]);
                if (high != -1 && low != -1)
                {
                    int ascii_code = high * 16 + low;
                    *decode_url += static_cast<char>(ascii_code);
                    i += 2;
                }
                else
                {
                    *decode_url += c;
                }
            }
            // 3. 无需转换的字符
            else
            {
                *decode_url += c;
            }
        }
    }

    // 通过状态码获取描述信息
    static std::string GetDescriptionFromStatus(int status)
    {
        auto it = status_code_description.find(status);
        if (it == status_code_description.end())
        {
            return "Unknown";
        }
        return it->second;
    }
    // 通过文件名获取 Mime
    static std::string GetMimeFromFileName(const std::string &full_file_name)
    {
        size_t extension_pos = full_file_name.find_last_of('.');
        if (extension_pos == std::string::npos)
            return "application/octet-stream";
        auto it = extension_mime_type.find(full_file_name.substr(extension_pos));
        if (it == extension_mime_type.end())
            return "application/octet-stream";
        return it->second;
    }
    // 是否是目录
    static bool IsDirectory(const std::string directory_name)
    {
        struct stat directory_stat;
        if (stat(directory_name.c_str(), &directory_stat) != 0)
        {
            // TODO
            return false;
        }
        return S_ISDIR(directory_stat.st_mode);
    }
    // 是否是普通文件
    static bool IsRegularFile(const std::string file_name)
    {
        struct stat file_stat;
        if (stat(file_name.c_str(), &file_stat) != 0)
        {
            // TODO
            return false;
        }
        return S_ISREG(file_stat.st_mode);
    }
    // 路径是否有效
    static bool IsValidPath(const std::string &path)
    {
        if (path.empty())
            return false;
        // 不能访问到根目录之外的资源
        std::vector<std::string> sub_path_strs;
        // 按照 / 进行分割
        Split("/", path, &sub_path_strs);
        int depth = 0;
        // 检查目录深度
        for (auto &sub : sub_path_strs)
        {
            // a) 空字符或者当前目录
            if (sub == " " || sub == ".")
            {
                continue;
            }
            // b) 遇到 .. 回退一级，深度减 1
            if (sub == "..")
            {
                depth--;
                if (depth < 0)
                {
                    // 一旦超出当前目录即返回
                    return false;
                }
            }
            // c) 正常目录，深度加 1
            else
            {
                depth++;
            }
        }
        return depth >= 0 ? true : false;
    }
    // 剔除空格
    static std::string Trim(const std::string &s)
    {
        size_t begin = 0;
        while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin])))
            ++begin;

        size_t end = s.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])))
            --end;

        return s.substr(begin, end - begin);
    }
    // 转小写
    static std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c)
                       {
                           return static_cast<char>(std::tolower(c));
                       });
        return s;
    }
    // 转小写
    static std::string ToUpper(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c)
                       {
                           return static_cast<char>(std::toupper(c));
                       });
        return s;
    }
    // 大小写敏感
    static bool EqualCaseInsensitive(std::string lhs, std::string rhs)
    {
        return ToLower(std::move(lhs)) == ToLower(std::move(rhs));
    }
    // 按字符分割
    static std::vector<std::string> SplitByChar(const std::string &s, char sep)
    {
        std::vector<std::string> out;
        size_t start = 0;

        while (start <= s.size())
        {
            size_t pos = s.find(sep, start);
            if (pos == std::string::npos)
            {
                out.push_back(s.substr(start));
                break;
            }

            out.push_back(s.substr(start, pos - start));
            start = pos + 1;
        }

        return out;
    }
    // 将 response 中的头部字段统一转换为小写存储---输出时再规范化
    static std::string CanonicalHeaderName(const std::string &key)
    {
        std::string normalized_key = Util::ToLower(Util::Trim(key));
        if (normalized_key == "content-type")
            return "Content-Type";
        if (normalized_key == "content-length")
            return "Content-Length";
        if (normalized_key == "connection")
            return "Connection";
        if (normalized_key == "host")
            return "Host";
        if (normalized_key == "user-agent")
            return "User-Agent";
        // 其余的小写也没问题
        return normalized_key;
    }
};

/// @brief HTTP 请求模块
class Request
{
public:
    Request() {}
    ~Request() {}
    // 设置请求方法
    void SetMethod(const std::string &method)
    {
        _method = method;
    }
    // 设置请求路径
    void SetPath(const std::string &path)
    {
        _path = path;
    }
    // 设置 HTTP 版本
    void SetVersion(const std::string &version)
    {
        _version = version;
    }
    // 设置请求头部---统一转小写存储
    void SetHeaders(const std::string &key, const std::string &value)
    {
        std::string normal_key = Util::ToLower(Util::Trim(key));
        std::string normal_value = Util::Trim(value);
        _headers[normal_key] = normal_value;
        // _headers.insert(std::make_pair(key, value));
    }
    // 设置查询参数
    void SetQueryParameter(const std::string &key, const std::string &value)
    {
        _query_parameters.insert(std::make_pair(key, value));
    }
    // 设置请求正文
    void SetBody(const std::string &body)
    {
        _body = body;
    }
    void AppendBody(const std::string &body)
    {
        _body.append(body);
    }

    // 获取请求方法
    std::string GetMethod()
    {
        return _method;
    }
    // 获取请求路径
    std::string GetPath()
    {
        return _path;
    }
    // 获取 HTTP 版本
    std::string GetVersion()
    {
        return _version;
    }
    // 获取请求头部
    std::unordered_map<std::string, std::string> GetHeaders()
    {
        return _headers;
    }
    std::string GetHeadersValueByKey(const std::string &key) const
    {
        std::string normalized_key = Util::ToLower(Util::Trim(key));
        auto it = _headers.find(normalized_key);
        if (it == _headers.end())
        {
            return "";
        }
        return it->second;
    }
    // 获取查询参数
    std::unordered_map<std::string, std::string> GetQueryParameter()
    {
        return _query_parameters;
    }
    std::string GetQueryParameterValueByKey(const std::string &key)
    {
        auto it = _query_parameters.find(key);
        if (it == _query_parameters.end())
        {
            return "";
        }
        return it->second;
    }
    // 获取请求正文
    std::string GetBody()
    {
        return _body;
    }
    // 获取连接长度
    size_t GetContextLength() const
    {
        std::string content_length = GetHeadersValueByKey("content-Length");
        if (content_length.empty())
            return 0;
        return std::stol(content_length);
    }
    // 获取正文类型
    std::string GetContentType() const
    {
        return GetHeadersValueByKey("content-Type");
    }
    // 头部中是否存在某个字段
    bool IsExistInHeaders(const std::string &key) const
    {
        std::string normalized_key = Util::ToLower(Util::Trim(key));
        return _headers.find(normalized_key) == _headers.end() ? false : true;
    }
    // 查询参数中是否存在某个字段
    bool IsExistsInQueryParameter(const std::string &key)
    {
        return _query_parameters.find(key) == _query_parameters.end() ? false : true;
    }
    // 是否有连接选项
    bool HasConnectionOption(const std::string &target) const
    {
        std::string connection = GetHeadersValueByKey("connection");
        if (connection.empty())
            return false;
        std::vector<std::string> options = Util::SplitByChar(connection, ',');
        std::string normalized_target = Util::ToLower(Util::Trim(target));
        for (auto &option : options)
        {
            std::string normalized_option = Util::ToLower(Util::Trim(option));
            if (normalized_option == normalized_target)
                return true;
        }
        return false;
    }
    // 是否是长连接
    bool IsPersistentConnection()
    {
        std::string version = GetVersion();
        bool has_close = HasConnectionOption("close");
        bool has_keep_alive = HasConnectionOption("keep-alive");
        if (version == "1.1")
        {
            // 1.1 默认长连接，除非显示 Connection: close
            return !has_close;
        }
        if (version == "1.0")
        {
            // 1.0 默认短链接，除非显示 Connection: keep-alive
            return has_keep_alive;
        }
        return false;
    }
    // 重置
    void Reset()
    {
        _method.clear();
        _path.clear();
        _version.clear();
        _body.clear();
        _headers.clear();
        _query_parameters.clear();
        std::smatch matcher;
        _matcher.swap(matcher);
    }

private:
    std::string _method;                                            // 请求方法
    std::string _path;                                              // 请求路径
    std::string _version;                                           // HTTP 版本
    std::unordered_map<std::string, std::string> _headers;          // 请求头部
    std::unordered_map<std::string, std::string> _query_parameters; // 查询参数
    std::string _body;                                              // 请求正文
    std::smatch _matcher;                                           // 正则匹配器
};

/// @brief HTTP 响应模块
class Response
{
public:
    Response()
        : _status_code(200),
          _status_code_description("OK")
    {
    }
    Response(int status_code = 200)
        : _status_code(status_code)
    {
    }
    ~Response() {}
    // 设置状态码
    void SetStatusCode(int status_code)
    {
        _status_code = status_code;
    }
    // 设置状态码说明
    void SetStatusCodeDescription(const std::string &status_code_description)
    {
        _status_code_description = status_code_description;
    }
    // 设置响应头部
    void SetHeaders(const std::string &key, const std::string &value)
    {
        std::string normalized_key = Util::ToLower(Util::Trim(key));
        std::string normalized_value = Util::Trim(value);
        _headers[normalized_key] = normalized_value;
    }
    // 设置响应正文
    void SetBody(const std::string &body)
    {
        _body = body;
    }
    // 获取状态码
    int GetStatusCode()
    {
        return _status_code;
    }
    // 获取状态码说明
    std::string GetStatusCodeDescription()
    {
        return _status_code_description;
    }
    // 获取响应头部
    std::string GetHeadersValueByKey(const std::string &key)
    {
        std::string normalized_key = Util::ToLower(Util::Trim(key));
        auto it = _headers.find(normalized_key);
        if (it == _headers.end())
        {
            return "";
        }
        return it->second;
    }
    // 获取响应正文
    std::string GetBody()
    {
        return _body;
    }
    // 获取响应正文原始指针
    std::string *GetBodyPointer()
    {
        return &_body;
    }
    std::unordered_map<std::string, std::string> GetHeaders()
    {
        return _headers;
    }
    // 头部中是否存在某个字段
    bool IsExistInHeaders(const std::string &key)
    {
        std::string normalized_key = Util::ToLower(Util::Trim(key));
        return _headers.find(normalized_key) == _headers.end() ? false : true;
    }
    // 是否是长连接
    bool IsPersistentConnection()
    {
        std::string persistent = GetHeadersValueByKey("Connection");
        if (IsExistInHeaders("Connection") == true && GetHeadersValueByKey("Connection") == "keep-alive")
            return true;
        return false;
    }
    // 重置
    void Reset()
    {
        _status_code = 200;
        _status_code_description.clear();
        _headers.clear();
        _body.clear();
    }

private:
    int _status_code;                                      // 响应状态码
    std::string _status_code_description;                  // 响应状态码说明
    std::unordered_map<std::string, std::string> _headers; // 响应头部
    std::string _body;                                     // 响应正文
};

/// @brief HTTP 上下文模块

// 接收状态---标识处理到哪个地方
enum Receive_Status
{
    RECEIVE_ERROR,        // 处理错误
    RECEIVE_REQUEST_LINE, // 处理请求行
    RECEIVE_HEADER,       // 处理请求头部
    RECEIVE_BODY,         // 处理请求正文
    RECEIVE_OVER          // 处理完毕
};

// 解析状态---将非法请求和不完整请求解耦
enum Parse_Status
{
    PARSE_OK,    // 解析完毕---合法且完整请求
    PARSE_AGAIN, // 再次解析---不完整请求
    PARSE_ERROR  // 解析错误---非法请求
};

enum Route_Results
{
    SYNC_DONE,
    ASYNC_SUBMITTED
};

class Context
{
private:
#define MAX_PROCESS_SIZE 8192
public:
    Context()
        : _processing_status(Receive_Status::RECEIVE_REQUEST_LINE),
          _response_status_code(200)
    {
    }
    ~Context() {}
    // 解析并处理请求
    Parse_Status Parse(Buffer *buffer)
    {
        Parse_Status ret = Parse_Status::PARSE_AGAIN;
        // 依据当前处理状态进行分流
        if (_processing_status == RECEIVE_REQUEST_LINE && _processing_status != Receive_Status::RECEIVE_ERROR)
        {
            ret = RecieveRequestLine(buffer);
            if (ret != Parse_Status::PARSE_OK)
                return ret;
        }
        if (_processing_status == RECEIVE_HEADER && _processing_status != Receive_Status::RECEIVE_ERROR)
        {
            ret = RecieveHeader(buffer);
            if (ret != Parse_Status::PARSE_OK)
                return ret;
        }
        if (_processing_status == RECEIVE_BODY && _processing_status != Receive_Status::RECEIVE_ERROR)
        {
            ret = RecieveBody(buffer);
            if (ret != Parse_Status::PARSE_OK)
                return ret;
        }
        if (_processing_status == RECEIVE_OVER && _processing_status != Receive_Status::RECEIVE_ERROR)
            return Parse_Status::PARSE_OK;
        return Parse_Status::PARSE_ERROR;
    }
    // 解析请求行
    bool ParseRequestLine(const std::string &request_line)
    {
        if (_processing_status != Receive_Status::RECEIVE_REQUEST_LINE)
            return false;
        // 利用正则表达式进行匹配
        // std::regex pattern(R"(^([A-Z]+)\s+(([^?\s]+)(?:\?([^\s]*))?)\s+HTTP/(\d+\.\d+)\r?\n?$)");
        // 宽松版本---便于支持测试工具和手写请求
        std::regex pattern(
            R"(^([!#$%&'*+\-.^_`|~0-9A-Za-z]+)\s+(\S+)\s+([Hh][Tt][Tt][Pp])/(\d+\.\d+)\r?\n?$)");
        // results[0]GET /api/user/info?id=123&name=gemini HTTP/1.1
        // results[1]GET
        // results[2]/api/user/info?id=123&name=gemini
        // results[3]/api/user/info
        // results[4]id=123&name=gemini
        // results[5]1.1
        std::smatch results;
        bool r = std::regex_match(request_line, results, pattern);
        if (!r)
        {

            return false;
        }
        // results[1] = method
        // results[2] = request-target, e.g. /search?a=1&b=2
        // results[3] = HTTP
        // results[4] = version

        std::string method = Util::ToUpper(results[1].str());
        std::string target = results[2].str();
        std::string version = results[4].str();

        _request.SetMethod(method);
        _request.SetVersion(version);

        // 只支持 origin-form: /path?query
        // 暂时不支持 absolute-form: http://host/path
        if (target.empty() || target[0] != '/')
            return false;
        std::string raw_path;
        std::string raw_query;

        size_t question_pos = target.find('?');
        if (question_pos == std::string::npos)
        {
            raw_path = target;
        }
        else
        {
            raw_path = target.substr(0, question_pos);
            raw_query = target.substr(question_pos + 1);
        }

        if (raw_path.empty())
            raw_path = "/";

        // 解码 path
        std::string decode_path;
        Util::URLDecode(raw_path, &decode_path, false);
        _request.SetPath(decode_path);
        // 解析 query
        if (!raw_query.empty())
        {
            std::vector<std::string> parameters;
            Util::Split("&", raw_query, &parameters);

            for (const auto &param : parameters)
            {
                if (param.empty())
                    continue;

                size_t equal_pos = param.find('=');

                std::string raw_key;
                std::string raw_value;

                if (equal_pos == std::string::npos)
                {
                    raw_key = param;
                    raw_value = "";
                }
                else
                {
                    raw_key = param.substr(0, equal_pos);
                    raw_value = param.substr(equal_pos + 1);
                }

                std::string decode_key;
                std::string decode_value;

                Util::URLDecode(raw_key, &decode_key, true);
                Util::URLDecode(raw_value, &decode_value, true);

                if (!decode_key.empty())
                    _request.SetQueryParameter(decode_key, decode_value);
            }
        }
        return true;

        // // 1. 设置请求方法
        // if (results.size() >= 1)
        //     _request.SetMethod(results[1]);
        // // 2. 设置请求路径
        // if (results.size() >= 3)
        // {
        //     std::string raw_path = results[3];
        //     std::string decode_path;
        //     Util::URLDecode(raw_path, &decode_path, false);
        //     _request.SetPath(decode_path);
        // }
        // // 3. 设置查询参数
        // if (results.size() >= 5 && results[4].length() > 0)
        // {
        //     std::vector<std::string> parameters;
        //     // a) 先按照 & 分割到一个或多个 key=value
        //     Util::Split("&", results[4], &parameters);
        //     for (int i = 0; i < parameters.size(); i++)
        //     {
        //         std::vector<std::string> sub_parameters;
        //         // b) 再按照 = 分割 key 和 value 进行解码
        //         Util::Split("=", parameters[i], &sub_parameters);
        //         std::string raw_key = sub_parameters[0];
        //         std::string raw_value = sub_parameters[1];
        //         std::string decode_key;
        //         std::string decode_value;
        //         Util::URLDecode(raw_key, &decode_key, false);
        //         Util::URLDecode(raw_value, &decode_value, false);
        //         _request.SetQueryParameter(decode_key, decode_value);
        //     }
        // }
        // // 4. 设置 HTTP 版本
        // if (results.size() >= 5)
        //     _request.SetVersion(results[5]);
        // return true;
    }
    // 解析请求头部
    bool ParseHeadersLine(const std::string &headers_line)
    {
        /*
        合法请求可以是：
        Host:127.0.0.1
        Host: 127.0.0.1
        Host:    127.0.0.1
        connection:keep-alive
        */
        if (_processing_status != Receive_Status::RECEIVE_HEADER)
            return false;
        std::string line = headers_line;
        // 去除行尾 CRLF
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        size_t colon_pos = line.find(":");
        if (colon_pos == std::string::npos)
            return false;
        std::string key = Util::Trim(line.substr(0, colon_pos));
        std::string value = Util::Trim(line.substr(colon_pos + 1));
        if (key.empty())
            return false;
        _request.SetHeaders(key, value);
        return true;
    }
    // 接收请求行
    Parse_Status RecieveRequestLine(Buffer *buffer)
    {
        if (_processing_status != Receive_Status::RECEIVE_REQUEST_LINE)
            return Parse_Status::PARSE_AGAIN;
        // 1. 获取请求行数据
        std::string request_line = buffer->ReadLine();
        // LOG_DEBUG("RecieveRequestLine: %s\n", request_line.c_str());
        // 2. 请求行数据合法性检验
        if (request_line == "")
        {
            // a) 数据大小过大
            if (buffer->GetReadableSize() > MAX_PROCESS_SIZE)
            {
                _processing_status = Receive_Status::RECEIVE_ERROR;
                _response_status_code = 414;
                return Parse_Status::PARSE_ERROR;
            }
            // b) 数据大小正常，只是还未读取完整
            return Parse_Status::PARSE_AGAIN;
        }
        // 3. 请求行数据过大
        else if (request_line.size() > MAX_PROCESS_SIZE)
        {
            _processing_status = Receive_Status::RECEIVE_ERROR;
            _response_status_code = 414;
            return Parse_Status::PARSE_ERROR;
        }
        // 4. 请求数据大小正常且完整---解析接口应该只负责解析，不进行跳转或者状态标志位修改
        bool ret = ParseRequestLine(request_line);
        // a) 解析失败---已经将非法请求和不完整请求做解耦，出错了直接终止
        if (ret == false)
        {
            _response_status_code = 400;
            _processing_status = Receive_Status::RECEIVE_OVER;
            return Parse_Status::PARSE_ERROR; // BUG---如果解析出错的话，那么本来正确格式的未被处理的请求行就已经 MoveReadOffset 了
        }
        // b) 解析成功
        // 5. 修改状态标志位---由外层统一的接口进行处理的跳转
        _processing_status = Receive_Status::RECEIVE_HEADER;
        return Parse_Status::PARSE_OK;
    }
    // 接收请求头部
    Parse_Status RecieveHeader(Buffer *buffer)
    {
        if (_processing_status != Receive_Status::RECEIVE_HEADER)
            return Parse_Status::PARSE_ERROR;
        // 循环读取请求头部数据
        while (1)
        {
            // 1. 逐行读取
            std::string headers_line = buffer->ReadLine();
            // LOG_DEBUG("RecieveHeader: %s\n", headers_line.c_str());
            // 2. 行数据合法性检验
            if (headers_line == "")
            {
                // a) 行数据大小过大
                if (buffer->GetReadableSize() > MAX_PROCESS_SIZE)
                {
                    _processing_status = Receive_Status::RECEIVE_ERROR;
                    _response_status_code = 414;
                    return Parse_Status::PARSE_ERROR;
                }
                // b) 行数据大小正常，只是还未读取完整
                return Parse_Status::PARSE_AGAIN;
            }
            // 3. 行数据过大
            else if (headers_line.size() > MAX_PROCESS_SIZE)
            {
                _processing_status = Receive_Status::RECEIVE_ERROR;
                _response_status_code = 414;
                return Parse_Status::PARSE_ERROR;
            }
            // 4. 读取到空行就结束
            if (headers_line == "\r\n" || headers_line == "\r")
            {
                break;
            }
            // 5. 解析请求头部数据中的一行
            bool ret = ParseHeadersLine(headers_line);
            if (ret == false)
            {
                _response_status_code = 400;
                _processing_status = Receive_Status::RECEIVE_ERROR;
                return Parse_Status::PARSE_ERROR; // BUG---如果解析出错的话，那么本来正确格式的未被处理的请求头部行数据就已经 MoveReadOffset 了
            }
        }
        _processing_status = Receive_Status::RECEIVE_BODY;
        return Parse_Status::PARSE_OK;
    }
    // 接收请求正文
    Parse_Status RecieveBody(Buffer *buffer)
    {
        if (_processing_status != Receive_Status::RECEIVE_BODY)
            return Parse_Status::PARSE_ERROR;
        // 1. 获取到请求正文数据大小
        size_t body_size = _request.GetContextLength();
        if (body_size == 0)
        {
            _processing_status = Receive_Status::RECEIVE_OVER;
            _response_status_code = 200;
            return Parse_Status::PARSE_OK;
        }
        // 2. 真正需要读取的请求正文数据大小 = 请求报头中的正文数据大小 - 请求中的正文大小
        size_t real_size = body_size - _request.GetBody().size();
        // LOG_DEBUG("Real Size: %ld", real_size);
        // LOG_DEBUG("Buffer Size: %d", buffer->GetReadableSize());
        // 3. 检查缓冲区中数据大小
        // a) 缓冲区中的数据不足正文长度
        if (buffer->GetReadableSize() < real_size)
        {
            // BUG---real_size???
            _request.AppendBody(buffer->Read(real_size));
            return Parse_Status::PARSE_AGAIN;
        }
        // b) 缓冲区中的数据满足正文长度
        // 头部直接原模原样读取到请求中，不用解析
        _request.AppendBody(buffer->Read(real_size));
        _processing_status = Receive_Status::RECEIVE_OVER;
        _response_status_code = 200;
        return Parse_Status::PARSE_OK;
    }
    Receive_Status GetProcessingStatus()
    {
        return _processing_status;
    }
    int GetStatusCode()
    {
        return _response_status_code;
    }
    Request *GetRequest()
    {
        return &_request;
    }
    void Reset()
    {
        _request.Reset();
        _processing_status = Receive_Status::RECEIVE_REQUEST_LINE;
        _response_status_code = 200;
    }

private:
    Request _request;                  // HTTP 请求
    Receive_Status _processing_status; // 当前处理状态
    int _response_status_code;         // 响应状态码
};

/// @brief 业务线程池模块---执行耗时的业务
class WorkerThreadPool
{
private:
    // 任务类型
    using Task = std::function<void()>;
    // struct WorkerTask
    // {
    //     std::function<void()> _callback;
    //     Connection::ConnectionShared _connection;
    // };

#define DEFAULT_WORKERTHREADPOOL_COUNT 3

    // 业务线程主循环
    void WorkerLoop()
    {
        while (true)
        {
            Task task;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                // 被取消了或者任务队列不为空就一直阻塞
                _condition.wait(lock, [this]
                                { return _is_stop || !_tasks.empty(); });
                if (_is_stop && _tasks.empty())
                    return;
                // 没有被取消并且任务队列不为空
                task = std::move(_tasks.front());
                _tasks.pop_front();
            }
            // LOG_DEBUG("Worker thread get task");
            // 执行任务不需要加锁
            if (task)
                task();
        }
    }

public:
    WorkerThreadPool(int count = DEFAULT_WORKERTHREADPOOL_COUNT)
        : _thread_count(count),
          _is_stop(false)
    {
    }
    // 如何启动?添加一个接口给上层 TcpServer 启动?
    // 那么什么时候上层才调用呢?插入任务的时候，不对。如果说多个业务任务被插入，多并发的时候一个线程被切换了之后，再切回来的时候就无法执行了

    // 创建线程
    void Start()
    {
        for (int i = 0; i < _thread_count; i++)
        {
            _threads.emplace_back(&WorkerThreadPool::WorkerLoop, this);
        }
    }
    // 提交任务
    void Submit(const Task &worker_task)
    {
        // LOG_DEBUG("Submit worker task, queue size before push: %ld", _tasks.size());
        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_is_stop)
                return;
            _tasks.push_back(worker_task);
        }
        _condition.notify_one();
    }
    void Stop()
    {
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _is_stop = true;
        }
        _condition.notify_all();
        for (auto &it : _threads)
        {
            if (it.joinable())
                it.join();
        }
    }
    ~WorkerThreadPool() { Stop(); }

private:
    bool _is_stop;                      // 是否停止标志位
    int _thread_count;                  // 线程个数
    std::vector<std::thread> _threads;  // 线程
    std::deque<Task> _tasks;            // 任务---业务回调
    std::mutex _mutex;                  // 互斥锁
    std::condition_variable _condition; // 条件变量
};

/// @brief HTTP 服务器模块
class HttpServer
{
private:
    using Handler = std::function<void(Request *request, Response *response)>;

private:
    // 构建一个静态资源的真实文件路径
    std::string MakeRealFilePath(Request *request)
    {
        std::string url_path = request->GetPath();
        // a) 有一类特殊请求，请求 / 一个目录，如果是请求一个目录，那么默认返回的目录下的 index.html
        if (url_path == "/")
            url_path = "/index.html";
        std::string real_path = _static_resource_path + url_path;
        // b) 请求本来就是一个目录
        if (Util::IsDirectory(real_path))
        {
            if (real_path.back() != '/')
                real_path += '/';
            real_path += "index.html";
        }
        return real_path;
    }
    // 是否是静态资源请求
    bool EnsureStaticResourceRequest(Request *request)
    {
        // 必须是 Get 或者 Head 方法
        if (request->GetMethod() == "GET" || request->GetMethod() == "HEAD")
        {
            return true;
        }
        return false;
    }
    // 静态资源请求
    void HandleStaticResourceRequest(Request *request, Response *response)
    {
        std::string real_path = MakeRealFilePath(request);
        // 1. 确保是一个静态资源请求---但是不一定是合法的请求
        if (!EnsureStaticResourceRequest(request))
        {
            return;
        }
        // a) 是否越级访问目录
        if (!Util::IsValidPath(request->GetPath()))
        {
            response->SetStatusCode(403);
            return;
        }
        // b) 是否是一个文件
        if (!Util::IsRegularFile(real_path))
        {
            response->SetStatusCode(404);
            return;
        }
        // 2. 设置响应头部
        response->SetHeaders("Content-Type", Util::GetMimeFromFileName(real_path));
        // 3. 读取文件到响应正文中
        if (!Util::ReadFromFile(real_path, response->GetBodyPointer()))
            response->SetStatusCode(500);
        else
            response->SetStatusCode(200);
        // 4. 设置正文长度
        response->SetHeaders("Content-Length", std::to_string(response->GetBody().size()));
        return;
    }
    // 派发路由表
    Handler DispatcherFunctionRequest(std::unordered_map<std::string, Handler> route_table, Request *request)
    {
        for (auto it : route_table)
        {
            // TODO---正则匹配
            if (request->GetPath() == it.first)
                return it.second;
        }
        return nullptr;
    }
    // 是否是功能性请求
    bool EnsureFunctionalRequest(Request *request, Handler *handler)
    {
        // 路由查找
        std::string request_method = request->GetMethod();
        if (request_method == "GET")
            *handler = DispatcherFunctionRequest(_get_route_table, request);
        if (request_method == "HEAD")
            *handler = DispatcherFunctionRequest(_head_route_table, request);
        if (request_method == "OPTIONS")
            *handler = DispatcherFunctionRequest(_options_route_table, request);
        if (request_method == "TRACE")
            *handler = DispatcherFunctionRequest(_trace_route_table, request);
        if (request_method == "PUT")
            *handler = DispatcherFunctionRequest(_put_route_table, request);
        if (request_method == "DELETE")
            *handler = DispatcherFunctionRequest(_delete_route_table, request);
        if (request_method == "POST")
            *handler = DispatcherFunctionRequest(_post_route_table, request);
        if (request_method == "PATCH")
            *handler = DispatcherFunctionRequest(_patch_route_table, request);
        if (request_method == "CONNECT")
            *handler = DispatcherFunctionRequest(_connect_route_table, request);
        return (*handler) ? true : false;
    }
    // 功能性请求
    void HandleFunctionalRequest(Connection::ConnectionShared connection, Request *request, Response *response, const Handler &handler)
    {
        // 执行回调---不直接在 EventLoop 中执行，而是将业务处理作为一个任务放到业务线程池中执行
        // 不能直接使用原生 request 和 response，会导致后续的 EventLoop 进行操作后导致其内容改变
        Request copy_request = *request;
        Response copy_response = *response;
        uint32_t timeout = _http_server.GetTimeOut();
        // 0) 暂停 connection 的非活跃链接释放操作
        connection.get()->DisableInactiveRelease();
        connection.get()->SetBusy(true);
        _worker_thread_pool.Submit(
            [this, connection, copy_request, copy_response, handler, timeout]() mutable
            {
                // a) 执行业务回调
                if (handler)
                    handler(&copy_request, &copy_response);
                // b) 构建响应并发送
                BuildAndSendResponse(connection, &copy_request, &copy_response);
                connection.get()->SetBusy(false);
                // c) 是否要断开连接
                if (!copy_request.IsPersistentConnection())
                {
                    connection.get()->Shutdown();
                    return;
                }
                else
                {
                    if (timeout > 0)
                        connection.get()->EnableInactiveRelease(timeout);
                }
            });
    }
    // 构建并发送一个 HTTP 响应
    void BuildAndSendResponse(Connection::ConnectionShared connection, Request *request, Response *response)
    {
        // 1. 检查响应是否完整
        // TODO---Location: /search?q=C%2B%2B 需要编码
        // a) 长短连接---取决于 Request
        // LOG_DEBUG("IsPersistentConnection: %d", request->IsPersistentConnection());
        if (request->IsPersistentConnection())
        {
            // LOG_DEBUG("Set keep-alive");
            response->SetHeaders("Connection", "keep-alive");
        }
        else
        {
            // LOG_DEBUG("Set close");
            response->SetHeaders("Connection", "close");
        }
        // b) 正文类型---取决于 Response
        if (response->GetHeadersValueByKey("Content-Type").empty())
            response->SetHeaders("Content-Type", "application/octet-stream");
        // c) 正文长度
        if (response->GetHeadersValueByKey("Content-Length").empty())
            response->SetHeaders("Content-Length", std::to_string(response->GetBody().size()));
        // d) 正文内容
        // 没有响应正文
        // 2. 按照 HTTP 响应格式组织数据
        std::stringstream response_stream;
        // a) 响应行---版本要特殊处理，非法请求中可能为空
        std::string version = request->GetVersion().empty() ? "1.1" : request->GetVersion();
        response_stream << "HTTP/" << version << " " << response->GetStatusCode() << " " << Util::GetDescriptionFromStatus(response->GetStatusCode()) << "\r\n";
        // b) 响应头部
        for (auto it : response->GetHeaders())
        {
            response_stream << Util::CanonicalHeaderName(it.first) << ": " << it.second << "\r\n"; // TODO---: 是否需要放入到原始头部字段中?
        }
        // c) 空行
        response_stream << "\r\n";
        // d) 响应正文
        bool is_head = (request->GetMethod() == "HEAD");
        if (!is_head)
            response_stream << response->GetBody(); // TODO---是否有必要添加换行---不要添加，避免破坏源文件格式
        // 3. 发送响应
        std::string resp = response_stream.str();
        // LOG_DEBUG("Build response to %p:\n %s", connection.get(), resp.c_str());
        connection.get()->Send(resp.c_str(), resp.size());
    }
    // 路由分流
    Route_Results Route(Connection::ConnectionShared connection, Request *request, Response *response)
    {
        Handler handler = nullptr;
        if (EnsureFunctionalRequest(request, &handler))
        {
            // /search 这种极快业务，直接在 EventLoop 中同步执行---仅测试QPS
            if (request->GetMethod() == "GET" && request->GetPath() == "/search")
            {
                if (handler)
                    handler(request, response);

                return Route_Results::SYNC_DONE;
            }
            // 1. 功能性请求路由
            // LOG_DEBUG("HandleFunctionalRequest");
            HandleFunctionalRequest(connection, request, response, handler);
            return Route_Results::ASYNC_SUBMITTED;
        }
        else if (EnsureStaticResourceRequest(request))
        {
            // LOG_DEBUG("HandleStaticResourceRequest");
            // 2. 静态资源请求路由
            HandleStaticResourceRequest(request, response);
            return Route_Results::SYNC_DONE;
        }
        // 3. 未知请求路由---Method Not Allowed
        response->SetStatusCode(405);
        return Route_Results::SYNC_DONE;
    }
    // 状态码错误类型页面分流
    std::string RouteErrorPageByStatusCode(int status_code)
    {
        // TODO---暂时统一返回一个错误页面
        return "www/error.html";
    }
    // 错误处理
    void HandleError(Response *response)
    {
        // 1. 设置状态码描述信息
        response->SetStatusCodeDescription(Util::GetDescriptionFromStatus(response->GetStatusCode()));
        // 2. 依据状态码进行错误页面分流
        std::string page_path = RouteErrorPageByStatusCode(response->GetStatusCode());
        // 3. 设置到响应头部并读取到正文中
        if (Util::IsValidPath(page_path))
        {
            response->SetHeaders("Content-Type", Util::GetMimeFromFileName(page_path));
            Util::ReadFromFile(page_path, response->GetBodyPointer());
            response->SetHeaders("Content-Length", std::to_string(response->GetBody().size()));
        }
    }
    // 设置收到数据后的回调
    void HandleRecieveRequest(Connection::ConnectionShared connection, Buffer *buffer)
    {
        // LOG_INFO("HandleRecieveRequest, readable=%d", buffer->GetReadableSize());
        while (buffer->GetReadableSize() > 0)
        {
            // 1. 读取请求
            // if (buffer->GetReadableSize() > 0)
            //     LOG_DEBUG("Raw Buffer: \n%s", buffer->GetReadPosition());
            // a) 获取到上下文
            Context *context = AnyCast<Context>(*connection.get()->GetContext());
            // b) 解析请求
            Parse_Status parse_ret = context->Parse(buffer);
            if (buffer->GetReadableSize() > 0)
                // LOG_DEBUG("After Parse Buffer: \n%s", buffer->GetReadPosition());
                // LOG_INFO("parse ret=%d, status=%d, method=%s, path=%s",
                //          p,
                //          context->GetProcessingStatus(),
                //          context->GetRequest()->GetMethod().c_str(),
                //          context->GetRequest()->GetPath().c_str());
                // c) 检查是否成功解析---没有解析成功可能是当前数据不完整 -> 直接返回，等到下一次处理

                // 数据不完整
                if (parse_ret == Parse_Status::PARSE_AGAIN)
                    return;
            // 非法请求
            if (parse_ret == Parse_Status::PARSE_ERROR)
            {
                LOG_ERROR("Bad Request");
                Response response(context->GetStatusCode());
                HandleError(&response);
                BuildAndSendResponse(connection, context->GetRequest(), &response);
                connection.get()->Shutdown();
                context->Reset();
                return;
            }

            // if (!p || context->GetProcessingStatus() != Receive_Status::RECEIVE_OVER)
            // {
            //     // LOG_DEBUG("Parse not finished");
            //     // LOG_DEBUG("Request Body: \n%s", context->GetRequest()->GetBody().c_str());
            //     return;
            // }
            // LOG_DEBUG("Parse finished");
            // d) 初始化应答和提取出一个完整的请求
            Request *request = context->GetRequest();
            Response response(context->GetStatusCode());
            // response.SetStatusCode(context->GetStatusCode());
            // e) 得到一个完整的请求---未必是一个正确的请求
            // if (context->GetStatusCode() >= 400)
            // {
            //     LOG_DEBUG("Error Request");
            //     // 错误请求
            //     HandleError(&response);
            //     BuildAndSendResponse(connection, request, &response);
            //     connection.get()->Shutdown();
            //     context->Reset();
            //     return;
            // }
            LOG_INFO("Correct Request");
            std::string debug_request = request->GetMethod() + " " + request->GetPath() + " /HTTP" + request->GetVersion() + "\r\n";
            std::unordered_map<std::string, std::string> debug_request_headers = request->GetHeaders();
            for (auto &it : debug_request_headers)
            {
                debug_request += it.first;
                debug_request += ": ";
                debug_request += it.second;
                debug_request += "\n";
            }
            std::unordered_map<std::string, std::string> debug_request_query_parameter = request->GetQueryParameter();
            for (auto &it : debug_request_query_parameter)
            {
                debug_request += it.first;
                debug_request += ": ";
                debug_request += it.second;
                debug_request += "\n";
            }
            debug_request += (request->GetBody());
            // LOG_DEBUG("GetHeadersValueByKey(Connection): %s\n", request->GetHeadersValueByKey("Connection").c_str());
            // LOG_DEBUG("IsPersistentConnection: %d\n", request->IsPersistentConnection());
            // LOG_DEBUG("Receive from %p: \n", connection.get());
            // std::cout << debug_request << std::endl;
            // f) 得到一个完整且正确的请求
            // 2. 路由分流
            // LOG_DEBUG("Enter Route module");
            Route_Results route_ret = Route(connection, request, &response);
            if (route_ret == Route_Results::ASYNC_SUBMITTED)
            {
                // 不用担心会影响到业务线程，因为其处理的拷贝副本
                context->Reset();
                return;
            }
            // 3. 构建响应并发送
            // LOG_DEBUG("Enter BuildAndSendResponse module");
            BuildAndSendResponse(connection, request, &response);
            // 4. 是否要断开连接
            if (!request->IsPersistentConnection())
            {
                // 短连接就直接关闭连接
                // LOG_DEBUG("Shutdown connection");
                connection.get()->Shutdown();
                context->Reset();
                return;
            }
            // 5. 重置上下文
            // LOG_DEBUG("Reset");
            // LOG_DEBUG("buffer.size(): %d", buffer->GetReadableSize());
            // std::cout << buffer->GetReadPosition() << std::endl;
            context->Reset();
        }
    }
    void HandleConnected(Connection::ConnectionShared connection)
    {
        // 设置协议上下文---HTTP上下文
        connection.get()->SetContext(Context());
        LOG_INFO("%p connected", connection.get());
    }

public:
    HttpServer(uint32_t port)
        : _http_server(port),
          _worker_thread_pool(3)
    {
        _http_server.SetConnectedCallback(std::bind(&HttpServer::HandleConnected, this, std::placeholders::_1));
        _http_server.SetAfterReceiveCallback(std::bind(&HttpServer::HandleRecieveRequest, this, std::placeholders::_1, std::placeholders::_2));
        _http_server.EnableInactiveRelease(30);
    }
    // 设置 GET 请求路由
    void SetGetRouteTable(std::string service_name, Handler service_handler)
    {
        _get_route_table.insert(std::make_pair(service_name, service_handler));
    }
    // 设置 Head 请求路由
    void SetHeadRouteTable(std::string service_name, Handler service_handler)
    {
        _head_route_table.insert(std::make_pair(service_name, service_handler));
    }
    // 设置 Options 请求路由
    void SetOptionsRouteTable(std::string service_name, Handler service_handler)
    {
        _options_route_table.insert(std::make_pair(service_name, service_handler));
    }
    // 设置 Trace 请求路由
    void SetTraceRouteTable(std::string service_name, Handler service_handler)
    {
        _trace_route_table.insert(std::make_pair(service_name, service_handler));
    }
    // 设置 Put 请求路由
    void SetPutRouteTable(std::string service_name, Handler service_handler)
    {
        _put_route_table.insert(std::make_pair(service_name, service_handler));
    }
    // 设置 Delete 请求路由
    void SetDeleteRouteTable(std::string service_name, Handler service_handler)
    {
        _delete_route_table.insert(std::make_pair(service_name, service_handler));
    }
    // 设置 Post 请求路由
    void SetPostRouteTable(std::string service_name, Handler service_handler)
    {
        _post_route_table.insert(std::make_pair(service_name, service_handler));
    }
    // 设置 Patch 请求路由
    void SetPatchRouteTable(std::string service_name, Handler service_handler)
    {
        _patch_route_table.insert(std::make_pair(service_name, service_handler));
    }
    // 设置 Connect 请求路由
    void SetConnectRouteTable(std::string service_name, Handler service_handler)
    {
        _connect_route_table.insert(std::make_pair(service_name, service_handler));
    }
    // 设置静态资源根目录
    void SetStaticResourcePath(const std::string &path)
    {
        _static_resource_path = path;
    }
    // 设置线程池数量
    void SetThreadPollCount(int count)
    {
        _http_server.SetThreadPoolCount(count);
    }
    // 启动服务器
    void Start()
    {
        LOG_DEBUG("Worker thread start");
        _worker_thread_pool.Start();
        _http_server.Launch();
    }
    ~HttpServer() {}

private:
    TcpServer _http_server;                                        // HTTP 服务器
    WorkerThreadPool _worker_thread_pool;                          // 业务线程池
    std::string _static_resource_path;                             // 静态资源根目录
    std::unordered_map<std::string, Handler> _get_route_table;     // GET 请求路由表
    std::unordered_map<std::string, Handler> _head_route_table;    // Head 请求路由表
    std::unordered_map<std::string, Handler> _options_route_table; // Options 请求路由表
    std::unordered_map<std::string, Handler> _trace_route_table;   // Trace 请求路由表
    std::unordered_map<std::string, Handler> _put_route_table;     // Put 请求路由表
    std::unordered_map<std::string, Handler> _delete_route_table;  // Delete 请求路由表
    std::unordered_map<std::string, Handler> _post_route_table;    // Post 请求路由表
    std::unordered_map<std::string, Handler> _patch_route_table;   // Patch 请求路由表
    std::unordered_map<std::string, Handler> _connect_route_table; // Connect 请求路由表
};
