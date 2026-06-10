#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <cstring>
#include <time.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <functional>
#include <sys/epoll.h>
#include <unordered_map>
#include <thread>
#include <sys/eventfd.h>
#include <mutex>
#include <memory>
#include <sys/timerfd.h>
#include <typeinfo>
#include <condition_variable>
#include <atomic>

// [2026-05-16 21:20:32] [INFO] [Socket.hpp] [76] - socket file descriptor 3 create success
/// @brief 日志宏模块

// 日志模块注意事项
// __VA_ARGS__ 是 C99 引入的预处理关键字，代表宏定义中 ... 对应的所有参数
// ##__VA_ARGS__ 是 GCC 扩展，在可变参数为空时自动吃掉前面的逗号
// "[%s][%d]" msg "\n"            ---> msg 是 "x=%d"，展开后变成："[%s][%d]" "x=%d" "\n"
// 编译器合并成："[%s][%d]x=%d\n"  ---> 如果 msg 是运行时的 char* 变量，就没法在编译期拼接，会直接报错
// 宏---纯替换!!!
// localtime 会自动处理%Y从1900开始的问题和%m范围是[0-11]的问题

enum Log_Level
{
    LOG_LEVEL_DEBUG = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_ERROR = 4,
};

#define DEFAULT_FD -1

#define DEFAULT_LOG_LEVEL Log_Level::LOG_LEVEL_ERROR

