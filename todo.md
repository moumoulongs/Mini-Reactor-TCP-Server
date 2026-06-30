下面是一份你可以直接照着执行的 **Mini Reactor TCP Server 项目任务书**。

---

# Mini Reactor TCP Server 项目任务书

## 1. 项目目标

使用 C++ 实现一个基于 Reactor 模型的高并发 TCP Echo Server。

项目需要具备以下能力：

```text
1. 使用 epoll 实现 IO 多路复用
2. 使用 non-blocking socket
3. 使用线程池处理业务任务
4. 支持多个客户端并发连接
5. 实现 echo server：客户端发送什么，服务端返回什么
6. 支持基础压测
7. 形成可写入简历的 GitHub 项目
```

最终简历描述：

```text
基于 epoll 的高并发 Reactor 网络服务器
```

---

# 2. 技术栈要求

## 开发语言

```text
C++17
```

## 运行环境

```text
Linux / WSL / Ubuntu 虚拟机
```

## 构建工具

```text
CMake
```

## 需要使用的系统调用

```text
socket
bind
listen
accept
read
write
close
fcntl
epoll_create1
epoll_ctl
epoll_wait
setsockopt
```

## 建议工具

```text
g++
cmake
make
nc
telnet
tcpkali
```

---

# 3. 最终交付物

你需要完成以下交付物：

```text
1. 一个可运行的 TCP Echo Server
2. 完整 C++ 源码
3. CMakeLists.txt
4. README.md
5. 压测结果记录
6. 简历项目描述
```

---

# 4. 项目目录结构

最终项目目录建议如下：

```text
mini-reactor/
├── CMakeLists.txt
├── README.md
├── docs/
│   └── benchmark.md
├── include/
│   ├── TcpServer.h
│   ├── Epoller.h
│   ├── ThreadPool.h
│   └── SocketUtil.h
├── src/
│   ├── main.cpp
│   ├── TcpServer.cpp
│   ├── Epoller.cpp
│   ├── ThreadPool.cpp
│   └── SocketUtil.cpp
└── test/
    └── client.cpp
```

---

# 5. 开发阶段规划

整个项目分为 5 个阶段完成。

```text
阶段 1：阻塞式 TCP Echo Server
阶段 2：non-blocking + epoll Echo Server
阶段 3：加入线程池
阶段 4：模块化重构
阶段 5：测试、压测、README、简历整理
```

---

# 阶段 1：阻塞式 TCP Echo Server

## 目标

先用最简单的方式实现一个 TCP Echo Server，熟悉 TCP 服务端基本流程。

## 任务 1.1：创建项目

创建目录：

```bash
mkdir mini-reactor
cd mini-reactor
mkdir src include test docs
touch CMakeLists.txt README.md
touch src/main.cpp
```

## 任务 1.2：实现阻塞式服务端

在 `src/main.cpp` 中实现：

```text
1. 创建 socket
2. 设置 SO_REUSEADDR
3. bind 到 0.0.0.0:8080
4. listen
5. accept 客户端连接
6. read 客户端数据
7. write 原样返回
8. 客户端断开后 close
```

## 任务 1.3：编写 CMakeLists.txt

要求：

```text
1. 使用 C++17
2. 生成可执行文件 mini_reactor
3. 编译 src/main.cpp
```

## 任务 1.4：运行测试

编译：

```bash
mkdir build
cd build
cmake ..
make
```

启动服务端：

```bash
./mini_reactor
```

新开终端测试：

```bash
nc 127.0.0.1 8080
```

输入：

```text
hello
```

期望输出：

```text
hello
```

## 阶段 1 验收标准

```text
1. 服务端可以启动
2. nc 可以连接服务器
3. 输入 hello，服务器返回 hello
4. 客户端断开后服务器不崩溃
```

---

# 阶段 2：non-blocking + epoll Echo Server

## 目标

把阻塞式服务器改造成基于 epoll 的事件驱动服务器。

## 任务 2.1：实现 setNonBlocking 函数

在 `main.cpp` 中先实现工具函数：

```cpp
int setNonBlocking(int fd);
```

要求：

```text
1. 使用 fcntl 获取 fd 原 flag
2. 给 fd 添加 O_NONBLOCK
3. 出错时返回 -1
4. 成功时返回 0
```

