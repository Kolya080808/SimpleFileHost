#ifndef SIMPLEFILEHOST_SERVER_H
#define SIMPLEFILEHOST_SERVER_H

#include <string>
#include <functional>
#include <atomic>

struct ServerOptions {
    int port = 0;
    std::string token;
    std::string mode;
    std::string path;
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
    int listen_fd=-1;
    int port;
    std::atomic<bool> running{false};
    void server_loop();
};

#endif
