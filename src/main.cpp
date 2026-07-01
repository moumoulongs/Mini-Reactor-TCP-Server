// src/main.cpp
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <vector>
#include <fcntl.h>
#include <map>
#include "ThreadPool.h"

const int PORT = 8080;
const int BACKLOG = 8;
const int BUFFER_SIZE = 8;
const int MAX_EVENTS = 64; // epoll 最大事件数
const int THREAD_POOL_SIZE = 4; // 线程池大小

class EpollServer {
public:
    // 构造函数
    EpollServer(int port = PORT) 
        : port_(port), 
          listen_fd_(-1), 
          epoll_fd_(-1), 
          is_running_(false),
          thread_pool_(THREAD_POOL_SIZE) {
        setupSignalHandler();
    }
    
    // 析构函数：确保资源释放
    ~EpollServer() {
        shutdown();
    }
    
    // 初始化服务器：创建 socket、设置 SO_REUSEADDR、bind、listen
    bool init() {
        // 1. 创建 socket
        listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) {
            perror("socket creation failed");
            return false;
        }
        std::cout << "Socket created successfully (fd: " << listen_fd_ << ")" << std::endl;
        
        // 2. 设置 SO_REUSEADDR
        if (!setReuseAddr()) {
            close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }
        
        // 3. bind 到 0.0.0.0:port
        if (!bindAddress()) {
            close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }
        
        // 4. listen
        if (!startListen()) {
            close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }

        // 5. 创建 epoll 实例
        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ < 0) {
            perror("epoll_create1 failed");
            close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }

        std::cout << "Epoll instance created successfully (fd: " << epoll_fd_ << ")" << std::endl;

        // 6. 将监听 socket 添加到 epoll
        struct epoll_event ev;
        ev.events = EPOLLIN; // 可读事件
        ev.data.fd = listen_fd_;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev) < 0) {
            perror("epoll_ctl ADD listen_fd failed");
            close(listen_fd_);
            close(epoll_fd_);
            listen_fd_ = -1;
            epoll_fd_ = -1;
            return false;
        }
        std::cout << "Listening socket added to epoll" << std::endl;
        
        is_running_ = true;
        return true;
    }
    
    // 启动服务器主循环
    void run() {
        if (!is_running_) {
            std::cerr << "Server not initialized. Call init() first." << std::endl;
            return;
        }
        
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "Server is running on 0.0.0.0:" << port_ << std::endl;
        std::cout << "Waiting for client connections..." << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        
        struct epoll_event events[MAX_EVENTS];

        while(is_running_) {
            int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);
            if (nfds < 0) {
                if (errno == EINTR) {
                    continue; // 被信号中断，继续等待
                }
                perror("epoll_wait failed");
                break;
            }

            for (int i = 0; i < nfds; ++i) {
                int fd = events[i].data.fd;

                // 错误或挂起事件
                if(events[i].events & (EPOLLERR | EPOLLHUP)) {
                    std::cerr << "Epoll error on fd: " << fd << std::endl;
                    closeConnection(fd);
                    continue;
                }

                // 新的客户端连接
                if(fd == listen_fd_) {
                    handleNewConnection();
                } else if(events[i].events & EPOLLIN) { //socket 有可读事件
                    handleClientData(fd);
                }
            }
        }

    }
    
    // 停止服务器
    void shutdown() {
        is_running_ = false;
        if (listen_fd_ >= 0) {
            close(listen_fd_);
            listen_fd_ = -1;
            std::cout << "Listen socket closed" << std::endl;
        }
        if (epoll_fd_ >= 0) {
            close(epoll_fd_);
            epoll_fd_ = -1;
            std::cout << "Server socket closed" << std::endl;
        }
        //关闭所有客户端连接
        for(auto& pair : clients_) {
            close(pair.first);
        }
        std::cout << "All client connections closed" << std::endl;
    }
    
    // 检查服务器是否在运行
    bool isRunning() const {
        return is_running_;
    }