## 任务 2.2：设置 listen_fd 为非阻塞

创建监听 socket 后调用：

```cpp
setNonBlocking(listen_fd);
```

## 任务 2.3：创建 epoll

要求：

```text
1. 使用 epoll_create1(0)
2. 将 listen_fd 加入 epoll
3. 监听 EPOLLIN 事件
```

## 任务 2.4：实现 epoll 主循环

主循环结构：

```text
1. 调用 epoll_wait 等待事件
2. 如果事件 fd 是 listen_fd，处理新连接
3. 如果事件 fd 是 client_fd，处理客户端数据
```

## 任务 2.5：处理新连接

实现逻辑：

```text
1. 循环 accept
2. accept 成功后设置 client_fd 为 non-blocking
3. 将 client_fd 加入 epoll
4. accept 返回 -1 且 errno 是 EAGAIN/EWOULDBLOCK 时，退出 accept 循环
5. 其他错误打印错误信息
```

## 任务 2.6：处理客户端读事件

实现逻辑：

```text
1. 循环 read
2. n > 0：将数据 write 回客户端
3. n == 0：客户端关闭连接，close fd
4. n < 0：
   - errno 是 EAGAIN/EWOULDBLOCK：说明数据读完，退出循环
   - 其他错误：关闭连接
```

## 任务 2.7：关闭连接

实现函数：

```cpp
void closeConnection(int epoll_fd, int client_fd);
```

要求：

```text
1. epoll_ctl 删除 client_fd
2. close client_fd
3. 打印连接关闭日志
```

## 阶段 2 验收标准

```text
1. 服务端使用 epoll_wait 处理事件
2. listen_fd 是 non-blocking
3. client_fd 是 non-blocking
4. 多个 nc 客户端可以同时连接
5. 每个客户端都可以正常 echo
6. 客户端断开后服务端不崩溃
```

测试方式：

```bash
nc 127.0.0.1 8080
```

多开几个终端同时连接测试。

---

# 阶段 3：加入线程池

## 目标

引入线程池，让 Reactor 主线程负责 IO 事件监听和分发，worker 线程处理 echo 任务。

## 任务 3.1：新增 ThreadPool 文件

创建文件：

```bash
touch include/ThreadPool.h
touch src/ThreadPool.cpp
```

## 任务 3.2：实现 ThreadPool 类

接口要求：

```cpp
class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount);
    ~ThreadPool();

    void submit(std::function<void()> task);

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_;
};
```

## 任务 3.3：实现构造函数

要求：

```text
1. 根据 threadCount 创建 worker 线程
2. 每个 worker 循环等待任务
3. 使用 condition_variable 阻塞等待任务
4. 取出任务后执行
```

## 任务 3.4：实现 submit

要求：

```text
1. 加锁
2. 将任务加入队列
3. 解锁
4. 通知一个 worker
```

## 任务 3.5：实现析构函数

要求：

```text
1. 设置 stop_ = true
2. notify_all
3. join 所有 worker 线程
```

## 任务 3.6：接入 echo server

修改客户端读事件处理逻辑：

原来：

```text
read 后直接 write
```

改成：

```text
read 后把 echo 任务提交给线程池
worker 线程执行 write
```

伪代码：

```cpp
std::string data(buffer, n);

threadPool.submit([client_fd, data]() {
    write(client_fd, data.data(), data.size());
});
```

## 任务 3.7：忽略 SIGPIPE

在 `main.cpp` 中添加：

```cpp
#include <signal.h>
```

启动时调用：

```cpp
signal(SIGPIPE, SIG_IGN);
```

避免客户端断开后服务端写入导致进程退出。

## 阶段 3 验收标准

```text
1. 服务端可以正常启动
2. 主线程只负责 epoll_wait、accept、read、任务提交
3. echo write 在 worker 线程中执行
4. 多个客户端并发连接仍然可以正常 echo
5. 客户端异常断开后服务端不崩溃
```

---

# 阶段 4：模块化重构

## 目标

将单文件代码拆成多个模块，形成一个像样的 C++ 项目。

---

## 任务 4.1：封装 SocketUtil

创建：

```bash
touch include/SocketUtil.h
touch src/SocketUtil.cpp
```

需要提供：

```cpp
int setNonBlocking(int fd);
int createListenSocket(int port);
```