#define LOG(level, msg, ...)                                                                                              \
    do                                                                                                                    \
    {                                                                                                                     \
        if (level < DEFAULT_LOG_LEVEL)                                                                                    \
            break;                                                                                                        \
        FILE *out = (level >= Log_Level::LOG_LEVEL_ERROR ? stderr : stdout);                                              \
        time_t base_time = time(NULL);                                                                                    \
        struct tm *current_time = localtime(&base_time);                                                                  \
        char buffer_time[32];                                                                                             \
        strftime(buffer_time, sizeof(buffer_time), "%Y-%m-%d %H:%M:%S", current_time);                                    \
        fprintf(out, "[%s][%p][%s:%d]" msg "\n", buffer_time, (void *)pthread_self(), __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define LOG_DEBUG(msg, ...) LOG(Log_Level::LOG_LEVEL_DEBUG, msg, ##__VA_ARGS__)
#define LOG_INFO(msg, ...) LOG(Log_Level::LOG_LEVEL_INFO, msg, ##__VA_ARGS__)
#define LOG_ERROR(msg, ...) LOG(Log_Level::LOG_LEVEL_ERROR, msg, ##__VA_ARGS__)

/// @brief 缓冲区模块--IO数据
#define DEFAULT_BUFFER_SIZE 1024

class Buffer
{
public:
    Buffer()
        : _read_offset(0),
          _write_offset(0),
          _buffer(DEFAULT_BUFFER_SIZE)
    {
    }
    ~Buffer()
    {
        _buffer.clear();
    }
    // 获取到第一个字符的地址
    char *Begin()
    {
        return &(*_buffer.begin());
    }
    // 获取当前起始读位置
    char *GetReadPosition()
    {
        return Begin() + _read_offset;
    }
    // 获取当前起始写位置
    char *GetWritePosition()
    {
        return Begin() + _write_offset;
    }
    // 获取读偏移量
    int GetReadOffset()
    {
        return _read_offset;
    }
    // 获取写偏移量
    int GetWriteOffset()
    {
        return _write_offset;
    }
    // 获取可读前的空闲空间
    int GetHeadFreeSize()
    {
        return _read_offset;
    }
    // 获取可写后的空闲空间
    int GetTailFreeSize()
    {
        return _buffer.size() - _write_offset;
    }
    // 获取可读数据大小
    int GetReadableSize()
    {
        return (_write_offset - _read_offset) < 0 ? 0 : (_write_offset - _read_offset);
    }
    // 获取可写数据大小
    int GetWriteableSize()
    {
        return GetHeadFreeSize() + GetTailFreeSize();
    }
    // 检查是否有足够的空间
    bool IsSpaceEnough(int len)
    {
        // 当前空闲空间 VS 目标空间
        return len <= GetHeadFreeSize() + GetTailFreeSize();
    }
    // 扩容空间
    void ExpansionSpace(int size)
    {
        _buffer.resize(size);
    }
    // 确保有足够的空间
    void EnsureSpaceEnough(int target_size)
    {
        // 1. 空间充裕->返回
        // 1) 尾部空间已经充裕
        if (target_size < GetTailFreeSize())
            return;
        // 2) 尾部空间+头部空间充裕
        if (target_size <= GetHeadFreeSize() + GetTailFreeSize())
        {
            // a. 保存可读数据大小---移动数据后导致读写偏移量一个是新值，一个是旧值，出现问题
            int read_size = GetReadableSize();
            // b. 移动数据
            std::copy(GetReadPosition(), GetWritePosition(), Begin());
            // c. 更新偏移量
            _read_offset = 0;
            _write_offset = read_size;
            return;
        }
        // 2. 空间不足->扩容
        else
            ExpansionSpace(_write_offset + target_size);
    }
    // 向后移动读偏移量
    void MoveReadOffset(int offset)
    {
        // 偏移量 > 可读数据大小->移动到写偏移量的位置
        _read_offset += (offset > GetReadableSize() ? GetReadableSize() : offset);
    }
    // 向后移动写偏移量
    void MoveWriteOffset(int offset)
    {
        _write_offset += (offset > GetWriteableSize() ? GetWriteableSize() : offset);
    }
    // 读
    void Read(void *buffer, int len)
    {
        // 读取数据
        int read_len = len <= GetReadableSize() ? len : GetReadableSize();
        if (read_len <= 0)
        {
            return;
        }
        std::copy(GetReadPosition(), GetReadPosition() + read_len, (char *)buffer);
        // 移动偏移量
        MoveReadOffset(read_len);
    }
    const std::string Read(int len)
    {
        // BUG---len 并不一定是真实能读取的字节数
        int real_len = std::min(len, GetReadableSize());
        if (real_len <= 0)
        {
            return "";
        }
        if (GetReadableSize() <= 0)
        {
            return "";
        }
        // 不要多读一个字符，可能会导致处理字符串失败
        std::string str(real_len, '\0');
        Read(&(*str.begin()), real_len);
        return str;
    }
    // 找特定符号的位置
    char *FindSymbols(const std::string &symbols)
    {
        return std::search(GetReadPosition(), GetReadPosition() + GetReadableSize(), symbols.begin(), symbols.end());
    }
    // 获取到行结束标识符
    char *FindCRLF()
    {
        return (char *)memchr(&_buffer[_read_offset], '\n', GetReadableSize());
    }
    // 读取一行
    std::string ReadLine()
    {
        // char *pos = FindSymbols("\r\n");
        char *pos = FindCRLF();
        if (pos == nullptr)
        {
            return "";
        }
        // 注意将\n也读走
        return Read(pos - GetReadPosition() + 1);
    }
    // 写
    void Write(const void *buffer, int len)
    {
        // 1. 确保空间足够---len肯定可以写入
        EnsureSpaceEnough(len);
        const char *buffer_char = static_cast<const char *>(buffer);
        // 2. 写入数据
        std::copy(buffer_char, buffer_char + len, _buffer.begin() + _write_offset);
        // 3. 移动偏移量
        MoveWriteOffset(len);
    }
    void Write(const std::string &buffer)
    {
        if (GetWriteableSize() <= 0)
        {
            return;
        }
        Write(buffer.c_str(), buffer.size());
    }
    // 清除空间
    void Reset()
    {
        _read_offset = 0;
        _write_offset = 0;
    }

private:
    std::vector<char> _buffer;
    int _read_offset;
    int _write_offset;
};

/// @brief 套接字模块

// #define

enum Exit_Status
{
    OK = 0,
    SOCKET_ERROR = 1,
    BIND_ERROR = 2,
    LISTEN_ERROR = 3,
    CONNECT_ERROR = 4,
    FORK_ERROR = 5,
    USAGE_ERROR = 6,
    OPEN_ERROR = 7,
    DAEMON_ERROR = 8,
    EPOLL_CREATE_ERROR = 9,
    EPOLL_CTL_ERROR = 10,
};

class Socket
{
private:
    int default_backlog = 1024;

public:
    Socket() : _socket_fd(DEFAULT_FD) {}
    Socket(int socket_fd) : _socket_fd(socket_fd) {}
    ~Socket()
    {
        if (_socket_fd >= 0)
        {
            // a) 关闭连接
            Close();
            // b) 重置文件描述符
            _socket_fd = -1;
        }
    }
    // 创建socket
    bool SocketOrDie()
    {
        _socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (_socket_fd < 0)
        {
            LOG_ERROR("socket error: %s", strerror(errno));
            return false;
        }
        return true;
    }
    // 绑定port(按需绑定)
    bool BindOrDie(std::string ip, uint16_t port)
    {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        int n = inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
        if (n != 1)
        {
            LOG_ERROR("inet_pton error: %s", strerror(errno));
            return false;
        }
        socklen_t addr_len = sizeof(addr);
        int b = bind(_socket_fd, (struct sockaddr *)&addr, addr_len);
        if (b < 0)
        {
            LOG_ERROR("bind error: %s", strerror(errno));
            return false;
        }
        return true;
    }
    // 监听socket
    bool ListenOrDie()
    {
        int l = listen(_socket_fd, default_backlog);
        if (l < 0)
        {
            LOG_ERROR("listen error: %s", strerror(errno));
            return false;
        }
        return true;
    }
    // 获取用于通信的socket
    int Accept()
    {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        socklen_t addr_len = sizeof(addr);
        int fd = accept(_socket_fd, (struct sockaddr *)&addr, &addr_len);
        if (fd < 0)
        {
            // 所有连接都处理完了
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return -1;
            // 被信号中断
            else if (errno == EINTR)
                return -1;
            // 真的出错了
            else
            {
                LOG_ERROR("accept erro: %s", strerror(errno));
                return -1;
            }
        }
        uint16_t port = ntohs(addr.sin_port);
        char ip[64];
        memset(ip, 0, sizeof(ip));
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
        LOG_INFO("||%lu|| accept <%s:%u> successful", pthread_self(), ip, port);
        return fd;
    }
    // client获取socket建立通信
    bool Connect(const std::string server_ip, u_int16_t server_port)
    {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(server_port);
        int n = inet_pton(AF_INET, server_ip.c_str(), &addr.sin_addr);
        if (n != 1)
        {
            LOG_ERROR("inet_pton error: %s", strerror(errno));
            return false;
        }
        socklen_t addr_len = sizeof(addr);
        int c = connect(_socket_fd, (struct sockaddr *)&addr, addr_len);
        if (c < 0)
        {
            LOG_ERROR("connect error: %s", strerror(errno));
            return false;
        }
        return true;
    }
    // 关闭文件描述符
    int Close()
    {
        if (_socket_fd < 0)
            return 0;
        int ret = close(_socket_fd);
        _socket_fd = -1;
        return ret;
    }
    // 接收接口
    int Receive(void *buffer, size_t len, int flag)
    {
        int r = recv(_socket_fd, buffer, len, flag);
        if (r < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
                return 0;
            LOG_ERROR("recv from %d failed: %s", _socket_fd, strerror(errno));
            return -1;
        }
        return r;
    }
    // 非阻塞接收
    int ReceiveWithNoBlock(void *buffer, size_t len)
    {
        return Receive(buffer, len, MSG_DONTWAIT);
    }
    // 发送接口
    int Send(const void *buffer, size_t len, int flag)
    {
        // 注意 size_t 类型永远 > 0
        if (len == 0)
        {
            return 0;
        }
        int s = send(_socket_fd, buffer, len, flag);
        if (s < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
                return 0;
            LOG_ERROR("send  to %d failed: %s", _socket_fd, strerror(errno));
            return -1;
        }
        return s;
    }
    // 非阻塞发送
    int SendWithNoBlock(const void *buffer, size_t len)
    {
        return Send(buffer, len, MSG_DONTWAIT);
    }
    // 开启地址端口复用
    bool EnableReuse()
    {
        int opt = 1;
        int n = setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (n < 0)
        {
            LOG_ERROR("setsockopt %d reuse address failed: %s", _socket_fd, strerror(errno));
            return false;
        }
        opt = 1;
        n = setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
        if (n < 0)
        {
            LOG_ERROR("setsockopt %d reuse port failed: %s", _socket_fd, strerror(errno));
            return false;
        }
        return true;
    }
    // 设置非阻塞
    bool SetNoBlock()
    {
        int old_flag = fcntl(_socket_fd, F_GETFL, 0);
        if (old_flag < 0)
        {
            LOG_ERROR("fcntl F_GETFL failed: %s", strerror(errno));
            return false;
        }

        if (fcntl(_socket_fd, F_SETFL, old_flag | O_NONBLOCK) < 0)
        {
            LOG_ERROR("fcntl F_SETFL failed: %s", strerror(errno));
            return false;
        }
        return true;
    }
    // 获取到 socket 文件描述符
    int GetFd()
    {
        return _socket_fd;
    }
    // 构建TCP server
    bool BuildTcpMethod(uint16_t port)
    {
        // 1. 创建套接字
        if (!SocketOrDie())
            return false;
        // 2. 开启地址复用
        if (!EnableReuse())
            return false;
        // // 3. 设置为非阻塞
        if (!SetNoBlock())
            return false;
        // 4. 绑定地址
        if (!BindOrDie("0.0.0.0", port))
            return false;
        // 5. 开始监听
        if (!ListenOrDie())
            return false;
        return true;
    }
    // 构建TCP client
    bool BuildTcpMethodClient(std::string server_ip, u_int16_t server_port)
    {
        // 1. 创建套接字
        if (!SocketOrDie())
            return false;
        // 2. 连接服务器
        if (!Connect(server_ip, server_port))
            return false;
        return true;
    }

private:
    int _socket_fd;
};

/// @brief 通用类 Any 模块
class Any
{
public:
    // constructor
    Any() : _content(nullptr) {}
    Any(Any &&other) noexcept
        : _content(other._content)
    {
        other._content = nullptr;
        // other.Swap(*this);
    }
    Any(const Any &other)
    {
        _content = other._content ? other._content->Clone() : nullptr;
    }
    template <class ValueType>
    Any(ValueType &&value)
        : _content(new Holder<std::decay_t<ValueType>>(std::forward<ValueType>(value)))
    {
    }
    // functional
    Any &Swap(Any &other) noexcept
    {
        std::swap(this->_content, other._content);
        return *this;
    }
    const std::type_info &Type() const
    {
        return _content ? _content->Type() : typeid(void);
    }
    bool IsEmpty() const noexcept
    {
        return !_content;
    }
    void Clear()
    {
        Any().Swap(*this);
    }
    // operator overload
    Any &operator=(const Any &other)
    {
        Any(other).Swap(*this);
        return *this;
    }
    Any &operator=(Any &&other)
    {
        // 1. 将 other 和 this 互换
        other.Swap(*this);
        // 2.将 other 中存留 this 中的资源清空
        Any().Swap(other);
        // 3. this中的资源转移到了other中，再次将other中的资源转移到Any()中，出作用域Any()会立即释放
        return *this;
    }
    template <class ValueType>
    Any &operator=(ValueType &&value)
    {
        // 完美转发---左值->左值--右值->右值
        Any(std::forward<ValueType>(value)).Swap(*this);
        return *this;
    }
    // destructor
    ~Any()
    {
        delete _content;
    }

private:
    class PlaceHolder
    {
    public:
        PlaceHolder() {}
        virtual ~PlaceHolder() {};
        virtual const std::type_info &Type() const = 0;
        virtual PlaceHolder *Clone() const = 0;
    };
    // 多态 + 虚函数->实现类型擦除
    template <class ValueType>
    class Holder : public PlaceHolder
    {
    public:
        // constructor
        Holder(const ValueType &value)
            : _held(value)
        {
        }
        Holder(ValueType &&value)
            : _held(std::move(value))
        {
        }
        // query
        const std::type_info &Type() const override
        {
            return typeid(_held);
        }
        PlaceHolder *Clone() const override
        {
            return new Holder(_held);
        }
        // private:
        ValueType _held;
    };

private:
    template <class ValueType>
    friend ValueType *AnyCast(Any &other) noexcept;
    template <class ValueType>
    friend const ValueType *AnyCast(const Any &other) noexcept;

    PlaceHolder *_content;
};

template <class ValueType>
ValueType *AnyCast(Any &other) noexcept
{
    if (other.Type() == typeid(ValueType))
    {
        return &static_cast<Any::Holder<ValueType> *>(other._content)->_held;
    }
    return nullptr;
}
template <class ValueType>
const ValueType *AnyCast(const Any &other) noexcept
{
    if (other.Type() == typeid(ValueType))
    {
        return &static_cast<const Any::Holder<ValueType> *>(other._content)->_held;
    }
    return nullptr;
}

/// @brief 事件管理模块
// 位图注意事项
// 1. | FLAG  -> 开启 FLAG
// 2. & ~FLAG -> 移除 FLAG
// 3. & FLAG  -> 检测 FLAG 是否存在
class Poller;
class EventLoop;

class Channel
{
private:
    using EventCallback = std::function<void()>;

public:
    Channel(EventLoop &eventloop, int fd = DEFAULT_FD)
        : _fd(fd),
          _event_loop(eventloop),
          _monitor_event(0),
          _trigger_event(0)
    {
    }
    ~Channel() {}
    // 设置实际触发的事件
    void setTriggeredEvent(const uint32_t &event)
    {
        _trigger_event = event;
    }
    // 设置读事件回调
    void SetReadCallback(const EventCallback &callback)
    {
        _read_callback = callback;
    }
    // 设置写事件回调
    void SetWriteCallback(const EventCallback &callback)
    {
        _write_callback = callback;
    }
    // 设置关闭事件回调
    void SetCloseCallback(const EventCallback &callback)
    {
        _close_callback = callback;
    }
    // 设置错误事件回调
    void SetErrorCallback(const EventCallback &callback)
    {
        _error_callback = callback;
    }
    // 设置任意事件回调
    void SetAnyCallback(const EventCallback &callback)
    {
        _any_callback = callback;
    }
    // 是否监控了可读事件
    bool IsReadable()
    {
        return (_monitor_event & EPOLLIN);
    }
    // 是否监控了可写事件
    bool IsWriteable()
    {
        return (_monitor_event & EPOLLOUT);
    }
    // 两者会涉及到外部类 EventLoop 的定义---需要放到后面实现
    // 移除事件
    void Remove();
    // 更新入口
    void Update();
    // 开启可读事件
    void EnableRead()
    {
        _monitor_event |= EPOLLIN;
        Update();
    }
    // 开启可写事件
    void EnableWrite()
    {
        _monitor_event |= EPOLLOUT;
        Update();
    }
    // 关闭可读事件
    void DisableRead()
    {
        _monitor_event &= ~EPOLLIN;
        Update();
    }
    // 关闭可写事件
    void DisableWrite()
    {
        _monitor_event &= ~EPOLLOUT;
        Update();
    }
    // 关闭所有事件
    void DisableAll()
    {
        _monitor_event = 0;
        Update();
    }
    // 执行回调入口
    void HandleEvent()
    {
        // 1. 任意事件就绪
        if (_any_callback)
            _any_callback();
        // 2. 关闭事件就绪
        if ((_trigger_event & EPOLLHUP) && !(_trigger_event & EPOLLIN))
        {
            // 可能调用的回调已经讲 Channel 对象释放，但是还会往后执行 -> UAF---TODO-Connection 中处理
            if (_close_callback)
                _close_callback();
        }
        // 3. 错误事件就绪
        if (_trigger_event & EPOLLERR)
        {
            if (_error_callback)
                _error_callback();
        }
        // 4. 可读事件就绪
        // EPOLLIN  -> 普通数据可读
        // EPOLLPRI -> 紧急数据可读
        // EPOLLHUP -> 对端关闭(读取到 0/EOF)可读
        if (_trigger_event & (EPOLLIN | EPOLLPRI | EPOLLHUP))
        {
            if (_read_callback)
                _read_callback();
        }
        // 5. 可写事件就绪
        if (_trigger_event & EPOLLOUT)
        {
            if (_write_callback)
                _write_callback();
        }
    }
    // 获取连接描述符
    int GetFd()
    {
        return _fd;
    }
    // 获取当前监控的事件
    uint32_t GetMonitorEvent()
    {
        return _monitor_event;
    }

private:
    int _fd;                       // 事件对应连接描述符
    EventLoop &_event_loop;        // 所属的事件循环---Update/Remove 时通过其调用上层 Poller
    uint32_t _monitor_event;       // 当前连接监控的事件
    uint32_t _trigger_event;       // 当前连接触发的事件
    EventCallback _read_callback;  // 读事件回调
    EventCallback _write_callback; // 写事件回调
    EventCallback _close_callback; // 关闭事件回调
    EventCallback _error_callback; // 错误事件回调
    EventCallback _any_callback;   // 任意事件回调
};

/// @brief poller 模块
// 通过 fd->Channel* 映射操作 epoll_* 接口
class Poller
{
private:
#define MAX_EPOLL_EVENTS 1024
    // 判断是否存在
    bool IsExist(int fd)
    {
        auto it = _channels.find(fd);
        return it == _channels.end() ? false : true;
    }
    // 内部更新事件
    void Update(Channel *channel, int op)
    {
        int fd = channel->GetFd();
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = channel->GetMonitorEvent();
        int ec = epoll_ctl(_ep_fd, op, fd, &ev);
        if (ec < 0)
        {
            LOG_ERROR("epoll_ctl %d failed: %s", fd, strerror(errno));
            return;
        }
    }

public:
    Poller(int ep_fd = DEFAULT_FD)
        : _ep_fd(ep_fd)
    {
        _ep_fd = epoll_create(MAX_EPOLL_EVENTS);
        if (_ep_fd < 0)
        {
            LOG_ERROR("epoll_create failed: %s", strerror(errno));
        }
    }
    ~Poller() {}
    // 添加/修改事件
    void AddOrUpdateEvent(Channel *channel)
    {
        int fd = channel->GetFd();
        if (IsExist(fd))
        {
            // 存在 -> 更新
            Update(channel, EPOLL_CTL_MOD);
        }
        else
        {
            // 不存在 -> 插入
            Update(channel, EPOLL_CTL_ADD);
            _channels.insert(std::make_pair(fd, channel));
        }
    }
    // 移除事件
    void RemoveEvent(Channel *channel)
    {
        int fd = channel->GetFd();
        if (!IsExist(fd))
        {
            LOG_ERROR("No file descriptor %d : %s", fd, strerror(errno));
            return;
        }
        auto it = _channels.find(fd);
        if (it == _channels.end())
        {
            LOG_ERROR("");
        }
        // 1) 更新事件
        Update(channel, EPOLL_CTL_DEL);
        // 2) 更新管理容器
        _channels.erase(fd);
    }
    // 开始监听
    void StartListening(std::vector<Channel *> &active)
    {
        // 开始等待事件就绪---阻塞等待
        int ew = epoll_wait(_ep_fd, _event, MAX_EPOLL_EVENTS, -1);
        if (ew < 0)
        {
            if (errno == EINTR)
            {
                return;
            }
            LOG_ERROR("epoll_wait %d failed: %s", _ep_fd, strerror(errno));
            return;
        }
        // 将就绪的连接个数返回
        for (int i = 0; i < ew; i++)
        {
            auto it = _channels.find(_event[i].data.fd);
            if (it != _channels.end())
            {
                // 为就绪连接设置事件
                it->second->setTriggeredEvent(_event[i].events);
                // 保存就绪的事件
                active.push_back(it->second);
            }
        }
    }

private:
    int _ep_fd;                                   // epoll 文件描述符
    struct epoll_event _event[MAX_EPOLL_EVENTS];  // epoll_event 结构体
    std::unordered_map<int, Channel *> _channels; // 管理 channel 的容器
};

/// @brief 定时任务模块
#define DEFAULT_WHEEL_CAPACITY 60
using TaskFunction = std::function<void()>;
using ReleaseFunction = std::function<void()>;

class TimerTask
{
public:
    TimerTask(uint64_t task_id, uint32_t trigger_time, int rotation = 0)
        : _task_id(task_id),
          _trigger_time(trigger_time),
          _rotation(rotation),
          _is_cancel(false)
    {
    }
    TimerTask(uint64_t task_id, uint32_t trigger_time, int rotation, const TaskFunction &task_callback, const ReleaseFunction &release_callback)
        : _task_id(task_id),
          _trigger_time(trigger_time),
          _rotation(rotation),
          _is_cancel(false),
          _task_callback(task_callback),
          _release_callback(release_callback)
    {
    }
    ~TimerTask()
    {
        if (_task_callback && !_is_cancel)
        {
            _task_callback();
        }
        if (_release_callback && !_is_cancel)
            _release_callback();
    }
    void SelfDecrease()
    {
        --_rotation;
    }
    void SetTaskFunction(const ReleaseFunction &task_callback)
    {
        _task_callback = task_callback;
    }
    void SetReleaseFunction(const ReleaseFunction &release_callback)
    {
        _release_callback = release_callback;
    }
    void SetCancel(bool is_cancel)
    {
        _is_cancel = is_cancel;
    }
    uint32_t GetTriggeredTime()
    {
        return _trigger_time;
    }
    TaskFunction GetTaskCallback() const
    {
        if (_task_callback)
            return _task_callback;
        else
            return nullptr;
    }
    int GetRotation()
    {
        return _rotation;
    }
    uint64_t GetTaskId()
    {
        return _task_id;
    }

private:
    uint64_t _task_id;                 // 任务 ID
    uint32_t _trigger_time;            // 任务触发时间
    int _rotation;                     // 圈数
    bool _is_cancel;                   // 是否取消(false-不取消--true-取消)
    TaskFunction _task_callback;       // 具体任务函数回调
    ReleaseFunction _release_callback; // 清理相关资源
};

using TimerTaskShared = std::shared_ptr<TimerTask>;
using TimerTaskWeak = std::weak_ptr<TimerTask>;

/// @brief 时间轮模块
class WheelOfTime
{
private:
    struct TimerRef
    {
        TimerTaskWeak _weak_task;
        int _slot;
    };

private:
    // 创建定时器 fd
    static int CreateTimerFd()
    {
        int timer_fd = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC);
        if (timer_fd < 0)
        {
            LOG_ERROR("timerfd_create failed: %s", strerror(errno));
            return -1;
        }
        // 2. 设置定时时间
        // 1) 设置定时周期
        struct itimerspec scheduled_value;
        scheduled_value.it_value.tv_sec = 1;     // 第一次超时(s)
        scheduled_value.it_value.tv_nsec = 0;    // (ns)
        scheduled_value.it_interval.tv_sec = 1;  // 第一次超时后间隔多长时间再次超时
        scheduled_value.it_interval.tv_nsec = 0; // (ns)
        // 2) 设置到定时器上---设置即启动
        timerfd_settime(timer_fd, 0, &scheduled_value, NULL);
        return timer_fd;
    }
    // 定时器超时---仅仅是 _timer_fd 变得可读，即定时器超时了
    int ReadTimerFd()
    {
        time_t base = time(NULL);
        u_int64_t tmp;
        size_t n = read(_timer_fd, &tmp, sizeof(tmp));
        // 读取错误
        if (n != sizeof(tmp))
            return 0;
        // 积压了多次超时
        if (tmp > 1)
            LOG_ERROR("Timer lost: %lu expirations", tmp - 1);
        // 读取正常
        // LOG_INFO("Timer cycle: %lu", tmp);
        return (int)tmp;
    }
    // 添加任务
    void AddTimerTask(uint64_t task_id, uint32_t trigger_time, const TaskFunction &task_callback)
    {
        // 注: 圈数的设置 -> 目标圈数 - 1
        trigger_time = (trigger_time == 0 ? 1 : trigger_time);
        int rotation = (trigger_time - 1) / _capacity;
        int slot = (_tick + trigger_time) % _capacity;
        // 1) 创建定时任务 Shared 指针并设置任务和清理资源函数
        TimerTaskShared timer_task_shared = std::make_shared<TimerTask>(
            task_id, trigger_time, rotation,
            task_callback, std::bind(&WheelOfTime::RemoveTimerTask, this, task_id));
        // 2) 写入时间轮
        _timer_wheel[slot].push_back(timer_task_shared);
        // 3) 添加到容器中管理
        _timers[task_id] = TimerRef{TimerTaskWeak(timer_task_shared), slot};
    }
    // 刷新任务(重置触发时间)
    void RefreshTimerTask(uint64_t task_id)
    {
        // BUG---旧任务只是被标记了，任然停留在时间轮中，会导致内存暴涨
        auto it = _timers.find(task_id);
        if (it == _timers.end())
            return;
        // 1. 找到原来的任务指针
        TimerTaskShared old_task = it->second._weak_task.lock();
        if (!old_task)
        {
            _timers.erase(it);
            return;
        }
        // 2. 保存一份构造新任务需要的内容
        uint32_t triggered_time = old_task->GetTriggeredTime();
        TaskFunction task_function = old_task->GetTaskCallback();
        // 3. 清理所有旧任务有关的内容
        // 1) 取消原来的任务指针
        old_task->SetCancel(true);

        // 旧任务的位置---可能包含多个任务
        int old_slot = it->second._slot;
        // 旧任务位置上的任务
        auto &bucket = _timer_wheel[old_slot];
        for (auto iter = bucket.begin(); iter != bucket.end(); ++iter)
        {
            if (iter->get() == old_task.get())
            {
                bucket.erase(iter);
                break;
            }
        }

        // 2) 清除旧任务的索引
        _timers.erase(it); // 避免出现过期的 task_id 在 _timers 中
        // 4. 设置新的任务指针
        // LOG_DEBUG("Refresh %ld", task_id);
        AddTimerTask(task_id, triggered_time, task_function);
    }
    // 取消定时任务
    void CancelTimerTask(uint64_t task_id)
    {
        auto it = _timers.find(task_id);
        // 没有任务
        if (it == _timers.end())
            return;
        TimerTaskShared timer_task_shared = it->second._weak_task.lock();
        // 取消任务
        if (timer_task_shared)
            timer_task_shared->SetCancel(true);
    }
    // 清理管理空间
    void RemoveTimerTask(uint64_t task_id)
    {
        // BUG---并没有完全移除，仅仅是移除了标识符
        auto it = _timers.find(task_id);
        if (it == _timers.end())
            return;
        TimerTaskShared task = it->second._weak_task.lock();
        if (task)
        {
            // 1. 先取消，避免被执行
            task.get()->SetCancel(true);
            // 2. 从时间轮桶中删除
            int old_slot = it->second._slot;
            auto &bucket = _timer_wheel[old_slot];
            for (auto iter = bucket.begin(); iter != bucket.end(); ++iter)
            {
                if (iter->get() == task.get())
                {
                    bucket.erase(iter);
                    break;
                }
            }
        }
        // 3. 清除索引
        // 找得到就说明需要移除 -> _timers 内部 task_id 全是合法的
        _timers.erase(it);
    }
    // 转动时间轮---仅仅是转动，没有带上定时器
    void RotateWheel()
    {
        // 1. 逐个清除
        // 1) 移动秒针
        _tick = (_tick + 1) % _capacity;
        // 2) 获取到秒针指向位置上的所有任务
        auto &bucket = _timer_wheel[_tick];
        // 3) 逐个进行检查---注意迭代器失效的问题 -> 不要把++操作直接放在for语句中!!!
        for (auto it = bucket.begin(); it != bucket.end();)
        {
            // 圈数实际上是自带一圈，3圈-> 3->2 2-1 走到1的时候其实已经是说明走完3圈了---外部必须传入严格正确的圈数(实际圈数-1)
            // 圈数 > 0
            if ((*it)->GetRotation() > 0)
            {
                // 秒针在一直移动，任务的位置在轮盘上是固定的---如果下一次秒针再次经过任务位置的时候，就说明走完了一圈!!!
                (*it)->SelfDecrease();
                ++it;
            }
            // 圈数 <= 0
            else
            {
                // 从任务队列中移除---引用计数会减少，减到0自动释放，释放即执行回调
                it = bucket.erase(it); // erase 会返回下一个有效的迭代器，不需要手动++，且不能访问原来旧的迭代器!!!
            }
        }
    }
    // 定时器到了 -> 定时器超时 + 转动时间轮
    void OnTime()
    {
        int n = ReadTimerFd();
        for (int i = 0; i < n; i++)
        {
            RotateWheel();
        }
    }
    void FindTimerTask(uint64_t task_id, int *is_exists)
    {
        for (auto it : _timers)
        {
            if (it.first == task_id)
            {
                *is_exists = 1;
                return;
            }
        }
        *is_exists = 0;
    }

public:
    WheelOfTime(EventLoop &event_loop, int wheel_capacity = DEFAULT_WHEEL_CAPACITY)
        : _tick(0),
          _capacity(wheel_capacity),
          _timer_fd(CreateTimerFd()),
          _timer_wheel(_capacity),
          _event_loop(event_loop),
          _channel(std::make_unique<Channel>(_event_loop, _timer_fd))
    {
        // 事件驱动
        // a) 设置读回调
        _channel.get()->SetReadCallback(std::bind(&WheelOfTime::OnTime, this));
        // b) 开启可读事件
        _channel.get()->EnableRead();
    }
    ~WheelOfTime() {}
    // 将对任务的操作放到 EventLoop 中
    void AddTimerTaskInEventLoop(uint64_t task_id, uint32_t trigger_time, const TaskFunction &task_callback);
    void RefreshTimerTaskInEventLoop(uint64_t task_id);
    void CancelTimerTaskInEventLoop(uint64_t task_id);
    void RemoveTimerTaskInEventLoop(uint64_t task_id);
    void FindTimerTaskInEventLoop(uint64_t task_id, int *is_exists);

private:
    int _tick;                                                   // 秒针
    int _capacity;                                               // 容量
    int _timer_fd;                                               // 定时器描述符
    std::vector<std::vector<TimerTaskShared>> _timer_wheel;      // 时间轮盘
    std::unordered_map<uint64_t, WheelOfTime::TimerRef> _timers; // 管理所有的时间轮盘---不能存放shared_ptr，会导致引用计数增加---需要记录在时间轮盘上的位置
    EventLoop &_event_loop;                                      // 所属的事件循环---Add/Refresh/Cancel/Remove 时通过其调用上层
    std::unique_ptr<Channel> _channel;                           // _timer_fd -> Channel* 映射
};

/// @brief 事件循环模块---负责IO
// One Thread One Loop---Thread 中所有涉及到线程安全的问题统一放到 EventLoop 中处理
class EventLoop
{
private:
    using TaskHandler = std::function<void()>;

    static int CreateEventFd()
    {
        int event_fd = eventfd(0, 0);
        if (event_fd < 0)
        {
            LOG_ERROR("eventfd create failed: %s", strerror(errno));
            return -1;
        }
        return event_fd;
    }
    void WriteEventFd()
    {
        uint64_t val = 1;
        int w = write(_event_fd, &val, sizeof(val));
        if (w < 0)
        {
            // 被信号打断
            if (errno == EINTR)
                return;
            LOG_ERROR("write %d failed: %s", _event_fd, strerror(errno));
            return;
        }
    }
    void ReadEventFd()
    {
        uint64_t val = 0;
        int r = read(_event_fd, &val, sizeof(val));
        if (r < 0)
        {
            // 被信号打断 || 无数据可读
            if (errno == EINTR || errno == EAGAIN)
                return;
            LOG_ERROR("write %d failed: %s", _event_fd, strerror(errno));
            return;
        }
    }
    // 执行任务池中的任务
    void ExecuteTask()
    {
        std::vector<TaskHandler> task_poll;
        // 1. 置换到另一个容器中执行---避免在执行时有任务要插入无法插入或者影响效率
        {
            // a) 加锁实现线程安全
            std::lock_guard<std::mutex> lock_guard(_task_poll_mutex);
            _task_poll.swap(task_poll);
        }
        // 2. 在另一个容器中执行任务，执行的过程不存在线程安全---执行的其实是设置的回调
        for (auto task_callback : task_poll)
        {
            task_callback();
        }
        // 是本线程中的任务---直接执行---否则插入到任务池中
    }

public:
    EventLoop()
        : _thread_id(std::this_thread::get_id()),
          _event_fd(CreateEventFd()),
          _channel(std::make_unique<Channel>(*this, _event_fd)),
          _wheel_of_time(*this)
    {
        // BUG---_channel没有绑定ReadEventFd，也没有EnableRead
        _channel.get()->SetReadCallback(std::bind(&EventLoop::ReadEventFd, this));
        _channel.get()->EnableRead();
    }
    // 是否是本线程自己
    bool IsThreadsSelf()
    {
        return (_thread_id == std::this_thread::get_id());
    }
    // 在事件循环中执行
    void ExecuteInEventLoop(const TaskHandler &task_callback)
    {
        // 是本线程自己直接执行---否则插入到任务池中
        if (IsThreadsSelf())
        {
            return task_callback();
        }
        else
        {
            // a) 插入到任务队列中
            {
                // 加锁保护
                std::lock_guard<std::mutex> lock_guard(_task_poll_mutex);
                _task_poll.push_back(task_callback);
            }
            // b) 唤醒写事件就绪
            WriteEventFd();
        }
    }
    // 压入到任务池中，延时执行
    void QueueInLoop(const TaskFunction &task_callback)
    {
        // 1. 插入到任务池中
        {
            // a) 加锁实现线程安全
            std::lock_guard<std::mutex> lock_guard(_task_poll_mutex);
            _task_poll.push_back(task_callback);
        }
        // 2. 不是线程自己执行就通知写事件就绪
        if (!IsThreadsSelf())
        {
            WriteEventFd();
        }
    }
    // 开启事件循环
    void Start()
    {
        while (true)
        {
            // 1. 开始监听
            std::vector<Channel *> active;
            _poller.StartListening(active);
            // 2. 执行事件处理函数回调
            for (auto &channel : active)
            {
                channel->HandleEvent();
            }
            // 3. 执行任务
            ExecuteTask();
            // ?. 同一个 channel 会出现两次吗?
        }
    }
    // 添加/修改事件
    void AddOrUpdateEvent(Channel *channel)
    {
        _poller.AddOrUpdateEvent(channel);
    }
    // 移除事件
    void RemoveEvent(Channel *channel)
    {
        _poller.RemoveEvent(channel);
    }
    // 添加定时任务
    void AddTimerTask(uint64_t task_id, uint32_t trigger_time, const TaskFunction &task_callback)
    {
        _wheel_of_time.AddTimerTaskInEventLoop(task_id, trigger_time, task_callback);
    }
    // 将定时任务延时
    void RefreshTimerTask(uint64_t task_id)
    {
        _wheel_of_time.RefreshTimerTaskInEventLoop(task_id);
    }
    // 取消定时任务
    void CancelTimerTask(uint64_t task_id)
    {
        _wheel_of_time.CancelTimerTaskInEventLoop(task_id);
    }
    // 移除定时任务
    void RemoveTimerTask(uint64_t task_id)
    {
        _wheel_of_time.RemoveTimerTaskInEventLoop(task_id);
    }
    // 判断是否存在定时任务---非线程安全---仅仅在线程安全的地方使用
    void FindTimerTask(uint64_t task_id, int *is_exists)
    {
        _wheel_of_time.FindTimerTaskInEventLoop(task_id, is_exists);
    }

    ~EventLoop() {}

private:
    std::thread::id _thread_id;          // 线程 ID
    int _event_fd;                       // 通知 IO 事件就绪的 fd
    std::vector<TaskHandler> _task_poll; // 任务池
    std::mutex _task_poll_mutex;         // 任务池锁---保证线程安全
    Poller _poller;                      // Poller -> 具体的 epoll_* 接口 -> 操作 Channel
    std::unique_ptr<Channel> _channel;   // _event_fd -> Channel* 映射
    WheelOfTime _wheel_of_time;          // 时间轮 -> 用于执行定时任务
};

// 用到 EventLoop 中的方法，这时候需要保证 EventLoop 是一个完整的对象
// 移除事件
void Channel::Remove()
{
    _event_loop.RemoveEvent(this);
}
// 更新入口
void Channel::Update()
{
    _event_loop.AddOrUpdateEvent(this);
}
void WheelOfTime::AddTimerTaskInEventLoop(uint64_t task_id, uint32_t trigger_time, const TaskFunction &task_callback)
{
    _event_loop.ExecuteInEventLoop(std::bind(&WheelOfTime::AddTimerTask, this, task_id, trigger_time, task_callback));
}
void WheelOfTime::RefreshTimerTaskInEventLoop(uint64_t task_id)
{
    _event_loop.ExecuteInEventLoop(std::bind(&WheelOfTime::RefreshTimerTask, this, task_id));
}
void WheelOfTime::CancelTimerTaskInEventLoop(uint64_t task_id)
{
    _event_loop.ExecuteInEventLoop(std::bind(&WheelOfTime::CancelTimerTask, this, task_id));
}
void WheelOfTime::RemoveTimerTaskInEventLoop(uint64_t task_id)
{
    _event_loop.ExecuteInEventLoop(std::bind(&WheelOfTime::RemoveTimerTask, this, task_id));
}
void WheelOfTime::FindTimerTaskInEventLoop(uint64_t task_id, int *is_exists)
{
    _event_loop.ExecuteInEventLoop(std::bind(&WheelOfTime::FindTimerTask, this, task_id, is_exists));
}

/// @brief 连接管理模块

enum Connection_Status
{
    CONNECTTING = 0,    // 连接建立成功---待处理的状态
    CONNECTED = 1,      // 连接建立完成，相关设置已经就绪，可以通信的状态
    DISCONNECTTING = 2, // 连接待关闭的状态
    DISCONNECTED = 3    // 连接已经关闭的状态
};

class Connection : public std::enable_shared_from_this<Connection>
{
public:
    // 外部使用的时候，为了防止一个 Connection 中某些操作释放了 Connection 或者某些资源被释放了导致后续的操作无效 -> 统一使用 shared_ptr 进行管理
    using ConnectionShared = std::shared_ptr<Connection>;

private:
    using ConnectedCallback = std::function<void(ConnectionShared)>;
    using AfterRecieveCallback = std::function<void(ConnectionShared, Buffer *)>;
    using BeforeCloseCallback = std::function<void(ConnectionShared)>;
    using AnyEventCallback = std::function<void(ConnectionShared)>;
    using ConnectionClosed = std::function<void(ConnectionShared)>;

    // 处理读事件
    void HandleRead()
    {
        // 1. 将数据放到输入缓冲区中
        char tmp[1024 * 4];
        int r = _socket.ReceiveWithNoBlock(tmp, sizeof(tmp));
        if (r < 0)
        {
            // 出错了不能直接释放!!!---关闭连接，因为只是读取发送错误，可能其它模块还在处理
            return ShutdownConnectionInLoop();
        }
        else if (r == 0)
        {
            // 读取到 0 并不是出错了，说明没有读取到数据
            // LOG_INFO("Read zero");
            return ShutdownConnectionInLoop();
            return;
        }
        // 读取到数据，说明不是非活跃连接---如果开启了非活跃释放标志位，要进行刷新
        if (_is_inactive_release)
        {
            // LOG_DEBUG("Before refresh");
            _event_loop->RefreshTimerTask(_id);
        }
        _in_buffer.Write(tmp, r); // Write 内部会移动偏移量
        // 2. 将输入缓冲区的数据交给处理业务的调用
        if (_in_buffer.GetReadableSize() > 0 && _after_receive_callback)
        {
            // 收到数据后的处理和发送数据后的处理是不对称的
            // 收到数据，服务端知道怎么处理
            // 发送数据，客户端知道怎么处理
            return _after_receive_callback(shared_from_this(), &_in_buffer);
        }
    }
    // 处理写事件
    void HandleWrite()
    {
        // 1. 将输出缓冲区中的数据发送出去
        int s = _socket.SendWithNoBlock(_out_buffer.GetReadPosition(), _out_buffer.GetReadableSize());
        if (s < 0)
        {
            // 未发送成功 ↓
            // a) 输出缓冲区中有/没有数据---肯定有数据，不过现在出错了，无法发送，直接丢弃
            // b) 输入缓冲区中有/没有数据---有 -> 处理数据 没有 -> 释放连接
            if (_in_buffer.GetReadableSize() > 0 && _after_receive_callback)
            {
                return _after_receive_callback(shared_from_this(), &_in_buffer);
            }
            // 直接释放当前连接，而不是关闭，因为当前无法发送，也就无法响应给客户端
            return Release();
        }
        // LOG_DEBUG("Actually send %d bytes to fd %d", s, _socket_fd);
        // 2. 检查输出缓冲是否还有数据
        _out_buffer.MoveReadOffset(s);
        if (_out_buffer.GetReadableSize() == 0)
        {
            // BUG!!!---事件循环风暴---都需要关闭写事件
            // a) 关闭写事件
            _channel.DisableWrite();
            _out_buffer.Reset();
            if (_connection_status == Connection_Status::DISCONNECTTING)
                return Release();
            return;
        }
    }
    // 处理关闭/挂起事件
    void HandleClose()
    {
        // 1. 设置状态
        if (_connection_status == Connection_Status::DISCONNECTED)
        {
            return;
            // _connection_status = Connection_Status::DISCONNECTED;
        }
        // 2. 处理残余数据
        if (_in_buffer.GetReadableSize() > 0 && _after_receive_callback)
            // 输入缓冲区中有数据，收到数据后的处理逻辑
            _after_receive_callback(shared_from_this(), &_in_buffer);
        if (_out_buffer.GetReadableSize() > 0)
            // 输出缓冲区中有数据，开启写事件就绪即可
            _channel.EnableWrite();
        // 统一在 Release 中处理
        // // 2. 解除监控
        // _event_loop->RemoveEvent(&_channel);
        // 2. 执行回调
        if (_before_close_callback)
            _before_close_callback(shared_from_this());
        Release();
    }
    // 处理错误事件
    void HandleError()
    {
        // 出现错误的时候，准备关闭连接
        HandleClose();
    }
    // 处理任意事件
    void HandleAny()
    {
        // 应该在读取到事件的时候刷新
        // // 1. 刷新事件循环---如果标志位开启
        // if (_is_inactive_release)
        // {
        //     _event_loop->RefreshTimerTask(_id);
        // }
        // 2. 执行回调
        if (_any_event_callback)
            _any_event_callback(shared_from_this());
    }
    // 执行连接建立成功的回调
    void ExecuteConnectedInLoop()
    {
        // 1. 修改标志位
        _connection_status = Connection_Status::CONNECTED;
        // 2. 执行回调
        if (_connected_callback)
            _connected_callback(shared_from_this());
    }
    // 执行连接收到数据后的回调
    void ExecuteAfterReceiveInLoop()
    {
        if (_after_receive_callback)
        {
            _after_receive_callback(shared_from_this(), &_in_buffer);
        }
    }
    // 执行连接关闭前的回调
    void ExecuteBeforeCloseInLoop()
    {
        if (_before_close_callback)
        {
            _before_close_callback(shared_from_this());
        }
    }
    // 执行连接中任意事件的回调
    void ExecuteAnyEventInLoop()
    {
        if (_any_event_callback)
        {
            _any_event_callback(shared_from_this());
        }
    }
    void SendInLoop(Buffer &buffer)
    {
        // 1. 检查标志位
        if (_connection_status == Connection_Status::DISCONNECTED)
        {
            return;
        }
        // 2. 向输出缓冲区中写入
        _out_buffer.Write(buffer.GetReadPosition(), buffer.GetReadableSize());
        // 3. 开启可读事件就绪
        _channel.EnableWrite();
    }
    void SetBusyInLoop(bool is_busy)
    {
        _is_busy = is_busy;
    }
    void EnableInactiveReleaseInLoop(int seconds)
    {
        if (_connection_status != Connection_Status::CONNECTED)
            return;
        if (_is_busy)
            return;
        // 1. 修改标志位
        _is_inactive_release = true;
        // 2. 定时器(非活跃连接销毁)存在就刷新定时器，否则插入
        int is_exists = -1;
        _event_loop->FindTimerTask(_id, &is_exists);
        if (is_exists == 1)
            _event_loop->RefreshTimerTask(_id);
        else
            _event_loop->AddTimerTask(_id, seconds, std::bind(&Connection::InactiveRelease, shared_from_this())); // 注意: 非活跃连接销毁的任务是释放连接
    }
    void DisableInactiveReleaseInLoop()
    {
        // LOG_DEBUG("Enter DisableInactiveReleaseInLoop");
        // 1. 修改标志位
        _is_inactive_release = false;
        // 2. 移除定时器
        int is_exists = -1;
        _event_loop->FindTimerTask(_id, &is_exists);
        // 找到定时器---会修改为 1
        if (is_exists == 1)
        {
            _event_loop->RemoveTimerTask(_id);
        }
    }
    void EstablishedInLoop()
    {
        // 1. 修改标志位
        _connection_status = Connection_Status::CONNECTED;
        // 2. 开启读事件就绪
        _channel.EnableRead();
        // 3. 执行回调
        if (_connected_callback)
            _connected_callback(shared_from_this());
        // 4. 如果开启了非活跃连接释放，那么在开始事件循环之后刷新一下定时器
        if (_is_inactive_release)
        {
            int is_exists = -1;
            _event_loop->FindTimerTask(_id, &is_exists);
            if (is_exists == 1)
            {
                _event_loop->RefreshTimerTask(_id);
            }
        }
    }
    void SwitchInLoop(Any context,
                      const ConnectedCallback &connected_callback,
                      const AfterRecieveCallback &after_receive_callback,
                      const BeforeCloseCallback &before_close_callback,
                      const AnyEventCallback &any_event_callback,
                      const ConnectionClosed &connection_closed_callback)
    {
        _context = context;
        _connected_callback = connected_callback;
        _after_receive_callback = after_receive_callback;
        _before_close_callback = before_close_callback;
        _any_event_callback = any_event_callback;
        _connection_closed_callback = connection_closed_callback;
    }
    void ShutdownConnectionInLoop()
    {
        // 1. 修改标志位
        _connection_status = Connection_Status::DISCONNECTTING;
        // 2. 处理残余数据---如果客户端的请求格式不对或者发送恶意请求，就会导致服务器一直处理，无法关闭连接
        // if (_in_buffer.GetReadableSize() > 0 && _after_receive_callback)
        //     _after_receive_callback(shared_from_this(), &_in_buffer);
        if (_out_buffer.GetReadableSize() > 0)
            _channel.EnableWrite();
        else
            Release();
        // 3. 执行回调
        if (_before_close_callback)
            ExecuteBeforeCloseInLoop();
    }
    void ReleaseConnectionInLoop()
    {
        // 0. 关闭 幂等
        if (_connection_status == Connection_Status::DISCONNECTED)
            return;
        // 1. 修改标志位
        _connection_status = Connection_Status::DISCONNECTED;
        // 2. 移除关心事件
        _channel.Remove();
        // 3. 移除相关的定时器
        DisableInactiveReleaseInLoop();
        // 4. 关闭套接字
        _socket.Close();
        // 回调放到外层处理
        // 5. 执行回调
        // if (_before_close_callback)
        //     _before_close_callback(shared_from_this());
        if (_connection_closed_callback)
            _connection_closed_callback(shared_from_this());
    }
    void InactiveReleaseInLoop()
    {
        if (_connection_status == Connection_Status::DISCONNECTED)
            return;
        if (_is_inactive_release && !_is_busy && _connection_status == Connection_Status::CONNECTED)
        {
            // 非活跃链接标志 + 不忙状态 + 正常连接状态
            ReleaseConnectionInLoop();
        }
    }

public:
    Connection(uint64_t id, int socket_fd, EventLoop *event_loop)
        : _id(id),
          _is_inactive_release(false),
          _is_busy(false),
          _connection_status(Connection_Status::CONNECTTING),
          _socket_fd(socket_fd),
          _socket(_socket_fd),
          _event_loop(event_loop),
          _channel(*_event_loop, _socket_fd)
    {
        // 1. 开启事件循环
        _channel.SetReadCallback(std::bind(&Connection::HandleRead, this));
        _channel.SetWriteCallback(std::bind(&Connection::HandleWrite, this));
        _channel.SetCloseCallback(std::bind(&Connection::HandleClose, this));
        _channel.SetErrorCallback(std::bind(&Connection::HandleError, this));
        _channel.SetAnyCallback(std::bind(&Connection::HandleAny, this));
        // // 2. 开启可读事件
        // _channel.EnableRead();
    }
    // 获取内部成员变量的接口
    // 获取 ID
    int GetId()
    {
        return _id;
    }
    // 获取通信被描述符
    int GetFd()
    {
        return _socket_fd;
    }
    // 获取协议上下文
    Any *GetContext()
    {
        return &_context;
    }
    // 获取所处状态
    Connection_Status GetStatus()
    {
        return _connection_status;
    }

    // 设置回调的接口
    void SetConnectedCallback(const ConnectedCallback &callback)
    {
        _connected_callback = callback;
    }
    void SetAfterRecieveCallback(const AfterRecieveCallback &callback)
    {
        _after_receive_callback = callback;
    }
    void SetBeforeCloseCallback(const BeforeCloseCallback &callback)
    {
        _before_close_callback = callback;
    }
    void SetAnyEventCallback(const AnyEventCallback &callback)
    {
        _any_event_callback = callback;
    }
    void SetConnectionClosedCallback(const ConnectionClosed &callback)
    {
        _connection_closed_callback = callback;
    }
    // 功能接口
    // 发送数据
    void Send(const void *buffer, size_t len)
    {
        // 外面的 buffer 可能是个临时变量
        // 现在将发送操作压入任务池，有可能不会立即执行
        // 等到继续执行的时候，buffer 指向的空间可能已经被释放了
        Buffer tmp;
        tmp.Write(buffer, len);
        _event_loop->ExecuteInEventLoop(std::bind(&Connection::SendInLoop, shared_from_this(), tmp));
    }
    void SetBusy(bool is_busy)
    {
        _event_loop->ExecuteInEventLoop(std::bind(&Connection::SetBusyInLoop, shared_from_this(), is_busy));
    }
    // 开启非活跃连接释放
    void EnableInactiveRelease(int seconds)
    {
        _event_loop->ExecuteInEventLoop(std::bind(&Connection::EnableInactiveReleaseInLoop, shared_from_this(), seconds));
    }
    // 关闭非活跃连接释放
    void DisableInactiveRelease()
    {
        _event_loop->ExecuteInEventLoop(std::bind(&Connection::DisableInactiveReleaseInLoop, shared_from_this()));
    }
    // 建立连接
    void Established()
    {
        _event_loop->ExecuteInEventLoop(std::bind(&Connection::EstablishedInLoop, shared_from_this()));
    }
    // 切换协议
    void Switch(Any context,
                const ConnectedCallback &connected_callback,
                const AfterRecieveCallback &after_receive_callback,
                const BeforeCloseCallback &before_close_callback,
                const AnyEventCallback &any_event_callback,
                const ConnectionClosed &connection_closed_callback)
    {
        _event_loop->ExecuteInEventLoop(std::bind(&Connection::SwitchInLoop, shared_from_this(), context, connected_callback, after_receive_callback, before_close_callback, any_event_callback, connection_closed_callback));
    }
    // 释放连接---连接彻底释放
    void Release()
    {
        // 延时释放---对于释放任务
        _event_loop->QueueInLoop(std::bind(&Connection::ReleaseConnectionInLoop, shared_from_this()));
        // 释放连接属于连接管理任务，必须在对应的 EventLoop 中执行
        // 如果说在一个线程中，有多个连接就绪了，epoll_wait 返回的多个 active 数组
        // 但是这个时候 timerfd 发现其中的连接超时了，需要释放连接，这个时候连接都处于一个线程中
        // 就会导致直接释放了 active 数组中已经保存了的连接，后面访问就会出错
        // 为了避免 active Channel* 野指针，释放任务应该排队后延迟执行
        // BUG---本线程的释放任务和超时任务可能会导致野指针
        // _event_loop->ExecuteInEventLoop(std::bind(&Connection::ReleaseConnectionInLoop, shared_from_this()));
    }
    void InactiveRelease()
    {
        // 延时释放---对于释放任务
        _event_loop->QueueInLoop(std::bind(&Connection::InactiveReleaseInLoop, shared_from_this()));
    }
    // 关闭连接---释放前的准备工作
    void Shutdown()
    {
        _event_loop->ExecuteInEventLoop(std::bind(&Connection::ShutdownConnectionInLoop, shared_from_this()));
        // 设置协议上下文
    }
    void SetContext(const Any &any)
    {
        _context = any;
    }

    ~Connection() { LOG_INFO("DELETE: %p", this); }

private:
    uint64_t _id;                                 // 全局 ID -> Connection 和 TimerTask 共用，以标识唯一性
    bool _is_inactive_release;                    // 非活跃释放标志位(false-不释放--true-释放)
    bool _is_busy;                                // 是否处于
    Connection_Status _connection_status;         // 连接所处状态
    int _socket_fd;                               // 通信描述符
    Socket _socket;                               // 套接字---进行套接字的管理
    EventLoop *_event_loop;                       // 事件循环监控---进行事件的管理
    Channel _channel;                             // 连接事件---进行事件的操作
    Buffer _out_buffer;                           // 输出缓冲区---发送数据
    Buffer _in_buffer;                            // 输入缓冲区---接收数据
    Any _context;                                 // 协议上下文
    ConnectedCallback _connected_callback;        // 连接建立成功的回调
    AfterRecieveCallback _after_receive_callback; // 连接收到数据后的回调---收到数据后如何处理
    BeforeCloseCallback _before_close_callback;   // 连接关闭之前的回调---
    AnyEventCallback _any_event_callback;         // 连接中任意事件触发的回调---用于处理刷新连接等
    ConnectionClosed _connection_closed_callback; // 当前连接自己关闭的回调---用于给服务端释放资源
};

/// @brief 获取连接模块
class Acceptor
{
private:
    using AcceptedCallback = std::function<void(int)>;
    // 处理读事件
    void HandleRead()
    {
        // 1. 获取到通信描述符
        int new_fd = _socket.Accept();
        if (new_fd < 0)
            return;
        // 2. 往外层传递获取到的 fd
        if (_accepted_callback)
            _accepted_callback(new_fd);
    }

public:
    Acceptor(uint16_t port, EventLoop *event_loop)
        : _event_loop(event_loop)
    {
        // 1. 创建监听套接字---内部自己创建
        int b = _socket.BuildTcpMethod(port);
        if (!b)
            LOG_ERROR("BuildTcpMethod failed: %s", strerror(errno));
        else
        {
            // 2. 初始化新的 Channel
            _channel = std::make_unique<Channel>(*event_loop, _socket.GetFd());
            // 3. 设置回调
            _channel.get()->SetReadCallback(std::bind(&Acceptor::HandleRead, this));
        }
    }
    // 开启读事件---获取连接，不能直接放到构造函数中，因为其读事件还没有设置
    void EnableRead()
    {
        _channel.get()->EnableRead();
    }
    // 设置回调
    void SetAcceptCallback(const AcceptedCallback &callback)
    {
        _accepted_callback = callback;
    }
    ~Acceptor() {}

private:
    Socket _socket;                      // 监听套接字
    EventLoop *_event_loop;              // 事件循环监控
    std::unique_ptr<Channel> _channel;   // 监听的事件操作
    AcceptedCallback _accepted_callback; // 获取到新连接的回调
};

/// @brief 循环事件监控对应的线程模块
// 创建一个 EventLoop 的时候，必须是在线程中自己创建，不能由其它线程创建
// 线程和 EventLoop 一一对应
class EventLoopThread
{
private:
    // 实例化一个 EventLoop 并开启监控
    void Instantiation()
    {
        EventLoop event_loop;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            // 确保将指针赋值给成员变量的时候是线程安全的
            _event_loop = &event_loop;
            // 赋值完成可以唤醒所有条件变量
            _condition.notify_all();
        }
        // 必须开启---因为一个 LoopThread 和一个线程对应，如果不开启，那么线程执行完就退出了，LoopThread 对象就被释放了
        _event_loop->Start();
    }

public:
    EventLoopThread()
        : _event_loop(nullptr),
          _thread(&EventLoopThread::Instantiation, this)
    {
    }
    // 获取到 EventLoop 本体
    EventLoop *GetRaw()
    {
        EventLoop *event_loop;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            // 如果 EventLoop 指针为空，就会一直阻塞
            _condition.wait(lock, [this]
                            { return _event_loop != nullptr; });
            event_loop = _event_loop;
        }
        return event_loop;
    }
    ~EventLoopThread() {}

private:
    EventLoop *_event_loop;             // 循环监控本体
    std::thread _thread;                // 线程
    std::mutex _mutex;                  // 互斥锁
    std::condition_variable _condition; // 条件变量
};

