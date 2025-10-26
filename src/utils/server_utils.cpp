#include "server_utils.h"
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

long long extract_content_length(const std::string &req) {
    auto pos = req.find("Content-Length:");
    if (pos == std::string::npos) return -1;
    pos += strlen("Content-Length:");
    while (pos < req.size() && (req[pos] == ' ' || req[pos] == '\t')) ++pos;
    size_t end = req.find("\r\n", pos);
    if (end == std::string::npos) return -1;
    std::string num = req.substr(pos, end - pos);
    try {
        return std::stoll(num);
    } catch (...) {
        return -1;
    }
}

std::string get_client_ip(int fd) {
    sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getpeername(fd, (sockaddr*)&addr, &addr_len) == 0) {
        return inet_ntoa(addr.sin_addr);
    }
    return "unknown";
}
