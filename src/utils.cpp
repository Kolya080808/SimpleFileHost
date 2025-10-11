#include <iostream>
#include <string>
#include <random>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <sys/stat.h>
#include <cstdio>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>
#include <net/if.h>
#include "utils.h"
#include <netdb.h>
#include <vector>
#include <unistd.h>

std::string random_token(size_t len) {
    static std::mt19937_64 rng(std::random_device{}());
    static const char alphanum[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::uniform_int_distribution<int> dist(0, (int)sizeof(alphanum) - 2);
    std::string s;
    s.reserve(len);
    for (size_t i = 0; i < len; ++i)
        s.push_back(alphanum[dist(rng)]);
    return s;
}

bool file_exists(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

std::string file_basename(const std::string &path) {
    auto p = path.find_last_of("/\\");
    if (p == std::string::npos) return path;
    return path.substr(p + 1);
}

std::string mime_type(const std::string &name) {
    if (name.find(".html") != std::string::npos) return "text/html";
    if (name.find(".txt") != std::string::npos) return "text/plain";
    if (name.find(".png") != std::string::npos) return "image/png";
    if (name.find(".jpg") != std::string::npos || name.find(".jpeg") != std::string::npos) return "image/jpeg";
    if (name.find(".zip") != std::string::npos) return "application/zip";
    return "application/octet-stream";
}

void write_file(const std::string &path, const std::string &data) {
    std::ofstream ofs(path, std::ios::binary);
    ofs.write(data.data(), data.size());
}

std::string read_file_all(const std::string &path) {
    std::ifstream ifs(path, std::ios::binary);
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

static bool is_private_ipv4(const std::string &ip){
    if(ip.rfind("10.", 0) == 0) return true;
    if(ip.rfind("192.168.", 0) == 0) return true;
    if(ip.rfind("172.", 0) == 0){
        int a,b,c,d;
        if(sscanf(ip.c_str(), "%d.%d.%d.%d", &a,&b,&c,&d) == 4){
            if(b >= 16 && b <= 31) return true;
        }
    }
    return false;
}

std::string get_local_ip() {
    struct ifaddrs *ifaddr, *ifa;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return "127.0.0.1";
    }

    std::vector<std::string> candidates;

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            if (ifa->ifa_flags & IFF_LOOPBACK) continue;
            if (!(ifa->ifa_flags & IFF_UP)) continue;

            void *addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            if (inet_ntop(AF_INET, addr, host, sizeof(host)) == nullptr) continue;
            candidates.emplace_back(host);
        }
    }

    freeifaddrs(ifaddr);

    for (const auto &ip : candidates) {
        if (is_private_ipv4(ip)) return ip;
    }

    if (!candidates.empty()) return candidates.front();

    char hn[256];
    if (gethostname(hn, sizeof(hn)) == 0) {
        struct addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(hn, nullptr, &hints, &res) == 0) {
            for (struct addrinfo *p = res; p != nullptr; p = p->ai_next) {
                char ipbuf[NI_MAXHOST];
                if (getnameinfo(p->ai_addr, p->ai_addrlen, ipbuf, sizeof(ipbuf), nullptr, 0, NI_NUMERICHOST) == 0) {
                    std::string s(ipbuf);
                    if (!s.empty() && s != "127.0.0.1") {
                        freeaddrinfo(res);
                        return s;
                    }
                }
            }
            freeaddrinfo(res);
        }
    }

    return "127.0.0.1";
}

