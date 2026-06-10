# OneThreadOneLoopProject

OneThreadOneLoopProject 是一个使用 C++ 实现的 Linux 高并发网络服务器练习项目。项目从底层 Socket 封装开始，逐步实现 Buffer、Channel、Poller、EventLoop、定时器、连接管理、线程池、TCP Server，并在此基础上封装了一个简易 HTTP Server。

这个项目的重点不是调用现成网络库，而是拆解 Reactor 网络模型中各个模块的职责：如何把 fd 事件注册到 epoll，如何用一个线程维护一个事件循环，如何跨线程投递任务，如何管理连接生命周期，以及如何在 TCP 层之上解析 HTTP 请求并完成路由分发。

## 主要功能

- 基于 epoll 的 IO 事件监听与分发。
- One Thread One Loop 模型：每个 EventLoop 绑定一个线程，线程内串行处理 IO 与任务队列。
- Socket 封装：包含创建、绑定、监听、连接、accept、send、recv、非阻塞、地址复用等操作。
- Buffer 封装：支持读写偏移、自动扩容、数据搬移、按行读取等基础能力。
- Channel 封装：将 fd 与可读、可写、关闭、错误等事件回调绑定。
- Poller 封装：对 epoll_ctl / epoll_wait 做统一管理。
- eventfd 唤醒机制：支持其他线程向 EventLoop 安全投递任务。
- timerfd + 时间轮：支持定时任务、连接超时释放和定时任务刷新。
- Connection 生命周期管理：使用 shared_ptr 管理连接对象，处理建立、通信、关闭、释放等状态。
- TCP Server：封装 Acceptor、EventLoopThreadPool、连接表和用户回调。
- HTTP Server：支持 HTTP 请求解析、请求体读取、响应构建、长连接、静态资源返回和业务路由。
- 多 HTTP 方法路由：提供 GET、HEAD、OPTIONS、TRACE、PUT、DELETE、POST、PATCH、CONNECT 等路由注册接口。
- 业务线程池：HTTP 业务回调不直接阻塞 EventLoop，而是提交到 WorkerThreadPool 执行。
- 静态资源服务：支持从 `www/` 目录返回 HTML 等静态文件，并根据扩展名设置 MIME 类型。
- 示例客户端：提供多个 client demo，用于验证长连接、请求体、并发请求、慢请求和粘包场景。

## 技术栈

- C++11
- Linux Socket API
- epoll
- eventfd
- timerfd
- pthread / std::thread
- make / g++

## 项目结构

```text
.
├── server.hpp          # Reactor/TCP 服务器核心：Buffer、Socket、Channel、Poller、EventLoop、Connection、TcpServer
├── http.hpp            # HTTP 层：工具函数、Request、Response、Context、WorkerThreadPool、HttpServer
├── http_server.cc      # HTTP Server 示例入口，注册路由并启动服务
├── client01.cc         # 基础长连接请求测试
├── client02.cc         # 长时间保持连接测试
├── client03.cc         # 请求体长度与分段接收测试
├── client04.cc         # 请求体截断/剩余数据处理测试
├── client05.cc         # 多进程并发请求与慢请求测试
├── client06.cc         # 单连接连续发送多个 HTTP 请求测试
├── test.cc             # Buffer 和日志宏的简单验证
├── makefile            # 构建脚本
├── www/                # 静态资源目录
└── test/               # 另一份 HTTP Server 测试副本
```

## 核心架构

项目可以分为三层：

```text
HTTP Server
  ├── Request / Response / Context
  ├── 路由表与静态资源处理
  └── WorkerThreadPool

TCP Server
  ├── TcpServer
  ├── Acceptor
  ├── Connection
  └── EventLoopThreadPool

Reactor 基础设施
  ├── EventLoop
  ├── Poller / Channel
  ├── Socket / Buffer
  └── timerfd / eventfd / 时间轮
```

底层 Reactor 负责处理 IO 事件，中间层 TcpServer 管理连接，上层 HttpServer 负责协议解析和业务分发。HTTP 业务处理被提交到业务线程池执行，避免慢业务直接占用 EventLoop 线程。

## 实现特点

### 1. 从零实现 Reactor 网络模型