/// @brief 循环事件监控线程池模块
// 线程池中线程数量为零的时候，主线程即提供获取连接的服务，又提供和客户端通信的服务
class EventLoopThreadPool
{
public:
    EventLoopThreadPool(EventLoop *base_event_loop)
        : _pool_count(0),
          _index(0),
          _base_event_loops(base_event_loop)
    {
    }
    // 设置线程池个数
    void SetPoolCount(int count)
    {
        _pool_count = count;
    }
    void Polling()
    {
        if (_pool_count > 0)
        {
            _threads.resize(_pool_count);
            _event_loops.resize(_pool_count);
            for (int i = 0; i < _pool_count; i++)
            {
                // 创建 EventLoopThread 的时候，是线程安全的
                _threads[i] = new EventLoopThread();
                // 使用 EventLoopThread 中的 GetRaw 的时候，是线程安全的
                _event_loops[i] = _threads[i]->GetRaw();
            }
        }
    }
    // 获取到线程池中下一个线程
    EventLoop *GetNextEventLoop()
    {
        if (_pool_count == 0)
        {
            return _base_event_loops;
        }
        _index = (_index + 1) % _pool_count;
        return _event_loops[_index];
    }
    ~EventLoopThreadPool() {}

private:
    int _pool_count;                         // 线程池线程个数
    int _index;                              // 线程索引
    EventLoop *_base_event_loops;            // 主线程
    std::vector<EventLoop *> _event_loops;   // 事件循环池
    std::vector<EventLoopThread *> _threads; // 线程池
};

