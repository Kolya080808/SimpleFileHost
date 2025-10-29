#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <string>

void set_default_bind_address(const std::string &addr);

std::string get_default_bind_address();

void set_tls_enabled(bool enabled);
bool get_tls_enabled();
void set_tls_files(const std::string &cert_path, const std::string &key_path);
std::string get_tls_cert();
std::string get_tls_key();

#endif