项目没有直接使用 muduo、libevent 或 boost.asio，而是自己封装了 Reactor 模型中的关键组件。`Channel` 负责描述 fd 关心的事件和回调，`Poller` 负责 epoll 事件监听，`EventLoop` 负责事件分发和任务执行，整体结构接近常见高性能网络库的核心设计。

### 2. 明确实践 One Thread One Loop 思想

`EventLoop` 记录所属线程 ID。当前线程内的任务可以直接执行，跨线程任务则会进入任务队列，并通过 `eventfd` 唤醒目标事件循环。这样可以把线程安全问题收敛到 EventLoop 内部，减少连接对象被多个线程同时操作的风险。

### 3. 连接生命周期

`Connection` 使用 `shared_from_this()` 保护自身生命周期，并区分 CONNECTING、CONNECTED、DISCONNECTING、DISCONNECTED 等状态。连接关闭、发送缓冲区未清空、非活跃释放、用户回调触发等场景都有对应处理逻辑，说明项目不只是完成 accept/read/write 的最小 Demo。

### 4. 使用 timerfd 和时间轮管理超时

项目通过 `timerfd` 驱动时间轮，支持添加、刷新、取消和移除定时任务。TCP 连接可以启用非活跃释放，连接有事件发生时刷新定时任务，长时间无事件时自动关闭。这是网络服务器里比较实用的一块能力。

### 5. HTTP 层请求处理链路

`http.hpp` 中实现了请求行解析、Header 解析、Content-Length 请求体读取、URL 编解码、查询参数解析、长连接判断、响应构建、状态码描述、MIME 类型判断和静态资源返回。它不是只返回固定字符串，而是具备一个小型 HTTP 框架的雏形。

### 6. 区分 IO 线程和业务线程

HTTP 业务回调会被封装成任务提交到 `WorkerThreadPool`。像 `/slow` 这种慢请求不会直接睡在 EventLoop 中，这一点能体现对网络服务器吞吐和响应性的考虑。

### 7. 提供了多种测试客户端

多个 `client*.cc` 文件分别覆盖长连接、慢请求、并发连接、请求体长度异常、单连接连续请求等场景。虽然还不是自动化测试，但它们能看出开发过程中有针对性地验证过协议解析和连接管理问题。

## 当前不足

这个项目已经有比较完整的网络库雏形，但仍然更偏学习型实现，距离可直接用于生产还有不少工程化差距。

### 1. 代码组织过于集中

`server.hpp` 和 `http.hpp` 文件都比较大，多个类和实现集中在头文件中。这样方便学习时阅读和复制，但不利于长期维护、增量编译和模块边界管理。

### 2. 构建系统和平台说明较简单

当前只有基础 makefile，且依赖 Linux 的 epoll、eventfd、timerfd 等接口。项目本身并不支持 Windows 或 macOS。

### 3. HTTP 协议支持有限

目前主要依赖 `Content-Length` 处理请求体，没有完整支持 chunked transfer、multipart、Range、压缩、复杂 Header 语义等能力。路由匹配也主要是精确匹配，暂时没有路径参数或正则路由。

### 4. 错误处理和边界用例

代码中已经有一些 TODO 和 BUG 注释，例如解析失败后 Buffer 偏移回退、Channel 生命周期风险、响应头规范化等问题。这些点说明作者在实现过程中已经意识到风险，但还需要系统性修复和测试覆盖。

### 5. 业务线程池

WorkerThreadPool 当前可以异步执行业务任务，但任务队列容量、拒绝策略、优雅停止、任务超时、异常隔离等机制还比较基础。在高压场景下，慢任务堆积可能导致内存占用和响应延迟不可控。

### 6. 测试效率

`client01.cc` 到 `client06.cc` 覆盖了一些典型场景，但还不是可重复执行的自动化测试。

### 7. 日志和调试输出

当前日志宏足够用于调试，但日志级别、输出目标、格式化安全、开关配置等还不够完整。部分调试日志会打印原始请求或响应内容，高并发场景下会影响性能，也可能污染输出。

## 后续计划