/// @brief 服务器模块---整合上面的模块到一起，快速搭建服务器
static std::atomic<uint64_t> _global_timer_id{1000000}; // 服务器任务 ID
class TcpServer
{
private:
    using ConnectedCallback = std::function<void(Connection::ConnectionShared)>;
    using AfterRecieveCallback = std::function<void(Connection::ConnectionShared, Buffer *)>;
    using BeforeCloseCallback = std::function<void(Connection::ConnectionShared)>;
    using AnyEventCallback = std::function<void(Connection::ConnectionShared)>;
    using ConnectionClosed = std::function<void(Connection::ConnectionShared)>;

    ConnectedCallback _connected_callback;
    AfterRecieveCallback _after_receive_callback;
    BeforeCloseCallback _before_close_callback;
    AnyEventCallback _any_event_callback;

    // 移除对客户端连接的管理---移除的是 ConnectionShared，如果引用计数减到零，会自动释放对象
    void HandleConnectionClosed(Connection::ConnectionShared connection, uint64_t id)
    {
        auto it = _connections_maps.find(id);
        if (it == _connections_maps.end())
            return;
        LOG_INFO("%p closed", connection.get());
        _connections_maps.erase(it);
    }
    // 统一放到 ExecuteInEventLoop 中执行
    void HandleConnectionClosedInLoop(Connection::ConnectionShared connection, uint64_t id)
    {
        _base_event_loop.ExecuteInEventLoop(std::bind(&TcpServer::HandleConnectionClosed, this, connection, id));
    }
    // 处理读事件就绪---获取连接由Acceptor模块负责，所以获取到的连接就是客户端通信的套接字
    void HandleRead(int fd)
    {
        uint64_t conn_id = _id.fetch_add(1);
        Connection::ConnectionShared client_connection = std::make_shared<Connection>(conn_id, fd, _event_loop_thread_pool.GetNextEventLoop());
        client_connection->SetConnectedCallback(_connected_callback);
        client_connection->SetAfterRecieveCallback(_after_receive_callback);
        client_connection->SetBeforeCloseCallback(_before_close_callback);
        client_connection->SetAnyEventCallback(_any_event_callback);
        client_connection->SetConnectionClosedCallback(std::bind(&TcpServer::HandleConnectionClosedInLoop, this, std::placeholders::_1, conn_id));
        _connections_maps[conn_id] = client_connection;
        if (_is_enable_inactive_release)
        {
            client_connection->EnableInactiveRelease(_timeout);
        }
        client_connection->Established();
    }
    void HandleWorkerTask()
    {
    }
    // 添加定时器
    void AddTimerTask(/*EventLoop *event_loop,*/ uint32_t triggered_time, const TaskFunction &callback)
    {
        // ID 应该和标识符分离开
        uint64_t timer_id = _global_timer_id.fetch_add(1);
        _base_event_loop.AddTimerTask(timer_id, triggered_time, callback);
    }

public:
    TcpServer(uint32_t port)
        : _id(0),
          _timeout(0),
          _is_enable_inactive_release(false),
          _acceptor(port, &_base_event_loop),
          _event_loop_thread_pool(&_base_event_loop)
    {
        // 1. 设置获取到新连接的回调
        _acceptor.SetAcceptCallback(std::bind(&TcpServer::HandleRead, this, std::placeholders::_1));
        // 2. 开启对监听套接字的可读事件监听
        _acceptor.EnableRead();
    }
    void SetThreadPoolCount(int count)
    {
        _event_loop_thread_pool.SetPoolCount(count);
    }
    void SetConnectedCallback(const ConnectedCallback &callback)
    {
        _connected_callback = callback;
    }
    void SetAfterReceiveCallback(const AfterRecieveCallback &callback)
    {
        _after_receive_callback = callback;
    }
    void SetBeforeCloseCallback(const BeforeCloseCallback &callback)
    {
        _before_close_callback = callback;
    }
    void SetAnyEventCallback(const AnyEventCallback &callback)
    {
        _any_event_callback = callback;
    }
    void EnableInactiveRelease(uint32_t triggered_time)
    {
        // 1. 设置标志位
        _is_enable_inactive_release = true;
        // 2. 设置定时器时长
        _timeout = triggered_time;
    }
    void DisableInactiveRelease()
    {
        // 1. 修改标志位
        _is_enable_inactive_release = false;
        // 2. 清除定时器
        _timeout = 0;
    }
    uint32_t GetTimeOut()
    {
        return _timeout;
    }
    void AddTimer(uint32_t triggered_time, const TaskFunction &callback)
    {
        _base_event_loop.ExecuteInEventLoop(std::bind(&TcpServer::AddTimerTask, this, triggered_time, callback));
    }
    void Launch()
    {
        // 1. 池化事件循环线程池
        _event_loop_thread_pool.Polling();
        // 2. 启动主线程---主线程不会自动启动，从属线程会通过 EventLoopThreadPool 中的 Polling 在 new EventLoopThread 的时候自动启动
        _base_event_loop.Start();
    }
    ~TcpServer() {}

private:
    std::atomic<uint64_t> _id;                                               // 标识符(自增长)
    uint32_t _timeout;                                                       // 定时器时长
    bool _is_enable_inactive_release;                                        // 是否启动非活跃连接释放标志位
    EventLoop _base_event_loop;                                              // 主线程
    Acceptor _acceptor;                                                      // 获取连接
    EventLoopThreadPool _event_loop_thread_pool;                             // 事件循环线程池
    std::unordered_map<int, Connection::ConnectionShared> _connections_maps; // 管理对象
};