`createListenSocket` 要完成：

```text
1. socket
2. setsockopt SO_REUSEADDR
3. bind
4. listen
5. setNonBlocking
6. 返回 listen_fd
```

---

## 任务 4.2：封装 Epoller

创建：

```bash
touch include/Epoller.h
touch src/Epoller.cpp
```

类接口：

```cpp
class Epoller {
public:
    Epoller();
    ~Epoller();

    void addFd(int fd, uint32_t events);
    void modFd(int fd, uint32_t events);
    void delFd(int fd);
    int wait(epoll_event* events, int maxEvents, int timeout);

private:
    int epoll_fd_;
};
```

要求：

```text
1. 构造函数中 epoll_create1
2. 析构函数中 close epoll_fd_
3. addFd 封装 EPOLL_CTL_ADD
4. modFd 封装 EPOLL_CTL_MOD
5. delFd 封装 EPOLL_CTL_DEL
6. wait 封装 epoll_wait
```

---

## 任务 4.3：封装 TcpServer

创建：

```bash
touch include/TcpServer.h
touch src/TcpServer.cpp
```

类接口：

```cpp
class TcpServer {
public:
    TcpServer(int port, int threadCount);
    ~TcpServer();

    void start();

private:
    void handleAccept();
    void handleRead(int clientFd);
    void closeConnection(int clientFd);

private:
    int port_;
    int listen_fd_;
    Epoller epoller_;
    ThreadPool thread_pool_;
};
```

要求：

```text
1. 构造函数中创建 listen_fd
2. 将 listen_fd 加入 epoll
3. start 中执行 epoll_wait 主循环
4. handleAccept 负责接收新连接
5. handleRead 负责读取数据并提交线程池
6. closeConnection 负责删除 epoll 事件并关闭 fd
```

---

## 任务 4.4：整理 main.cpp

`main.cpp` 最终只保留：

```text
1. 解析端口和线程数
2. 忽略 SIGPIPE
3. 创建 TcpServer
4. 调用 server.start()
```

示例使用方式：

```bash
./mini_reactor 8080 4
```

其中：

```text
8080 是端口
4 是线程池线程数量
```

---

## 任务 4.5：更新 CMakeLists.txt

要求编译所有 `.cpp` 文件：

```text
src/main.cpp
src/TcpServer.cpp
src/Epoller.cpp
src/ThreadPool.cpp
src/SocketUtil.cpp
```

并包含头文件目录：

```text
include/
```

---

## 阶段 4 验收标准

```text
1. 项目可以通过 CMake 正常编译
2. main.cpp 不再堆满业务逻辑
3. TcpServer、Epoller、ThreadPool、SocketUtil 职责清晰
4. 服务端功能与阶段 3 一致
5. GitHub 仓库结构清晰
```

---

# 阶段 5：测试、压测与文档

## 目标

让项目达到可以放进简历和 GitHub 的程度。

---

## 任务 5.1：基础功能测试

测试服务端启动：

```bash
./mini_reactor 8080 4
```

测试单客户端：

```bash
nc 127.0.0.1 8080
```

输入：

```text
hello
```

期望返回：

```text
hello
```

测试多客户端：

```text
打开 5 个终端
每个终端都使用 nc 连接
每个客户端发送不同内容
检查是否都能正常 echo
```

---

## 任务 5.2：异常断开测试

测试场景：

```text
1. 客户端连接后直接 Ctrl+C
2. 客户端发送数据后立即断开
3. 多个客户端频繁连接和断开
```

验收：

```text
服务端不能崩溃
服务端要打印连接关闭日志
```

---

## 任务 5.3：压测

安装 tcpkali：

```bash
sudo apt update
sudo apt install tcpkali
```

执行压测：

```bash
tcpkali -c 1000 -T 30s 127.0.0.1:8080
```

记录以下数据到 `docs/benchmark.md`：

```text
1. 测试环境
2. CPU / 内存
3. 操作系统
4. 线程池数量
5. 并发连接数
6. 测试持续时间
7. QPS / 吞吐量
8. 是否出现错误
```

---

## 任务 5.4：编写 README.md

README 必须包含：

```text
1. 项目简介
2. 技术栈
3. 架构设计
4. 模块说明
5. 编译方式
6. 运行方式
7. 测试方式
8. 压测结果
9. 后续优化方向
```