private:
    // 设置 SO_REUSEADDR 选项
    bool setReuseAddr() {
        int optval = 1;
        if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
            perror("setsockopt SO_REUSEADDR failed");
            return false;
        }
        std::cout << "SO_REUSEADDR set successfully" << std::endl;
        return true;
    }
    
    // 绑定地址和端口
    bool bindAddress() {
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port_);
        server_addr.sin_addr.s_addr = INADDR_ANY;  // 0.0.0.0
        
        if (bind(listen_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("bind failed");
            return false;
        }
        std::cout << "Bound to 0.0.0.0:" << port_ << std::endl;
        return true;
    }
    
    // 开始监听
    bool startListen() {
        if (listen(listen_fd_, BACKLOG) < 0) {
            perror("listen failed");
            return false;
        }
        std::cout << "Listening (backlog: " << BACKLOG << ")" << std::endl;
        return true;
    }
    
    //处理新连接
    void handleNewConnection() {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        int client_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            perror("accept failed");
            return;
        }

        int flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK); // 设置为非阻塞模式
        
        // 打印客户端信息
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "\nClient connected: " << client_ip 
                  << ":" << ntohs(client_addr.sin_port) 
                  << " (fd: " << client_fd << ")" << std::endl;

        // 将客户端 socket 添加到 epoll
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET; // 边缘触发模式
        ev.data.fd = client_fd;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
            perror("epoll_ctl ADD client_fd failed");
            close(client_fd);
            return;
        }
        
        // 保存客户端信息
        clients_[client_fd] = {client_addr, "", false};
    }
    
    // 处理客户端请求（回显服务器）
    void handleClientData(int fd) {
        char buffer[BUFFER_SIZE];
        ssize_t bytes_read;
        
        // 边缘触发模式需要循环读取，直到 EAGAIN
        while(true) {
            bytes_read = read(fd, buffer, sizeof(buffer)-1);
            if(bytes_read > 0) {
                buffer[bytes_read] = '\0';
                clients_[fd].buffer.append(buffer, bytes_read);
                std::cout << "Received from client (fd: " << fd << "): " << buffer;
                
                // 将处理任务提交给线程池
                if(clients_[fd].processing == false) {
                    clients_[fd].processing = true;
                    thread_pool_.submit([this, fd]() {
                        std::string& data = clients_[fd].buffer;
                        // 模拟处理数据（这里直接回显）
                        ssize_t bytes_written = write(fd, data.c_str(), data.size());
                        if(bytes_written < 0) {
                            perror("write to client failed");
                            closeConnection(fd);
                        } else {
                            std::cout << "Echoed back to client (fd: " << fd << "): " << data;
                            data.clear(); // 清空缓冲区
                        }
                        clients_[fd].processing = false;
                    });
                }
                
            }
            else if(bytes_read == 0) {
                std::cout << "Client (fd: " << fd << ") disconnected" << std::endl;
                closeConnection(fd);
                break;
            }
            else {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 没有更多数据可读
                    break;
                } else {
                    perror("read from client failed");
                    closeConnection(fd);
                    break;
                }
            }
        }
    }
    
    // 关闭连接并清理
    void closeConnection(int fd) {
        // 从 epoll 中移除
        if (fd >= 0) {
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
            close(fd);
            clients_.erase(fd);
            std::cout << "  Closed fd " << fd << std::endl;
        }
    }

    
    // 设置信号处理器
    static void setupSignalHandler() {
        // 忽略 SIGPIPE，防止客户端断开时进程崩溃
        signal(SIGPIPE, SIG_IGN);
    }
    
private:
    struct ClientInfo {
        struct sockaddr_in addr;        // 客户端地址
        std::string buffer;          // 缓冲区
        bool processing;
    };

    int port_;
    int listen_fd_;
    int epoll_fd_;
    bool is_running_;
    std::map<int, ClientInfo> clients_;
    ThreadPool thread_pool_;
    
};

// ============== 主函数 ==============
int main() {
    // 创建服务器实例
    EpollServer server(PORT);
    
    
    // 初始化服务器
    if (!server.init()) {
        std::cerr << "Failed to initialize server. Exiting." << std::endl;
        return 1;
    }
    
    // 运行服务器主循环
    server.run();
    
    // 正常退出
    std::cout << "Server shutdown complete" << std::endl;
    return 0;
}