- 将 `server.hpp` 和 `http.hpp` 拆分为更清晰的头文件与实现文件。
- 完善 makefile，明确 Linux 平台、g++ 版本和 pthread 链接要求。
- 补充自动化测试，重点覆盖 Buffer、HTTP 解析、长连接、超时释放和路由分发。
- 修复代码中已标注的 TODO / BUG，尤其是解析失败后的 Buffer 状态一致性和 Channel 生命周期问题。
- 为 WorkerThreadPool 增加队列上限、优雅停止和任务拒绝策略。
- 支持更丰富的 HTTP 能力，例如路径参数、正则路由、chunked body、Range 请求等。
- 增加压测记录，例如 QPS、并发连接数、慢请求隔离效果等指标。
- 将示例客户端整理成 scripts 或 tests，降低复现成本。

## 运行方式

项目依赖 Linux 网络 API，建议在 Linux 或 WSL 环境中运行。

```bash
make
./http_server
```

启动后默认监听 `8080` 端口，可以访问：

```bash
curl http://127.0.0.1:8080/
curl http://127.0.0.1:8080/hello
curl http://127.0.0.1:8080/search
curl http://127.0.0.1:8080/slow
```

也可以编译并运行示例客户端：

```bash
make client05
./client05
```

`/slow` 路由会模拟慢业务，用于观察业务线程池和 EventLoop 解耦后的表现。

## 项目收获

通过这个项目，我主要练习了高并发服务器的底层实现思路：从 Socket 编程出发，逐步封装 epoll 事件分发、跨线程任务投递、连接生命周期管理、定时器和 HTTP 协议解析。相比只会使用现成框架，这个项目让我更清楚网络库内部为什么需要 EventLoop、Channel、Buffer、定时器和线程池这些模块。

这个项目目前还不是成熟的通用 HTTP 框架，但它已经覆盖了 Reactor 服务器的主链路，也暴露出很多真实网络项目会遇到的工程问题。后续我会重点围绕模块拆分、协议完整性、测试覆盖和性能验证继续迭代。

### 项目之外的收获

在这个项目之前，我平时接触更多的是偏后端业务层的代码，对应用层协议本身接触并不多。这次在自己实现的 TCP 服务器之上，继续编写简单的 HTTP 应用层协议支持，让我重新理解了操作系统和计算机网络课程中 TCP/IP 分层模型的实际价值。

以前提到 TCP/IP，可能更多只是停留在“现实中网络通信一般使用 TCP/IP”这样的结论上。但真正动手实现后，我意识到每一层协议都不是孤立存在的。设计或使用某一层协议时，不能只盯着当前层本身，还需要考虑它如何依赖下一层提供的能力，又如何向上一层暴露更易用的抽象。网络编程中的很多设计，其实都来自这种层与层之间的协作关系。

这个项目也让我联想到在学校学过的 Spring Boot、Vue 等框架。以前使用这些框架时，更多关注的是“怎么用”；而在自己实现 HTTP Server 的过程中，我发现路由、请求解析、响应封装、静态资源、业务回调等逻辑，和成熟 Web 框架中的 router、controller、view、handler 等概念有很多相通之处。区别在于，成熟框架把这些复杂度封装得更完整、更稳定、更易用。

这也让我开始思考一个更大的问题：为什么 Spring Boot 这样的框架会出现在 Java 生态中，并被广泛使用，而 C++ 并没有在同样场景下形成类似地位的主流产品？这个问题不能简单归结为语言优劣，而要结合生态、业务场景、开发效率、产品定位和语言特性一起看。并不是一门语言必须覆盖所有业务场景，而是不同语言会在不同领域形成自己的优势。Java 在企业级后端服务开发中有成熟生态和工程规范，因此诞生并发展出 Spring Boot 这样的框架；而 C++ 更常出现在对性能、资源控制和底层能力要求更高的场景中。

那么，自己手写这样一个服务器有什么意义呢？我觉得不能只从最终结果来判断学习价值。它的质量和鲁棒性当然不能和成熟框架或优秀开源项目相比，但它让我把网络服务器从底层 IO 到应用层协议的过程真正走了一遍。很多原本抽象的概念，比如事件循环、连接管理、路由分发、长连接、静态资源返回，在动手实现后变得具体了。

这种收获不一定会立刻表现为某个直接结果，却会内化成理解问题和解决问题的能力。以后再使用类似框架或排查网络问题时，我不会只把它们当作黑盒；如果未来真的需要实现一个轻量级服务框架或定制化网络模块，我也会更有底气。

无论怎样，在学习的路上，永远相信自己。