README 模板：

````md
# Mini Reactor TCP Server

一个基于 C++17 实现的高并发 TCP Echo Server，采用 Reactor 模式、epoll IO 多路复用、non-blocking socket 和线程池，实现多客户端并发处理。

## Features

- 基于 epoll 实现 IO 多路复用
- 使用 non-blocking socket 避免阻塞
- 主线程负责连接管理和事件分发
- 线程池负责业务任务处理
- 支持 TCP echo 服务
- 支持高并发连接测试

## Architecture

```text
Client
  |
  v
TcpServer
  |
  v
Epoller
  |
  +-- listen_fd readable -> accept new connection
  |
  +-- client_fd readable -> read data -> submit task
                                  |
                                  v
                              ThreadPool
                                  |
                                  v
                              write response
````

## Build

```bash
mkdir build
cd build
cmake ..
make
```

## Run

```bash
./mini_reactor 8080 4
```

## Test

```bash
nc 127.0.0.1 8080
```

## Benchmark

```bash
tcpkali -c 1000 -T 30s 127.0.0.1:8080
```

## Future Work

* 支持 EPOLLET 边缘触发模式
* 增加 Connection 对象
* 增加输入缓冲区和输出缓冲区
* 使用 EPOLLOUT 处理非阻塞写
* 支持 HTTP 协议解析
* 增加定时器清理空闲连接
* 增加异步日志系统

````

---

# 6. 每日任务安排

如果你想按 4 周完成，可以这样排。

---

## 第 1 周：TCP 基础恢复

### Day 1

```text
学习 socket、bind、listen、accept 流程
创建项目目录
写出阻塞式 TCP Server 框架
````

### Day 2

```text
实现阻塞式 echo
使用 nc 测试
处理客户端断开
```

### Day 3

```text
学习 fcntl 和 O_NONBLOCK
实现 setNonBlocking
理解 EAGAIN / EWOULDBLOCK
```

### Day 4

```text
学习 epoll_create1、epoll_ctl、epoll_wait
写一个最小 epoll demo
```

### Day 5

```text
将阻塞 echo 改成 epoll echo
支持多个客户端同时连接
```

### Day 6

```text
完善错误处理
补充日志输出
整理阶段 1 和阶段 2 代码
```

### Day 7

```text
复盘 TCP 和 epoll 流程
画出服务端主循环流程图
```

---

## 第 2 周：Reactor 主体实现

### Day 8

```text
学习 Reactor 模型
区分 listen_fd 事件和 client_fd 事件
整理 handleAccept / handleRead 函数
```

### Day 9

```text
完善 handleAccept
循环 accept 到 EAGAIN
所有 client_fd 设置 non-blocking
```

### Day 10

```text
完善 handleRead
循环 read 到 EAGAIN
处理 n == 0
处理异常断开
```

### Day 11

```text
实现 closeConnection
确保关闭前从 epoll 删除 fd
```

### Day 12

```text
学习线程池基本结构
实现 ThreadPool.h
```

### Day 13

```text
实现 ThreadPool.cpp
完成 submit、worker loop、析构 join
```

### Day 14

```text
将线程池接入 echo server
测试多客户端并发 echo
```

---

## 第 3 周：工程化重构

### Day 15

```text
创建 include/ 和 src/ 模块文件
拆出 SocketUtil
```

### Day 16

```text
拆出 Epoller
测试 addFd、delFd、wait 是否正常
```

### Day 17

```text
拆出 TcpServer
将 accept/read/close 逻辑移动到类中
```

### Day 18

```text
整理 main.cpp
支持命令行参数：port、threadCount
```

### Day 19

```text
完善 CMakeLists.txt
确保 clean build 可以通过
```

### Day 20

```text
补充日志
例如：server start、new connection、close connection、read error
```

### Day 21

```text
完整回归测试
单客户端、多客户端、异常断开
```

---

## 第 4 周：压测与简历化

### Day 22

```text
安装 tcpkali
进行 100、500、1000 并发连接测试
```

### Day 23

```text
记录压测结果
整理 docs/benchmark.md
```

### Day 24

```text
编写 README.md
补充项目简介、架构、模块说明
```

### Day 25

