#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include "server.h"
#include <string>


#include <openssl/ssl.h>


class ClientHandler {
public:

    ClientHandler(const ServerOptions& opts, int fd, SSL* ssl = nullptr);

    void handle();

    std::function<void(const std::string&)> on_log;
    std::function<void()> on_client_done;

private:
    ServerOptions opts_;
    int fd_;

    SSL* ssl_;

    
    bool read_headers(std::string& headers);
    void handle_get_request(const std::string& path, const std::string& headers);
    void handle_post_request(const std::string& path, const std::string& headers);
    void send_response(const std::string& response);
    void send_error(int code, const std::string& message);
};

#endif
