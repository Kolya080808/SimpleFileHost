#include "server.h"
#include "../utils/server_utils.h"
#include "../utils/network_utils.h"
#include "client_handler.h"
#include "../utils/utils.h"
#include <thread>
#include <cstring>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <algorithm>

bool SimpleHTTPServer::set_socket_timeout(int fd, int seconds) {
    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
    
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        vlog("Failed to set send timeout on socket");
        return false;
    }
    
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        vlog("Failed to set receive timeout on socket");
        return false;
    }
    
    return true;
}

SimpleHTTPServer::SimpleHTTPServer(const ServerOptions &opt)
    : opts(opt), port(opt.port) {}

SimpleHTTPServer::~SimpleHTTPServer() { stop(); }

void SimpleHTTPServer::add_client_socket(int fd) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    client_sockets.push_back(fd);
}

void SimpleHTTPServer::remove_client_socket(int fd) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    auto it = std::find(client_sockets.begin(), client_sockets.end(), fd);
    if (it != client_sockets.end()) {
        client_sockets.erase(it);
    }
}

void SimpleHTTPServer::close_all_client_sockets() {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (int fd : client_sockets) {
        close(fd);
    }
    client_sockets.clear();
}

bool SimpleHTTPServer::start() {
    vlog("Starting server...");
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        vlog("Socket creation failed");
        return false;
    }
    
    int optv = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optv, sizeof(optv));

    fcntl(listen_fd, F_SETFL, O_NONBLOCK);
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    std::string bind_ip = opts.bind_address.empty() ? get_default_bind_address() : opts.bind_address;

    if (inet_pton(AF_INET, bind_ip.c_str(), &addr.sin_addr) <= 0) {
        vlog("Invalid bind address: " + bind_ip);
        return false;
    }

    if (bind(listen_fd, (sockaddr *)&addr, sizeof(addr)) < 0) {
        vlog("Bind failed on address: " + bind_ip);
        return false;
    }

    if (port == 0) {
        socklen_t len = sizeof(addr);
        if (getsockname(listen_fd, (sockaddr *)&addr, &len) == 0)
            port = ntohs(addr.sin_port);
    }

    if (listen(listen_fd, 5) < 0) {
        vlog("Listen failed");
        return false;
    }

    running = true;
    std::thread(&SimpleHTTPServer::server_loop, this).detach();

    if (on_log)
        on_log("Server started on " + bind_ip + ":" + std::to_string(port));

    opts.bind_address = bind_ip;
    vlog("Server started successfully on port " + std::to_string(port));
    return true;
}

void SimpleHTTPServer::stop() {
    vlog("Stopping server...");
    if (running) {
        running = false;
        close_all_client_sockets(); 
        if (listen_fd != -1) {
            close(listen_fd);
            listen_fd = -1;
        }
    }
    vlog("Server stopped");
}

std::string SimpleHTTPServer::host_url() const {
    std::ostringstream s;
    s << "http://" << (opts.bind_address.empty() ? get_default_bind_address() : opts.bind_address)
      << ":" << port << "/" << opts.token;
    return s.str();
}

void SimpleHTTPServer::server_loop() {
    vlog("Server loop started");
    
    struct pollfd fds[1];
    fds[0].fd = listen_fd;
    fds[0].events = POLLIN;
    
    while (running) {
        if (opts.interrupted && *opts.interrupted) {
            vlog("Interrupted flag detected, stopping server");
            break;
        }
        
        int poll_result = poll(fds, 1, 100);
        
        if (poll_result < 0) {
            if (errno == EINTR) continue;
            vlog("Poll failed");
            break;
        }
        
        if (poll_result == 0) {
            continue;
        }
        
        if (fds[0].revents & POLLIN) {
            sockaddr_in cli{};
            socklen_t clilen = sizeof(cli);
            int fd = accept(listen_fd, (sockaddr *)&cli, &clilen);
            
            if (fd < 0) {
                if (!running) break;
                continue;
            }

            set_socket_timeout(fd, opts.socket_timeout_seconds);
            
            fcntl(fd, F_SETFL, O_NONBLOCK);
            
            add_client_socket(fd);
            
            vlog("New client connected: " + get_client_ip(fd));
            if (on_log) {
                std::ostringstream ss;
                ss << "Client connected: " << inet_ntoa(cli.sin_addr) << ":" << ntohs(cli.sin_port);
                on_log(ss.str());
            }

            std::thread([this, fd]() {
                ClientHandler handler(opts, fd);
                handler.on_log = this->on_log;
                handler.on_client_done = this->on_client_done;
                handler.handle();
                
                remove_client_socket(fd);
            }).detach();
        }
    }
    vlog("Server loop ended");
}

std::string SimpleHTTPServer::get_default_bind_address() const {
    if (opts.public_access) {
        return "0.0.0.0";
    }
    return "127.0.0.1";
}