```text
整理 GitHub 仓库
删除临时文件
补充 .gitignore
```

### Day 26

```text
写简历项目描述
准备面试讲解话术
```

### Day 27

```text
复盘项目中的关键问题：
1. 为什么使用 epoll
2. 为什么使用 non-blocking
3. 为什么 read 要循环
4. 为什么 accept 要循环
5. 为什么需要线程池
```

### Day 28

```text
完成最终验收
打 tag：v1.0
```

---

# 7. 功能验收清单

完成项目时逐项打勾。

```text
[ ] 可以通过 CMake 编译
[ ] 服务端可以指定端口启动
[ ] 服务端可以指定线程池线程数
[ ] listen_fd 设置为 non-blocking
[ ] client_fd 设置为 non-blocking
[ ] 使用 epoll_wait 监听事件
[ ] listen_fd 可读时可以 accept 新连接
[ ] accept 循环直到 EAGAIN
[ ] client_fd 可读时可以 read 数据
[ ] read 循环直到 EAGAIN
[ ] 客户端发送数据后可以收到 echo
[ ] 客户端断开后服务端可以清理连接
[ ] close 前从 epoll 删除 fd
[ ] 实现 ThreadPool
[ ] read 后可以 submit 任务到线程池
[ ] worker 线程可以执行 echo write
[ ] 忽略 SIGPIPE
[ ] 多客户端同时连接正常
[ ] 异常断开服务端不崩溃
[ ] README 完整
[ ] benchmark.md 完整
[ ] 简历描述完成
```

---

# 8. 代码质量要求

## 命名要求

```text
类名：大驼峰
例如：TcpServer, ThreadPool, Epoller

函数名：小驼峰
例如：handleAccept, handleRead, closeConnection

成员变量：下划线结尾
例如：listen_fd_, epoll_fd_, thread_pool_
```

## 错误处理要求

必须处理：

```text
socket 创建失败
bind 失败
listen 失败
epoll_create1 失败
epoll_ctl 失败
accept 失败
read 失败
write 失败
```

## 日志要求

至少打印：

```text
server started
new connection
client closed
read error
write error
```

---

# 9. 面试准备问题

你完成后必须能回答这些问题：

```text
1. 什么是 Reactor 模型？
2. epoll 相比 select/poll 的优势是什么？
3. LT 和 ET 有什么区别？
4. 为什么 socket 要设置成 non-blocking？
5. 为什么 accept 要循环？
6. 为什么 read 要循环？
7. EAGAIN / EWOULDBLOCK 是什么意思？
8. 线程池解决了什么问题？
9. 你的主线程和 worker 线程分别做什么？
10. 客户端断开时服务端怎么处理？
11. 为什么要忽略 SIGPIPE？
12. 你这个项目还有哪些不足？
```

---

# 10. 当前版本允许的简化

这个 M1–M2 阶段允许以下简化：

```text
1. 暂时不实现 HTTP
2. 暂时不实现定时器
3. 暂时不实现异步日志
4. 暂时不实现 Connection 类
5. 暂时不实现完整 output buffer
6. 暂时允许 worker 线程直接 write
7. 暂时使用 epoll LT 模式
```

但是你要知道这些是后续优化点，面试时可以主动说明：

```text
当前版本中 worker 线程直接 write 回客户端，存在非阻塞写不完整和多线程写同一 fd 的潜在问题。后续可以引入 Connection 对象和 output buffer，由主线程监听 EPOLLOUT 并统一完成写回，这样会更接近标准 Reactor 实现。
```

---

# 11. 最终简历版本

可以写成：

```text
Mini Reactor TCP Server｜C++ 高并发网络服务器

- 基于 C++17 实现高并发 TCP Echo Server，采用 Reactor 模型进行事件驱动式连接管理
- 使用 epoll 实现 IO 多路复用，配合 non-blocking socket 支持大量并发连接
- 封装 TcpServer、Epoller、ThreadPool、SocketUtil 等核心模块，实现连接接入、事件分发、任务处理解耦
- 使用线程池处理业务逻辑，降低频繁创建线程带来的性能开销
- 使用 tcpkali 对服务端进行并发压测，验证多连接场景下的稳定性
```

---

你就按这份任务书做。最关键的是：**先完成阶段 1 和阶段 2，不要一开始就追求完美架构。**
