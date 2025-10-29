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
#include <openssl/ssl.h>
#include <openssl/err.h>

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

SimpleHTTPServer::~SimpleHTTPServer() { 
    stop();
    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = nullptr;
    }
}

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

    if (get_tls_enabled()) {
        SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();

        ssl_ctx = SSL_CTX_new(TLS_server_method());
        if (!ssl_ctx) {
            vlog("Failed to create SSL_CTX");
            return false;
        }

        std::string cert = get_tls_cert();
        std::string key = get_tls_key();
        if (cert.empty() || key.empty()) {
            vlog("TLS enabled but cert/key not provided");
            SSL_CTX_free(ssl_ctx);
            ssl_ctx = nullptr;
            return false;
        }

        if (SSL_CTX_use_certificate_file(ssl_ctx, cert.c_str(), SSL_FILETYPE_PEM) <= 0) {
            vlog("Failed to load TLS certificate");
            SSL_CTX_free(ssl_ctx);
            ssl_ctx = nullptr;
            return false;
        }

        if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key.c_str(), SSL_FILETYPE_PEM) <= 0) {
            vlog("Failed to load TLS private key");
            SSL_CTX_free(ssl_ctx);
            ssl_ctx = nullptr;
            return false;
        }

        if (!SSL_CTX_check_private_key(ssl_ctx)) {
            vlog("TLS private key does not match certificate public key");
            SSL_CTX_free(ssl_ctx);
            ssl_ctx = nullptr;
            return false;
        }

        vlog("TLS initialized");
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
    bool tls = get_tls_enabled();
    s << (tls ? "https://" : "http://") << (opts.bind_address.empty() ? get_default_bind_address() : opts.bind_address)
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

            SSL* client_ssl = nullptr;
            if (ssl_ctx && get_tls_enabled()) {
                int flags = fcntl(fd, F_GETFL, 0);
                fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

                client_ssl = SSL_new(ssl_ctx);
                if (!client_ssl) {
                    vlog("Failed to create SSL object for client");
                    close(fd);
                    continue;
                }
                SSL_set_fd(client_ssl, fd);

                if (SSL_accept(client_ssl) <= 0) {
                    vlog("TLS handshake failed for incoming client");
                    SSL_shutdown(client_ssl);
                    SSL_free(client_ssl);
                    close(fd);
                    continue;
                }
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

            std::thread([this, fd, client_ssl]() {
                ClientHandler handler(this->opts, fd, client_ssl);
                handler.on_log = this->on_log;
                handler.on_client_done = this->on_client_done;
                handler.handle();
                
                remove_client_socket(fd);
            }).detach();
        }
    }
    vlog("Server loop ended");
}
