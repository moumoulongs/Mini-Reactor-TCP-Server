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

const int PORT = 8080;
const int BACKLOG = 8;
const int BUFFER_SIZE = 4096;
const int MAX_EVENTS = 64; // epoll 最大事件数

class EpollServer {
public:
    // 构造函数
    EpollServer(int port = PORT) 
        : _port(port), 
          _listen_fd(-1), 
          _epoll_fd(-1), 
          _is_running(false) {
        setupSignalHandler();
    }
    
    // 析构函数：确保资源释放
    ~EpollServer() {
        shutdown();
    }
    
    // 初始化服务器：创建 socket、设置 SO_REUSEADDR、bind、listen
    bool init() {
        // 1. 创建 socket
        _listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (_listen_fd < 0) {
            perror("socket creation failed");
            return false;
        }
        std::cout << "Socket created successfully (fd: " << _listen_fd << ")" << std::endl;
        
        // 2. 设置 SO_REUSEADDR
        if (!setReuseAddr()) {
            close(_listen_fd);
            _listen_fd = -1;
            return false;
        }
        
        // 3. bind 到 0.0.0.0:port
        if (!bindAddress()) {
            close(_listen_fd);
            _listen_fd = -1;
            return false;
        }
        
        // 4. listen
        if (!startListen()) {
            close(_listen_fd);
            _listen_fd = -1;
            return false;
        }

        // 5. 创建 epoll 实例
        _epoll_fd = epoll_create1(0);
        if (_epoll_fd < 0) {
            perror("epoll_create1 failed");
            close(_listen_fd);
            _listen_fd = -1;
            return false;
        }

        std::cout << "Epoll instance created successfully (fd: " << _epoll_fd << ")" << std::endl;

        // 6. 将监听 socket 添加到 epoll
        struct epoll_event ev;
        ev.events = EPOLLIN; // 可读事件
        ev.data.fd = _listen_fd;
        if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _listen_fd, &ev) < 0) {
            perror("epoll_ctl ADD listen_fd failed");
            close(_listen_fd);
            close(_epoll_fd);
            _listen_fd = -1;
            _epoll_fd = -1;
            return false;
        }
        std::cout << "Listening socket added to epoll" << std::endl;
        
        _is_running = true;
        return true;
    }
    
    // 启动服务器主循环
    void run() {
        if (!_is_running) {
            std::cerr << "Server not initialized. Call init() first." << std::endl;
            return;
        }
        
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "Server is running on 0.0.0.0:" << _port << std::endl;
        std::cout << "Waiting for client connections..." << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        
        struct epoll_event events[MAX_EVENTS];

        while(_is_running) {
            int nfds = epoll_wait(_epoll_fd, events, MAX_EVENTS, -1);
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
                if(fd == _listen_fd) {
                    handleNewConnection();
                } else if(events[i].events & EPOLLIN) { //socket 有可读事件
                    handleClientData(fd);
                }
            }
        }

    }
    
    // 停止服务器
    void shutdown() {
        _is_running = false;
        if (_listen_fd >= 0) {
            close(_listen_fd);
            _listen_fd = -1;
            std::cout << "Listen socket closed" << std::endl;
        }
        if (_epoll_fd >= 0) {
            close(_epoll_fd);
            _epoll_fd = -1;
            std::cout << "Server socket closed" << std::endl;
        }
        //关闭所有客户端连接
        for(auto& pair : _clients) {
            close(pair.first);
        }
        std::cout << "All client connections closed" << std::endl;
    }
    
    // 检查服务器是否在运行
    bool isRunning() const {
        return _is_running;
    }

private:
    // 设置 SO_REUSEADDR 选项
    bool setReuseAddr() {
        int optval = 1;
        if (setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
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
        server_addr.sin_port = htons(_port);
        server_addr.sin_addr.s_addr = INADDR_ANY;  // 0.0.0.0
        
        if (bind(_listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("bind failed");
            return false;
        }
        std::cout << "Bound to 0.0.0.0:" << _port << std::endl;
        return true;
    }
    
    // 开始监听
    bool startListen() {
        if (listen(_listen_fd, BACKLOG) < 0) {
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
        
        int client_fd = accept(_listen_fd, (struct sockaddr*)&client_addr, &client_addr_len);
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
        if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
            perror("epoll_ctl ADD client_fd failed");
            close(client_fd);
            return;
        }
        
        // 保存客户端信息
        _clients[client_fd] = {client_addr, ""};
    }
    
    // 处理客户端请求（回显服务器）
    void handleClientData(int fd) {
        char buffer[BUFFER_SIZE];
        ssize_t bytes_read;
        
        // 边缘触发模式需要循环读取，直到 EAGAIN
        while(true) {
            bytes_read = read(fd, buffer, sizeof(buffer-1));
            if(bytes_read > 0) {
                buffer[bytes_read] = '\0';
                std::cout << "Received from client (fd: " << fd << "): " << buffer;

                ssize_t bytes_write = write(fd, buffer, bytes_read);
                if(bytes_write < 0) {
                    perror("write to client failed");
                    closeConnection(fd);
                    break;
                }
                std::cout << "Echoed back to client (fd: " << fd << "): " << buffer;
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
            epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
            close(fd);
            _clients.erase(fd);
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
        struct sockaddr_in addr;
        std::string buffer;
    };

    int _port;
    int _listen_fd;
    int _epoll_fd;
    bool _is_running;
    std::map<int, ClientInfo> _clients;
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