#ifndef SIMPLEFILEHOST_SERVER_H
#define SIMPLEFILEHOST_SERVER_H

#include <string>
#include <functional>
#include <atomic>
#include <vector>
#include <mutex>


#include <openssl/ssl.h>


struct ServerOptions {
    int port = 0;
    std::string token;
    std::string mode;
    std::string path;
    std::string bind_address = "127.0.0.1";
    long long max_size = 0;
    bool public_access = false;
    std::string working_dir = ".";
    std::atomic<bool>* interrupted = nullptr;
    int socket_timeout_seconds = 30;
};

class SimpleHTTPServer {
public:
    SimpleHTTPServer(const ServerOptions& opt);
    ~SimpleHTTPServer();
    
    bool start();
    void stop();
    std::string host_url() const;
    
    std::function<void(const std::string&)> on_log;
    std::function<void()> on_client_done;

private:
    ServerOptions opts;
    int listen_fd = -1;
    int port = 0;
    std::atomic<bool> running{false};
    std::vector<int> client_sockets;
    std::mutex clients_mutex;
    
    void server_loop();
    void add_client_socket(int fd);
    void remove_client_socket(int fd);
    void close_all_client_sockets();
    bool set_socket_timeout(int fd, int seconds);


    SSL_CTX* ssl_ctx = nullptr;

};

#endif
