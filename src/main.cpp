// src/main.cpp
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

const int PORT = 8080;
const int BACKLOG = 8;
const int BUFFER_SIZE = 4096;

class TcpServer {
public:
    // 构造函数
    TcpServer(int port = PORT) 
        : _port(port), 
          _listen_fd(-1), 
          _client_fd(-1), 
          _is_running(false) {
        setupSignalHandler();
    }
    
    // 析构函数：确保资源释放
    ~TcpServer() {
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
        
        while (_is_running) {
            // 5. accept 客户端连接
            if (!acceptClient()) {
                continue;  // 接受失败，继续等待下一个连接
            }
            
            // 6-7. 处理客户端请求（read + write 原样返回）
            handleClient();
            
            // 8. 客户端断开后 close
            closeClient();
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
        if (_client_fd >= 0) {
            close(_client_fd);
            _client_fd = -1;
            std::cout << "Client socket closed" << std::endl;
        }
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
    
    // 接受客户端连接
    bool acceptClient() {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        _client_fd = accept(_listen_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (_client_fd < 0) {
            perror("accept failed");
            return false;
        }
        
        // 打印客户端信息
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "\nClient connected: " << client_ip 
                  << ":" << ntohs(client_addr.sin_port) << std::endl;
        return true;
    }
    
    // 处理客户端请求（回显服务器）
    void handleClient() {
        if (_client_fd < 0) return;
        
        char buffer[BUFFER_SIZE];
        ssize_t bytes_read;
        
        while ((bytes_read = read(_client_fd, buffer, sizeof(buffer) - 1)) > 0) {
            // 6. read 客户端数据
            buffer[bytes_read] = '\0';
            std::cout << "  Received " << bytes_read << " bytes: " << buffer;
            
            // 7. write 原样返回（回显）
            ssize_t bytes_written = write(_client_fd, buffer, bytes_read);
            if (bytes_written < 0) {
                perror("  write failed");
                break;
            }
            std::cout << "  → Echoed " << bytes_written << " bytes back" << std::endl;
        }
        
        // 处理断开连接的情况
        if (bytes_read == 0) {
            std::cout << "Client disconnected (graceful)" << std::endl;
        } else if (bytes_read < 0) {
            perror("read error");
            std::cout << "Client disconnected (error)" << std::endl;
        }
    }
    
    // 关闭客户端连接
    void closeClient() {
        if (_client_fd >= 0) {
            close(_client_fd);
            _client_fd = -1;
            std::cout << "Connection closed" << std::endl;
            std::cout << "----------------------------------------" << std::endl;
            std::cout << "Waiting for next client..." << std::endl;
        }
    }
    
    // 设置信号处理器
    static void setupSignalHandler() {
        // 忽略 SIGPIPE，防止客户端断开时进程崩溃
        signal(SIGPIPE, SIG_IGN);
    }
    
private:
    int _port;
    int _listen_fd;
    int _client_fd;
    bool _is_running;
};

// ============== 主函数 ==============
int main() {
    // 创建服务器实例
    TcpServer server(PORT);
    
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