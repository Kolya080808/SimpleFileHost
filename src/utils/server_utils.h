#ifndef SERVER_UTILS_H
#define SERVER_UTILS_H

#include <string>

long long extract_content_length(const std::string &req);
std::string get_client_ip(int fd);

#endif
